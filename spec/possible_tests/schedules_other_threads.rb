require File.dirname(__FILE__) + '/../lib/rev'
def dbg
 require 'rubygems'
 require 'ruby-debug'
 debugger
end
Thread.abort_on_exception=true

require 'socket'

describe Rev::TCPSocket do
      HOST = '127.0.0.1'
      PORT = 4321
  before :each do
      @server = Rev::TCPServer.new(HOST, PORT) do |c|
         c.on_connect { puts "#{remote_addr}:#{remote_port} connected" }
         c.on_close   { puts "#{remote_addr}:#{remote_port} disconnected" }
         #c.on_read    { |data| write data }
      end

  end

  after :each do
	@server.close
  end

  def sleep_until(seconds = 1, interval = 0.1)
   raise unless block_given?
   start_time=Time.now
   sleep interval until ((Time.now - start_time) > seconds) or yield
  end

  it "should stop" do
	loop = Rev::Loop.default
	stopped = false;
  	@server.attach(loop)
  	Thread.new {
                loop.run
		stopped = true
        }
	sleep 0
        stopped.should == false
        loop.stop
        sleep_until(3) { stopped == true }
        stopped.should == true
   end
end

