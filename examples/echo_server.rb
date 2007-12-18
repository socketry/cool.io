require File.dirname(__FILE__) + '/../lib/rev'

PORT = 4321

class EchoServerConnection < Rev::TCPSocket
  def initialize(socket)
    super

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
Rev::TCPServer.new('localhost', PORT).attach(event_loop, EchoServerConnection)

puts "Echo server listening on port #{PORT}"
event_loop.run
