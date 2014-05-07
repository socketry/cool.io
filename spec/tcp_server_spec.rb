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
  reactor = Coolio::Loop.new
  server = Cool.io::TCPServer.new(HOST, PORT, MyConnection, method(:on_message))
  reactor.attach(server)
  running = true
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
    test_run("hello").should == "hello"
  end

  it '#run_once' do
    test_run_once("hello").should == "hello"
  end

  it '#run_once(timeout)' do
    test_run_once_timeout # should not block
  end

  it '#run_once(-timeout)' do
    expect { test_run_once_timeout(-0.1) }.to raise_error(ArgumentError)
  end

  it '#run(timeout)' do
    test_run_timeout("hello").should == "hello"
  end

end

