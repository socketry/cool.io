require File.dirname(__FILE__) + '/../lib/rev'

ADDR = '127.0.0.1'
PORT = 4321

class EchoServerConnection < Rev::TCPSocket
  def on_connect
    puts "Received connection from #{remote_addr}:#{remote_port}"
  end

  def on_close
    puts "Connection closed from #{remote_addr}:#{remote_port}"
  end

  def on_read(data)
    write data
  end
end

event_loop = Rev::Loop.new
Rev::TCPServer.new(ADDR, PORT, EchoServerConnection).attach(event_loop)

puts "Echo server listening on #{ADDR}:#{PORT}"
event_loop.run
