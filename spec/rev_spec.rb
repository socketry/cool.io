#
# Oops, were you expecting some real RSpec?
#
# I'm sorry to disappoint.  Really, this is pretty damn weak.
#
# I swear that after Ruby 1.9 comes out proper, this will become an actual spec
#

require 'socket'
require File.dirname(__FILE__) + '/../lib/rev'

class MyWatcher < Rev::IOWatcher
  def on_readable
    puts "Yippie, I'm readable!"
    detach
  end
end

listen_socket = TCPServer.new 'localhost', 4321
watcher = MyWatcher.new(listen_socket)
event_loop = Rev::Loop.new
watcher.attach(event_loop)

puts "Running the event loop in blocking mode on localhost port 4321"
puts "Telnet there or something and you should get a read event"
event_loop.run
