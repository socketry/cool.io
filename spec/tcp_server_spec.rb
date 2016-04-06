require File.expand_path('../spec_helper', __FILE__)

TIMEOUT = 0.010
HOST = '127.0.0.1'
PORT = unused_port

def send_data(data)
  io = TCPSocket.new('127.0.0.1', PORT)
  begin
    io.write data
  ensure
    io.close
  end
end

class MyConnection < Coolio::Socket
  attr_accessor :data, :connected, :closed

  def initialize(io, on_message)
    super(io)
    @on_message = on_message
  end

  def on_connect
    @connected = true
  end

  def on_close
    @closed = true
  end

  def on_read(data)
    @on_message.call(data)
  end
end

@data = ""
def on_message(data)
  @data = data
end

def test_run(data = nil)
  reactor = Coolio::Loop.new
  server = Cool.io::TCPServer.new(HOST, PORT, MyConnection, method(:on_message))
  reactor.attach(server)
  thread = Thread.new { reactor.run }
  send_data(data) if data
  sleep TIMEOUT
  reactor.stop
  server.detach
  send_data('') # to leave from blocking loop
  thread.join
  @data
ensure
  server.close
end

def test_run_once(data = nil)
  reactor = Coolio::Loop.new
  server = Cool.io::TCPServer.new(HOST, PORT, MyConnection, method(:on_message))
  reactor.attach(server)
  thread = Thread.new do
    reactor.run_once # on_connect
    reactor.run_once # on_read
  end
  send_data(data) if data
  thread.join
  server.detach
  @data
ensure
  server.close
end

def test_run_once_timeout(timeout = TIMEOUT)
  @data = ""
  reactor = Coolio::Loop.new
  server = Cool.io::TCPServer.new(HOST, PORT, MyConnection, method(:on_message))
  reactor.attach(server)
  thread = Thread.new { reactor.run_once(timeout) }
  sleep timeout
  server.detach
  thread.join
  @data
ensure
  server.close
end

def test_run_timeout(data = nil, timeout = TIMEOUT)
  reactor = Coolio::Loop.new
  server = Cool.io::TCPServer.new(HOST, PORT, MyConnection, method(:on_message))
  reactor.attach(server)
  running = true
  thread = Thread.new do
    while running and reactor.has_active_watchers?
      reactor.run_once(timeout)
    end
  end
  send_data(data) if data
  sleep timeout
  server.detach
  running = false # another send is not required
  thread.join
  @data
ensure
  server.close
end

# This test should work on Windows
describe Coolio::TCPServer do

  it '#run' do
    expect(test_run("hello")).to eq("hello")
  end

  it '#run_once' do
    expect(test_run_once("hello")).to eq("hello")
  end

  it '#run_once(timeout)' do
    test_run_once_timeout # should not block
  end

  it '#run_once(-timeout)' do
    expect { test_run_once_timeout(-0.1) }.to raise_error(ArgumentError)
  end

  it '#run(timeout)' do
    expect(test_run_timeout("hello")).to eq("hello")
  end

  describe "functionaltest" do
    let :loop do
      Coolio::Loop.new
    end
    
    let :port do
      unused_port
    end
    
    context "#on_connect" do
      class ServerOnConnect < Coolio::Socket
        def initialize(io, cb)
          super(io)
          @cb = cb
        end
        def on_connect
          @cb.call
        end
      end
      
      it "connected socket called on_connect" do
        begin
          connected = false
          server = Cool.io::TCPServer.new("localhost", port, ServerOnConnect, proc { connected = true })
          loop.attach server
          s = TCPSocket.open("localhost", port)
          loop.run_once
          s.close
          expect(connected).to eq true
        ensure
          server.detach
        end
      end
    end
    
    context "#on_close" do
      class ServerOnClose < Coolio::Socket
        def initialize(io, cb)
          super(io)
          @cb = cb
        end
        def on_close
          @cb.call
        end
      end
      
      it "closed socket called on_close" do
        begin
          closed = false
          server = Cool.io::TCPServer.new("localhost", port, ServerOnConnect, proc { closed = true })
          loop.attach server
          s = TCPSocket.open("localhost", port)
          loop.run_once
          s.close
          loop.run_once
          expect(closed).to eq true
        ensure
          server.detach
        end
      end
    end
    
    context "#on_read" do
      class Echo < Coolio::Socket
        def initialize(io, cb)
          super(io)
          @cb = cb
        end
        def on_read(data)
          @cb.call data
          _size = write(data + "fff")
        end
      end
      
      it "server socket received data" do
        begin
          data = "aaa"
          server = Cool.io::TCPServer.new("localhost", port, Echo, proc { |d| data = d })
          loop.attach server
          thread = Thread.new { loop.run }
          s = TCPSocket.open("localhost", port)
          s.write "zzz"
          sleep 0.1
          expect(data).to eq "zzz"
          expect(s.read 6).to eq "zzzfff"
        ensure
          s.close
          loop.stop
          server.detach
          thread.join
        end
      end
    end
  end
end
