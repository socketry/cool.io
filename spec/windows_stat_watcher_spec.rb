require File.expand_path('../spec_helper', __FILE__)
require 'tmpdir'
require 'fileutils'
require 'rbconfig'

describe 'Windows notification-backed StatWatcher', :if => RUBY_PLATFORM =~ /mingw|mswin/ do
  before do
    @directory = Dir.mktmpdir('coolio-notify-')
    @loop = Coolio::Loop.new
    @watchers = []
    @threads = []
  end

  after do
    @threads.each(&:join)
    @watchers.each { |watcher| watcher.detach if watcher.attached? }
    FileUtils.remove_entry(@directory)
    @loop = nil
    @watchers = nil
    GC.start # exercise IOCP cancellation and loop destruction
  end

  def watch(path, interval = 60)
    watcher = Coolio::StatWatcher.new(path, interval)
    watcher.attach(@loop)
    @watchers << watcher
    watcher
  end

  # With a 60-second stat interval, a callback within this deadline must come
  # from native notifications. Mutate from another thread while select waits.
  def await_change(watcher, size: nil, exists: nil, &mutation)
    change = nil
    first_previous = nil
    reactor = @loop
    callback_thread = nil
    watcher.define_singleton_method(:on_change) do |previous, current|
      first_previous ||= previous
      if (size.nil? || current.size == size) && (exists.nil? || (current.nlink != 0) == exists)
        change = [first_previous, current]
        callback_thread = Thread.current
        reactor.stop
      end
    end
    deadline = Coolio::TimerWatcher.new(2, false)
    deadline.on_timer { @loop.stop }
    deadline.attach(@loop)
    @threads << Thread.new { sleep 0.1; mutation.call }
    @loop.run
    expect(change).not_to be_nil
    expect(callback_thread).to eq(Thread.current)
    change
  ensure
    deadline.detach if deadline && deadline.attached?
    @threads.each(&:join)
  end

  it 'wakes a blocked loop for a file size change without waiting for polling' do
    path = File.join(@directory, 'file')
    File.write(path, 'a')
    previous, current = await_change(watch(path)) { File.write(path, 'longer') }
    expect(previous.size).to eq(1)
    expect(current.size).to eq(6)
  end

  it 'notifies for creation, deletion, and recreation of a watched file' do
    path = File.join(@directory, 'file')
    watcher = watch(path)
    previous, current = await_change(watcher, size: 7, exists: true) { File.write(path, 'created') }
    expect(previous.nlink).to eq(0)
    expect(current.nlink).not_to eq(0)
    _, current = await_change(watcher, exists: false) { File.delete(path) }
    expect(current.nlink).to eq(0)
    _, current = await_change(watcher, size: 9, exists: true) { File.write(path, 'recreated') }
    expect(current.size).to eq(9)
  end

  it 'notifies when a watched file is renamed away' do
    path = File.join(@directory, 'file')
    File.write(path, 'a')
    _, current = await_change(watch(path)) { File.rename(path, "#{path}.old") }
    expect(current.nlink).to eq(0)
  end

  it 'supports Unicode parent directories and filenames' do
    directory = File.join(@directory, "\u76e3\u8996")
    Dir.mkdir(directory)
    path = File.join(directory, "\u5909\u66f4.txt")
    File.write(path, 'a')
    _, current = await_change(watch(path)) { File.write(path, 'longer') }
    expect(current.size).to eq(6)
  end

  it 'matches notification names using Windows Unicode case folding' do
    path = File.join(@directory, "\u00c4.txt")
    File.write(path, 'a')
    folded = File.join(@directory, "\u00e4.txt")
    skip 'requires a case-insensitive directory' unless File.exist?(folded)
    watcher = watch(folded)
    _, current = await_change(watcher, size: 6) { File.write(path, 'longer') }
    expect(current.size).to eq(6)
  end

  it 'continues monitoring a shared directory after another watcher detaches' do
    first = File.join(@directory, 'first')
    second = File.join(@directory, 'second')
    File.write(first, 'a')
    File.write(second, 'a')
    first_watcher = watch(first)
    second_watcher = watch(second)
    first_watcher.detach
    _, current = await_change(second_watcher) { File.write(second, 'longer') }
    expect(current.size).to eq(6)
  end

  it 'transcodes a Windows-31J path for the wide Windows APIs' do
    path = File.join(@directory, "\u5909\u66f4.txt")
    File.write(path, 'a')
    watcher = watch(path.encode('Windows-31J'))
    _, current = await_change(watcher) { File.write(path, 'longer') }
    expect(current.size).to eq(6)
  end

  it 'transfers notification monitoring to another loop' do
    path = File.join(@directory, 'file')
    File.write(path, 'a')
    watcher = watch(path)
    old_loop = @loop
    @loop = Coolio::Loop.new
    watcher.attach(@loop)
    old_loop.run_nonblock
    _, current = await_change(watcher) { File.write(path, 'longer') }
    expect(current.size).to eq(6)
  end

  it 'does not dispatch a queued change after disable and supports re-enable' do
    path = File.join(@directory, 'file')
    File.write(path, 'a')
    watcher = watch(path)
    calls = 0
    watcher.define_singleton_method(:on_change) { |_, _| calls += 1 }
    File.write(path, 'queued')
    sleep 0.05
    watcher.disable
    @loop.run_nonblock
    expect(calls).to eq(0)
    watcher.enable
    _, current = await_change(watcher) { File.write(path, 'after enable') }
    expect(current.size).to eq(12)
  end

  it 'falls back to polling when the parent is missing, then registers notifications' do
    directory = File.join(@directory, 'missing')
    path = File.join(directory, 'file')
    watcher = watch(path, 0.1)
    _, current = await_change(watcher, size: 7, exists: true) { Dir.mkdir(directory); File.write(path, 'created') }
    expect(current.size).to eq(7)
    # Reattach with a long interval to prove the now-existing parent is watched.
    watcher.detach
    _, current = await_change(watch(path)) { File.write(path, 'longer than before') }
    expect(current.size).to eq(18)
  end

  it 'handles directory entry changes' do
    directory = File.join(@directory, 'directory')
    Dir.mkdir(directory)
    watcher = watch(directory)
    # StatInfo exposes whole seconds for timestamps; cross a second boundary
    # so this directory-only change is observable through the existing API.
    sleep 1.1
    await_change(watcher) { File.write(File.join(directory, 'child'), 'a') }
  end

  it 'recovers after a parent directory is moved and replaced' do
    directory = File.join(@directory, 'parent')
    Dir.mkdir(directory)
    path = File.join(directory, 'file')
    File.write(path, 'a')
    watcher = watch(path, 0.1)
    _, current = await_change(watcher, size: 11, exists: true) do
      File.rename(directory, "#{directory}.old")
      Dir.mkdir(directory)
      File.write(path, 'replacement')
    end
    expect(current.size).to eq(11)
    _, current = await_change(watcher, size: 5) { File.write(path, 'final') }
    expect(current.size).to eq(5)
  end

  it 'coalesces a burst of notifications and observes the final state' do
    path = File.join(@directory, 'file')
    File.write(path, 'a')
    watcher = watch(path)
    200.times do |index|
      File.write(path, 'x' * (index + 2))
    end
    _, current = await_change(watcher, size: 5) { File.write(path, 'final') }
    expect(current.size).to eq(5)
  end

  it 'validates a shared directory handle before attaching a new watcher' do
    directory = File.join(@directory, 'parent')
    Dir.mkdir(directory)
    first = File.join(directory, 'first')
    File.write(first, 'a')
    watch(first)

    File.rename(directory, "#{directory}.old")
    Dir.mkdir(directory)
    second = File.join(directory, 'second')
    File.write(second, 'a')
    _, current = await_change(watch(second), size: 5) { File.write(second, 'final') }
    expect(current.size).to eq(5)
  end

  it 'keeps timers and Ruby threads responsive during sustained notifications' do
    observed_size = nil
    paths = 1000.times.map do |index|
      path = File.join(@directory, "#{index}.log")
      File.write(path, 'a')
      watch(path).define_singleton_method(:on_change) do |_, current|
        observed_size = current.size if index.zero?
      end
      path
    end
    clock = -> { Process.clock_gettime(Process::CLOCK_MONOTONIC) }
    timer_gaps = []
    thread_gaps = []
    previous = clock.call
    timer = Coolio::TimerWatcher.new(0.1, true)
    timer.on_timer do
      now = clock.call
      timer_gaps << now - previous
      previous = now
    end
    timer.attach(@loop)
    @watchers << timer
    done = false
    @threads << Thread.new do
      last = clock.call
      until done
        sleep 0.05
        now = clock.call
        thread_gaps << now - last
        last = now
      end
    end
    # Use a separate process: a Ruby writer thread would itself be blocked by
    # the GVL while the notification callback is draining the dirty set.
    writer = <<~'RUBY'
      path = ARGV.fetch(0)
      sleep 0.3
      clock = -> { Process.clock_gettime(Process::CLOCK_MONOTONIC) }
      start = clock.call
      writes = 0
      while clock.call - start < 3
        File.write(path, 'x', mode: 'a')
        writes += 1
        sleep [start + writes.fdiv(64) - clock.call, 0].max
      end
    RUBY
    child = Process.spawn(RbConfig.ruby, '-e', writer, paths.first)
    deadline = Coolio::TimerWatcher.new(4, false)
    deadline.on_timer { @loop.stop }
    deadline.attach(@loop)
    @watchers << deadline
    @loop.run
    _, status = Process.wait2(child)
    child = nil
    expect(status.success?).to eq(true)
    expect(timer_gaps.max).to be < 2
    expect(thread_gaps.max).to be < 2
    expect(File.size(paths.first)).to be > 1
    expect(observed_size).to eq(File.size(paths.first))
  ensure
    done = true
    Process.wait(child) if child
  end

  it 'can repeatedly detach and reattach while changes are pending' do
    path = File.join(@directory, 'file')
    File.write(path, 'a')
    watcher = watch(path)
    50.times do |index|
      File.write(path, 'x' * (index + 2))
      watcher.detach
      watcher.attach(@loop)
    end
    _, current = await_change(watcher) { File.write(path, 'final') }
    expect(current.size).to eq(5)
  end
end
