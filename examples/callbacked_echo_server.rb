$LOAD_PATH.unshift File.expand_path('../../lib', __FILE__)

require 'rubygems'
require 'cool.io'

ADDR = '127.0.0.1'
PORT = 4321

event_loop = Cool.io::Loop.default
server = Cool.io::TCPServer.new(ADDR, PORT) do |connection|
  puts "#{connection.remote_addr}:#{connection.remote_port} connected"

  connection.on_close do
    puts "#{connection.remote_addr}:#{connection.remote_port} disconnected"
  end

  connection.on_read do |data|
    connection.write data
  end
end
server.attach(event_loop)

puts "Echo server listening on #{ADDR}:#{PORT}"
event_loop.run
