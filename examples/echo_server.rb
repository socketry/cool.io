require File.dirname(__FILE__) + '/../lib/rev'

PORT = 4321

class EchoServerConnection < Rev::BufferedIO
  def initialize(socket)
    super

    @port, @addr = Socket.unpack_sockaddr_in(socket.getpeername)
    puts "Received connection from #{@addr}:#{@port}"
  end

  def on_close
    puts "Connection closed from #{@addr}:#{@port}"
  end

  def on_read(data)
    write data
  end
end

event_loop = Rev::Loop.new
Rev::TCPServer.new('localhost', PORT).attach(event_loop, EchoServerConnection)

puts "Echo server listening on port #{PORT}"
event_loop.run
