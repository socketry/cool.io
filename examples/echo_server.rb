$LOAD_PATH.unshift File.expand_path('../../lib', __FILE__)

require 'rubygems'
require 'cool.io'

ADDR = '127.0.0.1'
PORT = 4321

class EchoServerConnection < Cool.io::TCPSocket
  def on_connect
    puts "#{remote_addr}:#{remote_port} connected"
  end

  def on_close
    puts "#{remote_addr}:#{remote_port} disconnected"
  end

  def on_read(data)
    write data
  end
end

event_loop = Cool.io::Loop.default
Cool.io::TCPServer.new(ADDR, PORT, EchoServerConnection).attach(event_loop)

puts "Echo server listening on #{ADDR}:#{PORT}"
event_loop.run
