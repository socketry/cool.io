#--
# Copyright (C)2007-10 Tony Arcieri
# You can redistribute this under the terms of the Ruby license
# See file LICENSE for details
#
# Gimpy hacka asynchronous DNS resolver
#
# Word to the wise: I don't know what I'm doing here.  This was cobbled together
# as best I could with extremely limited knowledge of the DNS format.  There's
# obviously a ton of stuff it doesn't support (like IPv6 and TCP).
#
# If you do know what you're doing with DNS, feel free to improve this!
# A good starting point my be this EventMachine Net::DNS-based asynchronous
# resolver:
#
# http://gist.github.com/663299
#
#++

require 'resolv'

module Coolio
  # A non-blocking DNS resolver.  It provides interfaces for querying both
  # /etc/hosts and nameserves listed in /etc/resolv.conf, or nameservers of
  # your choosing.
  #
  # Presently the client only supports UDP requests against your nameservers
  # and cannot resolve anything with records larger than 512-bytes.  Also,
  # IPv6 is not presently supported.
  #
  # DNSResolver objects are one-shot.  Once they resolve a domain name they
  # automatically detach themselves from the event loop and cannot be used
  # again.
  class DNSResolver < IOWatcher
    #--
    DNS_PORT = 53
    DATAGRAM_SIZE = 512
    TIMEOUT = 3 # Retry timeout for each datagram sent
    RETRIES = 4 # Number of retries to attempt
    # so currently total is 12s before it will err due to timeouts
    # if it errs due to inability to reach the DNS server [Errno::EHOSTUNREACH], same
    # Query /etc/hosts (or the specified hostfile) for the given host
    def self.hosts(host, hostfile = Resolv::Hosts::DefaultFileName)
      hosts = {}
      File.open(hostfile) do |f|
        f.each_line do |host_entry|
          entries = host_entry.gsub(/#.*$/, '').gsub(/\s+/, ' ').split(' ')
          addr = entries.shift
          entries.each { |e| hosts[e] ||= addr }
        end
      end

      hosts[host]
    end

    # Create a new Coolio::Watcher descended object to resolve the
    # given hostname.  If you so desire you can also specify a
    # list of nameservers to query.  By default the resolver will
    # use nameservers listed in /etc/resolv.conf
    def initialize(hostname, *nameservers)
      if nameservers.empty?
        nameservers = Resolv::DNS::Config.default_config_hash[:nameserver]
        raise RuntimeError, "no nameservers found" if nameservers.empty? # TODO just call resolve_failed, not raise [also handle Errno::ENOENT)]
      end

      @nameservers = nameservers
      @question = request_question hostname

      @socket = UDPSocket.new
      @timer = Timeout.new(self)

      super(@socket)
    end

    # Attach the DNSResolver to the given event loop
    def attach(evloop)
      send_request
      @timer.attach(evloop)
      super
    end

    # Detach the DNSResolver from the given event loop
    def detach
      @timer.detach if @timer.attached?
      super
    end

    # Called when the name has successfully resolved to an address
    def on_success(address); end
    event_callback :on_success

    # Called when we receive a response indicating the name didn't resolve
    def on_failure; end
    event_callback :on_failure

    # Called if we don't receive a response, defaults to calling on_failure
    def on_timeout
      on_failure
    end

    #########
    protected
    #########

    # Send a request to the DNS server
    def send_request
      nameserver = @nameservers.shift
      @nameservers << nameserver # rotate them
      begin
        @socket.send request_message, 0, @nameservers.first, DNS_PORT
      rescue Errno::EHOSTUNREACH # TODO figure out why it has to be wrapper here, when the other wrapper should be wrapping this one!
      end
    end

    # Called by the subclass when the DNS response is available
    def on_readable
      datagram = nil
      begin
        datagram = @socket.recvfrom_nonblock(DATAGRAM_SIZE).first
      rescue Errno::ECONNREFUSED
      end

      address = response_address datagram rescue nil
      address ? on_success(address) : on_failure
      detach
    end

    def request_question(hostname)
      raise ArgumentError, "hostname cannot be nil" if hostname.nil?

      # Query name
      message = hostname.split('.').map { |s| [s.size].pack('C') << s }.join + "\0"

      # Host address query
      qtype = 1

      # Internet query
      qclass = 1

      message << [qtype, qclass].pack('nn')
    end

    def request_message
      # Standard query header
      message = [2, 1, 0].pack('nCC')

      # One entry
      qdcount = 1

      # No answer, authority, or additional records
      ancount = nscount = arcount = 0

      message << [qdcount, ancount, nscount, arcount].pack('nnnn')
      message << @question
    end

    def response_address(message)
      # Confirm the ID field
      id = message[0..1].unpack('n').first.to_i
      return unless id == 2

      # Check the QR value and confirm this message is a response
      qr = message[2..2].unpack('B1').first.to_i
      return unless qr == 1

      # Check the RCODE (lower nibble) and ensure there wasn't an error
      rcode = message[3..3].unpack('B8').first[4..7].to_i(2)
      return unless rcode == 0

      # Extract the question and answer counts
      qdcount, _ancount = message[4..7].unpack('nn').map { |n| n.to_i }

      # We only asked one question
      return unless qdcount == 1
      message.slice!(0, 12)

      # Make sure it's the same question
      return unless message[0..(@question.size-1)] == @question
      message.slice!(0, @question.size)

      # Extract the RDLENGTH
      while not message.empty?
        type = message[2..3].unpack('n').first.to_i
        rdlength = message[10..11].unpack('n').first.to_i
        rdata = message[12..(12 + rdlength - 1)]
        message.slice!(0, 12 + rdlength)

        # Only IPv4 supported
        next unless rdlength == 4

        # If we got an Internet address back, return it
        return rdata.unpack('CCCC').join('.') if type == 1
      end

      nil
    end

    class Timeout < TimerWatcher
      def initialize(resolver)
        @resolver = resolver
        @attempts = 0
        super(TIMEOUT, true)
      end

      def on_timer
        @attempts += 1
        if @attempts <= RETRIES
          begin
            return @resolver.__send__(:send_request)
          rescue Errno::EHOSTUNREACH # if the DNS is toast try again after the timeout occurs again
            return nil
          end
        end
        @resolver.__send__(:on_timeout)
        @resolver.detach
      end
    end
  end
end
