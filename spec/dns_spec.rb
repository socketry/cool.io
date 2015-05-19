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
end
