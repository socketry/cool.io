require File.expand_path('../spec_helper', __FILE__)

describe Coolio::TCPSocket do

  before :each do
    @server = TCPServer.new('127.0.0.1', 0)
    @host = @server.addr[3]
    @port = @server.addr[1]
  end

  after :each do
    @server.close
  end

  describe '#close' do

    it 'detaches all watchers on #close before loop#run' do
      reactor = Coolio::Loop.new
      client = Coolio::TCPSocket.connect(@host, @port)
      reactor.attach(client)
      client.close
      reactor.watchers.size.should == 0
    end
  end

end

