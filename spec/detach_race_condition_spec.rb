require 'spec_helper'

describe Cool.io::Loop do
  # An IOWatcher that drains its pipe and then runs a user-supplied block,
  # receiving itself as the argument.
  class Victim < Cool.io::IOWatcher
    def initialize(io, &on_readable)
      super(io)
      @io = io
      @on_readable = on_readable
    end

    def on_readable
      begin
        @io.read_nonblock(1024)
      rescue IO::WaitReadable, EOFError
      end
      @on_readable.call(self) if @on_readable
    end
  end

  # https://github.com/socketry/cool.io/issues/87
  #
  # Several watchers have an event pending in the same loop iteration.  When the
  # first one dispatched detaches the others, the loop must skip their now-stale
  # pending events instead of dispatching them to a detached watcher.  Before the
  # fix that raised "TypeError: wrong argument type nil (expected Coolio::Loop)"
  # and could crash the VM.
  #
  # This is exercised deterministically within a single thread (a preceding
  # callback detaching another watcher in the same loop cycle).  The original
  # reproduction detached from a separate thread while the loop was polling,
  # which is an unsupported concurrent mutation of libev (not thread-safe) and
  # crashed intermittently on macOS.
  it "does not raise when a watcher with a pending event is detached during dispatch" do
    iterations = 200

    expect {
      iterations.times do
        coolio_loop = Cool.io::Loop.new
        pipes = []
        watchers = []

        5.times do
          r, w = IO.pipe
          pipes << [r, w]

          watcher = Victim.new(r) do |fired|
            # Detach every other watcher whose event is already queued for this
            # same loop iteration.
            watchers.each do |other|
              other.detach if !other.equal?(fired) && other.attached?
            end
          end
          watcher.attach(coolio_loop)
          watchers << watcher

          w.write("dummy\n") # make the read end readable so an event is pending
        end

        coolio_loop.run_once

        # Only the first dispatched watcher runs; it detaches the other four,
        # whose pending events are then skipped.
        expect(watchers.count(&:attached?)).to eq(1)

        watchers.each { |watcher| watcher.detach if watcher.attached? }
        pipes.each { |r, w| r.close; w.close }
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
