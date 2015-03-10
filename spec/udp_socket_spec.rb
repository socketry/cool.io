require File.expand_path('../spec_helper', __FILE__)

describe "Coolio::UDPSocket" do
  let :loop do
    Coolio::Loop.new
  end
  
  before :each do
    @echo = UDPSocket.open
    @echo.bind nil, 0
    @port = @echo.addr[1]
    
    @running = true
    @echo_thread = Thread.new do
      while @running
        begin
          msg, sender = @echo.recvfrom_nonblock(3)
          @echo.send(msg + "bbb", 0, sender[3], sender[1])
        rescue IO::WaitReadable
        end
        Thread.pass
      end
    end
  end
  
  after :each do
    @running = false
    @echo_thread.join
    @echo.close
  end
  
  class Readable < Cool.io::IOWatcher
    attr :socket, :received
    def initialize
      @socket = UDPSocket.new
      super(@socket)
    end
    
    def on_readable
      @received = @socket.recvfrom_nonblock(6).first
    end
  end
  
  it "receive message #on_readable 5 times" do
    5.times do
      begin
        r = Readable.new
        r.socket.send "aaa", 0, "localhost", @port
        
        loop.attach r
        loop.run_once
        expect(r.received).to eq "aaabbb"
      ensure
        r.detach
      end
    end
  end
end
