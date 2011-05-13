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
                loop.run_nonblock_over_and_over_again
		stopped = true
        }
	sleep 0
        stopped.should == false
        loop.stop

        sleep_until { stopped == true }
        stopped.should == true
  end

  it "should auto bind on 1.8.6" do
      @server.close
      loop = Rev::Loop.default
      server = Rev::TCPServer.new(HOST, PORT) do |c|
        #c.on_connect { puts "#{remote_addr}:#{remote_port} connected" }
        #c.on_close   { puts "#{remote_addr}:#{remote_port} disconnected" }
	print "CREATING\n"
        c.on_read    { |conn, data| print "WITHIN MYINE"; conn.write data; conn.close; loop.stop }
      end

      server.attach(loop)

      Thread.new { loop.run_nonblock_over_and_over_again }
      puts "Echo server listening on #{HOST}:#{PORT}"
      # now connect and write -- it should close
      connector = TCPSocket.new HOST, PORT
      connector.write "yup"
      connector.read.should == 'yup'
      sleep 0
      loop.running?.should != true
      connector.closed?.should == true # it should close me, too
  end
end
