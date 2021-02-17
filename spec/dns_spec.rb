require File.expand_path('../spec_helper', __FILE__)

VALID_DOMAIN = "google.com"
INVALID_DOMAIN = "gibidigibigididibitidibigitibidigitidididi.com"

class ItWorked < StandardError; end
class WontResolve < StandardError; end

class ConnectorThingy < Cool.io::TCPSocket
  def on_connect
    raise ItWorked
  end

  def on_resolve_failed
    raise WontResolve
  end
end

describe "DNS" do
  before :each do
    @loop = Cool.io::Loop.new
    @preferred_localhost_address = ::Socket.getaddrinfo("localhost", nil).first[3]
  end
  
  it "connects to valid domains" do
    begin
      c = ConnectorThingy.connect(VALID_DOMAIN, 80).attach(@loop)
      
      expect do
        @loop.run
      end.to raise_error(ItWorked)
    ensure
      c.close
    end
  end
  
  it "fires on_resolve_failed for invalid domains" do
    ConnectorThingy.connect(INVALID_DOMAIN, 80).attach(@loop)
    
    expect do
      @loop.run
    end.to raise_error(WontResolve)
  end

  it "resolve localhost even though hosts is empty" do
    Tempfile.open("empty") do |file|
      expect( Coolio::DNSResolver.hosts("localhost", file.path)).to eq @preferred_localhost_address
    end
  end

  it "resolve missing localhost even though hosts entries exist" do
    Tempfile.open("empty") do |file|
      file.puts("127.0.0.1 example.internal")
      file.flush
      expect( Coolio::DNSResolver.hosts("localhost", file.path)).to eq @preferred_localhost_address
    end
  end
end
