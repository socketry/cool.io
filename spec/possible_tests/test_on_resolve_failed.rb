require "rev"
class Tester < Rev::TCPSocket
  def on_resolve_failed
    print "resolved failed! that's good!"
  end
end
a = Tester.connect('asdfgoogle.com', 80)
a.attach Rev::Loop.default
Rev::Loop.default.run
