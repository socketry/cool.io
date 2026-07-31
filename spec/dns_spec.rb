require File.expand_path('../spec_helper', __FILE__)
require 'tempfile'

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

  describe "IPv6 nameserver filtering" do
    it "ignores IPv6 nameservers provided in arguments" do
      resolver = Coolio::DNSResolver.new("example.com", "8.8.8.8", "2001:4860:4860::8888", "1.1.1.1")

      nameservers = resolver.instance_variable_get(:@nameservers)
      expect(nameservers).to eq(["8.8.8.8", "1.1.1.1"])
    end

    it "falls back to default IPv4 config if only IPv6 addresses are provided" do
      allow(Resolv::DNS::Config).to receive(:default_config_hash).and_return({
        nameserver: ["8.8.4.4", "2001:4860:4860::8844"]
      })

      resolver = Coolio::DNSResolver.new("example.com", "2001:4860:4860::8888")

      nameservers = resolver.instance_variable_get(:@nameservers)
      expect(nameservers).to eq(["8.8.4.4"])
    end
  end

  describe "nameserver normalization" do
    let(:localhost_address) do
      Addrinfo.getaddrinfo("localhost", nil, ::Socket::AF_INET, ::Socket::SOCK_DGRAM).first.ip_address
    end

    it "keeps the nameserver list as given" do
      resolver = Coolio::DNSResolver.new("example.com", "localhost")

      expect(resolver.instance_variable_get(:@nameservers)).to eq(["localhost"])
    end

    it "queries the numeric address of a nameserver given as a hostname" do
      resolver = Coolio::DNSResolver.new("example.com", "localhost")
      resolver.__send__(:send_request)

      expect(resolver.instance_variable_get(:@queried_addresses)).to eq([localhost_address])
    end

    it "accepts responses from a nameserver which was given as a hostname" do
      resolver = Coolio::DNSResolver.new("example.com", "localhost")
      resolver.__send__(:send_request)
      response = dns_response_for(resolver)

      expect(
        resolver.__send__(:solicited_response?, response, ["AF_INET", 53, localhost_address, localhost_address])
      ).to be true
    end

    it "looks a nameserver up once and reuses the result on retries" do
      resolved = Addrinfo.getaddrinfo("127.0.0.1", nil, ::Socket::AF_INET, ::Socket::SOCK_DGRAM)
      resolver = Coolio::DNSResolver.new("example.com", "127.0.0.1")

      expect(Addrinfo).to receive(:getaddrinfo).once.and_return(resolved)

      3.times { resolver.__send__(:send_request) }
    end

    it "does not reject an unresolvable nameserver at construction" do
      allow(Addrinfo).to receive(:getaddrinfo).and_raise(SocketError, "getaddrinfo: Name or service not known")

      expect do
        Coolio::DNSResolver.new("example.com", "no-such-nameserver.invalid")
      end.to_not raise_error
    end

    it "surfaces an unresolvable nameserver as a SocketError from the request" do
      allow(Addrinfo).to receive(:getaddrinfo).and_raise(SocketError, "getaddrinfo: Name or service not known")
      resolver = Coolio::DNSResolver.new("example.com", "no-such-nameserver.invalid")

      expect { resolver.attach(@loop) }.to raise_error(SocketError)
      expect(@loop.watchers).to be_empty
    end

    it "recovers when a nameserver is only transiently unresolvable" do
      resolved = Addrinfo.getaddrinfo("127.0.0.1", nil, ::Socket::AF_INET, ::Socket::SOCK_DGRAM)
      resolver = Coolio::DNSResolver.new("example.com", "ns.example.test")

      attempts = 0
      allow(Addrinfo).to receive(:getaddrinfo) do
        attempts += 1
        raise SocketError, "getaddrinfo: Name or service not known" if attempts == 1

        resolved
      end

      expect { resolver.__send__(:send_request) }.to raise_error(SocketError)
      expect { resolver.__send__(:send_request) }.to_not raise_error
      expect(resolver.instance_variable_get(:@queried_addresses)).to eq(["127.0.0.1"])
    end
  end

  describe "response validation" do
    let(:nameserver) { "127.0.0.1" }
    let(:sender) { ["AF_INET", 53, nameserver, nameserver] }
    let(:resolver) do
      Coolio::DNSResolver.new("example.com", nameserver).tap { |r| r.__send__(:send_request) }
    end

    it "uses an unpredictable transaction ID for each query" do
      ids = 10.times.map do
        request_id_of(Coolio::DNSResolver.new("example.com", nameserver))
      end

      expect(ids.uniq.size).to be > 1
    end

    it "accepts a response carrying our transaction ID from the queried nameserver" do
      expect(
        resolver.__send__(:solicited_response?, dns_response_for(resolver), sender)
      ).to be true
    end

    it "rejects a response carrying a different transaction ID" do
      forged = dns_response_for(resolver, id: (request_id_of(resolver) + 1) % 65536)

      expect(resolver.__send__(:solicited_response?, forged, sender)).to be false
    end

    it "rejects a response from a source address we did not query" do
      response = dns_response_for(resolver)

      expect(
        resolver.__send__(:solicited_response?, response, ["AF_INET", 53, "10.11.12.13", "10.11.12.13"])
      ).to be false
    end

    it "rejects a response arriving before the request was sent" do
      unsent = Coolio::DNSResolver.new("example.com", nameserver)

      expect(unsent.__send__(:solicited_response?, dns_response_for(unsent), sender)).to be false
    end

    it "rejects a response from a source port other than the DNS port" do
      response = dns_response_for(resolver)

      expect(
        resolver.__send__(:solicited_response?, response, ["AF_INET", 4444, nameserver, nameserver])
      ).to be false
    end

    it "rejects a truncated datagram" do
      expect(resolver.__send__(:solicited_response?, "\0\0", sender)).to be false
    end

    it "resolves from a response sent by the queried nameserver" do
      response = dns_response_for(resolver, address: "1.2.3.4")
      allow(resolver.instance_variable_get(:@socket)).to receive(:recvfrom_nonblock).and_return([response, sender])

      expect(resolver).to receive(:on_success).with("1.2.3.4")
      expect(resolver).to receive(:detach)

      resolver.__send__(:on_readable)
    end

    it "ignores a spoofed response instead of resolving or failing it" do
      forged = dns_response_for(resolver, address: "6.6.6.6")
      allow(resolver.instance_variable_get(:@socket)).to receive(:recvfrom_nonblock)
        .and_return([forged, ["AF_INET", 53, "10.11.12.13", "10.11.12.13"]])

      expect(resolver).to_not receive(:on_success)
      expect(resolver).to_not receive(:on_failure)
      expect(resolver).to_not receive(:detach)

      resolver.__send__(:on_readable)
    end
  end

  def request_id_of(resolver)
    resolver.__send__(:request_message)[0..1].unpack('n').first
  end

  # A response to the resolver's own query: header plus the echoed question,
  # and an A record when an address is given.
  def dns_response_for(resolver, id: request_id_of(resolver), address: nil)
    question = resolver.instance_variable_get(:@question)
    answer = if address
      # Compressed name pointer, type A, class IN, TTL, RDLENGTH, RDATA
      [0xc00c, 1, 1, 60, 4].pack('nnnNn') + address.split('.').map(&:to_i).pack('CCCC')
    else
      ""
    end

    [id, 0x81, 0x80, 1, answer.empty? ? 0 : 1, 0, 0].pack('nCCnnnn') + question + answer
  end
end
