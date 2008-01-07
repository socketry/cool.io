require File.dirname(__FILE__) + '/../lib/rev'

ADDR = '127.0.0.1'
PORT = 4321

class EchoServerConnection < Rev::TCPSocket
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

event_loop = Rev::Loop.default
Rev::TCPServer.new(ADDR, PORT, EchoServerConnection).attach(event_loop)

puts "Echo server listening on #{ADDR}:#{PORT}"
event_loop.run
