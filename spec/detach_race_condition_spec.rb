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

  class HttpHandler < Coolio::IO
    RESPONSE = "HTTP/1.1 200 OK\r\nContent-Length: 1024\r\nConnection: close\r\n\r\n" + ("X" * 1024)

    def on_connect
    end

    def on_read(data)
      write(RESPONSE)
    end

    def on_write_complete
      close
    end
  end

  # https://github.com/socketry/cool.io/issues/89
  it "does not cause memory leaks" do
    port = 18989
    loop = Coolio::Loop.default

    server = Coolio::TCPServer.new('127.0.0.1', port, HttpHandler)
    server.attach(loop)

    event_thread = Thread.new { loop.run }

    request = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"

    10.times do |iteration|
      begin
        sock = TCPSocket.new('127.0.0.1', port)
        sock.write(request)
        sock.read
        sock.close
      rescue => e
        sleep 0.01
        retry
      end
    end

    server.close
    event_thread.join

    expect(loop.watchers).to be_empty
  end
end
