require File.expand_path('../spec_helper', __FILE__)

describe Coolio::TCPSocket do
  let :loop do
    Coolio::Loop.new
  end
  
  before :each do
    @echo = TCPServer.new("127.0.0.1", 0)
    @host = @echo.addr[3]
    @port = @echo.addr[1]
    @running = true
    @echo_thread = Thread.new do
      socks = [@echo]
      begin
        serv socks
      ensure
        socks.each do |s|
          s.close
        end
      end
      Thread.pass
    end
  end
  
  def serv(socks)
    while @running
      selected = select(socks, [], [], 0.1)
      next if selected.nil?
      selected[0].each do |s|
        if s == @echo
          socks.push s.accept
          next
        end
        begin
          unless s.eof?
            s.write(s.read_nonblock 1)
          end
        rescue SystemCallError, EOFError, IOError, SocketError
        end
      end
    end
  end
  
  def shutdown
    if @running
      @running = false
      @echo_thread.join
    end
  end
  
  after :each do
    shutdown
  end

  context "#close" do
    it "detaches all watchers on #close before loop#run" do
      client = Coolio::TCPSocket.connect(@host, @port)
      loop.attach client
      client.close
      expect(loop.watchers.size).to eq 0
    end
  end

  context "#on_connect" do
    class OnConnect < Cool.io::TCPSocket
      attr :connected
      def on_connect
        @connected = true
      end
    end
    
    it "connected client called on_connect" do
      begin
        c = OnConnect.connect(@host, @port)
        loop.attach c
        loop.run_once
        expect(c.connected).to eq true
      ensure
        c.close
      end
    end
  end

  context "#on_connect_failed" do
    class OnConnectFailed < Cool.io::TCPSocket
      attr :connect_failed
      def on_connect_failed
        @connect_failed = true
      end
    end
    
    it "try to connect dead host" do
      serv = TCPServer.new(0)
      dead_host = serv.addr[3]
      dead_port = serv.addr[1]
      serv.close
      
      c = OnConnectFailed.connect(dead_host, dead_port)
      loop.attach c
      loop.run_once # on_connect_failed
      expect(c.connect_failed).to eq true
    end
  end

  context "#on_close" do
    class Closed < StandardError; end
    class OnClose < Cool.io::TCPSocket
      def on_close
        raise Closed
      end
    end
    
    let :client do
      OnClose.connect(@host, @port)
    end
    
    before :each do
      loop.attach client
      loop.run_once # on_connect
      client.write "0"
    end
    
    it "disconnect from client" do
      expect { client.close }.to raise_error(Closed)
    end

    it "disconnect from server" do
      shutdown
      expect { loop.run }.to raise_error(Closed)
    end
  end
  
  context "#on_read" do
    class Finished < StandardError; end
    class OnRead < Cool.io::TCPSocket
      attr :read_data, :times
      def on_connect
        @read_data = ""
        @times = 0
      end
      def on_read(data)
        @read_data += data
        @times += 1
        if @times < 5
          write "#{@times}"
        else
          close
          raise Finished
        end
      end
    end
    
    it "receive 5 times" do
      c = OnRead.connect(@host, @port)
      loop.attach c
      loop.run_once # on_connect
      c.write "0"
      expect { loop.run }.to raise_error(Finished)
      
      expect(c.times).to eq 5
      expect(c.read_data).to eq "01234"
    end
  end
  
  context "#on_write_complete" do
    class WriteComplete < StandardError; end
    class OnWriteComplete < Cool.io::TCPSocket
      attr :called
      def on_write_complete
        @called = true
        close
        raise WriteComplete
      end
    end
    
    it "on_write_complete is called" do
      c = OnWriteComplete.connect(@host, @port)
      loop.attach c
      loop.run_once # on_connect
      c.write "aaa"
      expect { loop.run }.to raise_error(WriteComplete)
    end
  end
end
