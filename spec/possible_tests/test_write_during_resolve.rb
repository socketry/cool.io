require "rev"

ADDR = 'wilkboardonline.com'
PORT = 80

class ClientConnection < Rev::TCPSocket
  def on_connect
    puts "#{remote_addr}:#{remote_port} connected"
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
client.write ("GET / HTTP/1.1\r\nhost:wilkboardonline.com:80\r\nconnection:close\r\n\r\n")
client.attach(event_loop)
puts "Echo client started to #{ADDR}:#{PORT}"
event_loop.run
