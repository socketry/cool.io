$LOAD_PATH.unshift File.expand_path('../../lib', __FILE__)

require 'rubygems'
require 'cool.io'

ADDR = '127.0.0.1'
PORT = 4321

cool.io.connection :echo_server do
  on_connect do
    puts "#{remote_addr}:#{remote_port} connected"
  end

  on_close do
    puts "#{remote_addr}:#{remote_port} disconnected"
  end

  on_read do |data|
    write data
  end
end

server = cool.io.server ADDR, PORT, :echo_server
cool.io.attach server

puts "Echo server listening on #{ADDR}:#{PORT}"
cool.io.run