require 'spec_helper'

describe Cool.io::Loop do
  class Victim < Cool.io::IOWatcher
    def initialize(io)
      super
      @io = io
    end

    def on_readable
      begin
        @io.read_nonblock(1024)
      rescue IO::WaitReadable, EOFError
      end
    end
  end

  # https://github.com/socketry/cool.io/issues/87
  it "does not raise TypeError when a watcher is detached while an event is pending" do
    loop = Cool.io::Loop.default

    iterations = 200

    expect {
      iterations.times do
        r_victim, w_victim = IO.pipe
        victim_watcher = Victim.new(r_victim)
        victim_watcher.attach(loop)

        t1 = Thread.new do
          sleep 0.01
          w_victim.write("dummy\n")
        end

        t2 = Thread.new do
          sleep 0.01
          victim_watcher.detach
        end

        loop.run_once

        t1.join
        t2.join

        r_victim.close
        w_victim.close
      end
    }.not_to raise_error
  end
end
