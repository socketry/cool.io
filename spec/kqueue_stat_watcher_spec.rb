require File.expand_path('../spec_helper', __FILE__)
require 'tmpdir'
require 'fileutils'
require 'rbconfig'
require 'timeout'

describe 'kqueue notification-backed StatWatcher', :if => RUBY_PLATFORM =~ /darwin/ do
  before do
    @directory = Dir.mktmpdir('coolio-kq-notify-')
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
    GC.start # exercise cleanup of unreachable loops and watchers
  end

  # Most tests use a 60-second interval so native notifications must deliver
  # changes before their deadlines. Recovery tests can override the interval.
  def watch(path, interval = 60)
    watcher = Coolio::StatWatcher.new(path, interval)
    watcher.attach(@loop)
    @watchers << watcher
    watcher
  end

  def await_change(watcher, size: nil, exists: nil, timeout: 2, &mutation)
    change = nil
    first_previous = nil
    reactor = @loop
    stopped = false
    stop = -> do
      unless stopped
        stopped = true
        reactor.stop
      end
    end
    watcher.define_singleton_method(:on_change) do |previous, current|
      first_previous ||= previous
      if (size.nil? || current.size == size) && (exists.nil? || (current.nlink != 0) == exists)
        change = [first_previous, current]
        stop.call
      end
    end
    deadline = Coolio::TimerWatcher.new(timeout, false)
    deadline.on_timer { stop.call }
    deadline.attach(@loop)
    @threads << Thread.new { sleep 0.1; mutation.call }
    @loop.run
    expect(change).not_to be_nil
    change
  ensure
    deadline.detach if deadline && deadline.attached?
    @threads.each(&:join)
  end

  def in_child
    pid = fork do
      begin
        yield
        exit! 0
      rescue Exception => error
        warn error.full_message
        exit! 1
      end
    end
    begin
      _, status = Timeout.timeout(10) { Process.wait2(pid) }
    rescue Timeout::Error
      Process.kill('KILL', pid)
      Process.wait(pid)
      raise 'forked watcher test timed out'
    end
    expect(status.success?).to eq(true)
  end

  it 'wakes a blocked loop for a file size change without waiting for polling' do
    path = File.join(@directory, 'file')
    File.write(path, 'a')
    previous, current = await_change(watch(path), size: 6) { File.write(path, 'longer') }
    expect(previous.size).to eq(1)
    expect(current.size).to eq(6)
  end

  it 'notifies for a truncation' do
    path = File.join(@directory, 'file')
    File.write(path, 'x' * 40)
    await_change(watch(path), size: 0) { File.truncate(path, 0) }
  end

  it 'notifies for creation, deletion, and recreation of a watched file' do
    directory = File.join(@directory, 'missing')
    path = File.join(directory, 'file')
    watcher = watch(path)
    previous, current = await_change(watcher, size: 7, exists: true) do
      Dir.mkdir(directory)
      File.write(path, 'created')
    end
    expect(previous.nlink).to eq(0)
    expect(current.nlink).not_to eq(0)
    _, current = await_change(watcher, exists: false) { File.delete(path) }
    expect(current.nlink).to eq(0)
    _, current = await_change(watcher, size: 9, exists: true) { File.write(path, 'recreated') }
    expect(current.size).to eq(9)
  end

  it 'notifies when a watched file is renamed away (log rotation)' do
    path = File.join(@directory, 'file')
    File.write(path, 'a')
    _, current = await_change(watch(path)) { File.rename(path, "#{path}.old") }
    expect(current.nlink).to eq(0)
  end

  it 'notifies when a watched file is replaced under the same name' do
    path = File.join(@directory, 'file')
    File.write(path, 'a')
    watcher = watch(path)
    _, current = await_change(watcher, size: 6, exists: true) do
      File.rename(path, "#{path}.old")
      File.write(path, 'longer')
    end
    expect(current.size).to eq(6)
  end

  it 'notifies when a child is added to a watched directory' do
    directory = File.join(@directory, 'sub')
    Dir.mkdir(directory)
    await_change(watch(directory)) { File.write(File.join(directory, 'child'), 'a') }
  end

  it 'notifies for a metadata-only change' do
    path = File.join(@directory, 'file')
    File.write(path, 'a')
    mode = (File.stat(path).mode & 0o777) ^ 0o100
    await_change(watch(path)) { File.chmod(mode, path) }
  end

  it 're-registers when the watched directory itself is recreated' do
    directory = File.join(@directory, 'sub')
    Dir.mkdir(directory)
    watcher = watch(directory)
    await_change(watcher, exists: false) { FileUtils.remove_entry(directory) }
    await_change(watcher, exists: true) { Dir.mkdir(directory) }
    await_change(watcher, timeout: 5) { File.write(File.join(directory, 'child'), 'a' * 6) }
  end

  it 're-registers when a parent of the watched file is recreated under the same name' do
    sub = File.join(@directory, 'sub')
    Dir.mkdir(sub)
    path = File.join(sub, 'file')
    File.write(path, 'a')
    watcher = watch(path)
    await_change(watcher, exists: false) { FileUtils.remove_entry(sub) }
    await_change(watcher, size: 6, exists: true, timeout: 5) do
      Dir.mkdir(sub)
      File.write(path, 'longer')
    end
  end

  it 'transfers notification monitoring to another loop' do
    path = File.join(@directory, 'file')
    File.write(path, 'a')
    watcher = watch(path)
    old_loop = @loop
    @loop = Coolio::Loop.new
    watcher.attach(@loop)
    old_loop.run_nonblock
    _, current = await_change(watcher, size: 6) { File.write(path, 'longer') }
    expect(current.size).to eq(6)
  end

  it 'recovers when an ancestor is renamed and the original path is recreated' do
    sub = File.join(@directory, 'sub')
    Dir.mkdir(sub)
    path = File.join(sub, 'file')
    File.write(path, 'a')
    watcher = watch(path, 0)

    await_change(watcher, size: 6, exists: true, timeout: 8) do
      File.rename(sub, "#{sub}.old")
      Dir.mkdir(sub)
      File.write(path, 'longer')
    end
    # The polling backstop must also transfer native monitoring to the new file.
    await_change(watcher, size: 10) { File.write(path, 'evenlonger') }
  end

  [false, true].each do |missing|
    it "does not inherit #{missing ? 'parent hint' : 'file'} descriptors across exec" do
      path = File.join(@directory, 'file')
      File.write(path, 'a') unless missing
      target = missing ? @directory : path
      watch(path)

      script = <<~'RUBY'
        target = File.stat(ARGV.fetch(0))
        leaked = Dir.children('/dev/fd').any? do |fd|
          begin
            stat = IO.for_fd(Integer(fd), autoclose: false).stat
            stat.dev == target.dev && stat.ino == target.ino
          rescue Errno::EBADF, ArgumentError
            false
          end
        end
        puts leaked
      RUBY
      inherited = IO.popen([RbConfig.ruby, '-e', script, target], close_others: false, &:read)
      expect(inherited.strip).to eq('false')
    end
  end

  it 'keeps monitoring when another watcher on the same path detaches' do
    path = File.join(@directory, 'file')
    File.write(path, 'a')
    first = watch(path)
    second = watch(path)
    await_change(first, size: 6) { File.write(path, 'b' * 6) }
    first.detach
    await_change(second, size: 7) { File.write(path, 'c' * 7) }
  end

  it 'notifies every watcher of many watched files within one burst' do
    # Exceed the native event batch size without requiring hundreds of fds.
    paths = (1..64).map do |i|
      path = File.join(@directory, "file#{i}")
      File.write(path, 'a')
      watch(path)
    end
    notified = 0
    reactor = @loop
    paths.each do |watcher|
      watcher.define_singleton_method(:on_change) do |_previous, current|
        if current.size == 2
          notified += 1
          reactor.stop if notified == paths.size
        end
      end
    end
    deadline = Coolio::TimerWatcher.new(5, false)
    deadline.on_timer { @loop.stop }
    deadline.attach(@loop)
    paths.each { |watcher| File.open(watcher.path, 'a') { |file| file.write 'b' } }
    @loop.run
    deadline.detach if deadline.attached?
    expect(notified).to eq(paths.size)
  end

  it 're-registers inherited watchers after fork without changing the parent loop' do
    @loop = Coolio::Loop.new(fork_check: true)
    path = File.join(@directory, 'file')
    File.write(path, 'a')
    watcher = watch(path)

    in_child do
      await_change(watcher, size: 6) { File.write(path, 'longer') }
      watcher.detach
    end
    await_change(watcher, size: 10) { File.write(path, 'evenlonger') }
  end

end
