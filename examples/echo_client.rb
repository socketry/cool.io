require File.dirname(__FILE__) + '/../lib/rev'

ADDR = '127.0.0.1'
PORT = 4321

class ClientConnection < Rev::TCPSocket
  def on_connect
    puts "#{remote_addr}:#{remote_port} connected"
    write "bounce this back to me"
  end

  def on_close
    puts "#{remote_addr}:#{remote_port} disconnected"
  end

  def on_read(data)
    print "got #{data}"
    close
  end

end

event_loop = Rev::Loop.default
client = ClientConnection.connect(ADDR, PORT)
client.attach(event_loop)
puts "Echo client started to #{ADDR}:#{PORT}"
event_loop.run
