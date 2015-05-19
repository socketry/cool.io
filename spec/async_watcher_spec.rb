require File.expand_path('../spec_helper', __FILE__)
require 'tempfile'
require 'fcntl'

describe Cool.io::AsyncWatcher, :env => :exclude_win do

  it "does not signal on spurious wakeups" do
    aw = Cool.io::AsyncWatcher.new
    tmp = Tempfile.new('coolio_async_watcher_test')
    nr_fork = 2 # must be at least two for spurious wakeups

    # We have aetter chance of failing if this overflows the pipe buffer
    # which POSIX requires >= 512 bytes, Linux 2.6 uses 4096 bytes
    nr_signal = 4096 * 4

    append = File.open(tmp.path, "ab")
    append.sync = true
    rd, wr = ::IO.pipe

    aw.on_signal { append.syswrite("#$$\n") }
    children = nr_fork.times.map do
      fork do
        trap(:TERM) { exit!(0) }
        rloop = Cool.io::Loop.default
        aw.attach(rloop)
        wr.write '.' # signal to master that we're ready
        rloop.run
        exit!(1) # should not get here
      end
    end

    # ensure children are ready
    nr_fork.times { expect(rd.sysread(1)).to eq('.') }

    # send our signals
    nr_signal.times { aw.signal }

    # wait for the pipe buffer to be consumed by the children
    sleep 1 while tmp.stat.ctime >= (Time.now - 4)

    children.each do |pid|
      Process.kill(:TERM, pid)
      _, status = Process.waitpid2(pid)
      expect(status.exitstatus).to eq(0)
    end

    # we should've written a line for every signal we sent
    lines = tmp.readlines
    expect(lines.size).to eq(nr_signal)

    # theoretically a bad kernel scheduler could give us fewer...
    expect(lines.sort.uniq.size).to eq(nr_fork)

    tmp.close!
  end

end
