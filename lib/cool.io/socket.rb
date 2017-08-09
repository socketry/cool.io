#--
# Copyright (C)2007 Tony Arcieri
# You can redistribute this under the terms of the Ruby license
# See file LICENSE for details
#++

require 'socket'
require 'resolv'

module Coolio
  class Socket < IO
    def self.connect(socket, *args)

      new(socket, *args).instance_eval do
        @_connector = Connector.new(self, socket)
        self
      end
    end

    # Just initializes some instance variables to avoid
    # warnings and calls super().
    def initialize *args
      @_failed = nil
      @_connector = nil
      super
    end

    watcher_delegate :@_connector

    remove_method :attach
    def attach(evloop)
      raise RuntimeError, "connection failed" if @_failed

      if @_connector
        @_connector.attach(evloop)
        return self
      end

      super
    end

    # Called upon completion of a socket connection
    def on_connect; end
    event_callback :on_connect

    # Called if a socket connection failed to complete
    def on_connect_failed; end
    event_callback :on_connect_failed

    # Called if a hostname failed to resolve when connecting
    # Defaults to calling on_connect_failed
    alias_method :on_resolve_failed, :on_connect_failed

    #########
    protected
    #########

    class Connector < IOWatcher
      def initialize(coolio_socket, ruby_socket)
        @coolio_socket, @ruby_socket = coolio_socket, ruby_socket
        super(ruby_socket, :w)
      end

      def on_writable
        evl = evloop
        detach

        if connect_successful?
          @coolio_socket.instance_eval { @_connector = nil }
          @coolio_socket.attach(evl)
          @ruby_socket.setsockopt(::Socket::IPPROTO_TCP, ::Socket::TCP_NODELAY, [1].pack("l"))
          @ruby_socket.setsockopt(::Socket::SOL_SOCKET, ::Socket::SO_KEEPALIVE, true)

          @coolio_socket.__send__(:on_connect)
        else
          @coolio_socket.instance_eval { @_failed = true }
          @coolio_socket.__send__(:on_connect_failed)
        end
      end

      #######
      private
      #######

      def connect_successful?
        @ruby_socket.getsockopt(::Socket::SOL_SOCKET, ::Socket::SO_ERROR).unpack('i').first == 0
      rescue IOError
        false
      end
    end
  end

  class TCPSocket < Socket
    attr_reader :remote_host, :remote_addr, :remote_port, :address_family
    watcher_delegate :@_resolver

    # Similar to .new, but used in cases where the resulting object is in a
    # "half-open" state.  This is primarily used for when asynchronous
    # DNS resolution is taking place.  We don't actually have a handle to
    # the socket we want to use to create the watcher yet, since we don't
    # know the IP address to connect to.
    def self.precreate(*args, &block)
      obj = allocate
      obj.__send__(:preinitialize, *args, &block)
      obj
    end

    # Perform a non-blocking connect to the given host and port
    # see examples/echo_client.rb
    # addr is a string, can be an IP address or a hostname.
    def self.connect(addr, port, *args)
      family = nil

      if (Resolv::IPv4.create(addr) rescue nil)
        family = ::Socket::AF_INET
      elsif(Resolv::IPv6.create(addr) rescue nil)
        family = ::Socket::AF_INET6
      end

      if family
        return super(TCPConnectSocket.new(family, addr, port), *args) # this creates a 'real' write buffer so we're ok there with regards to already having a write buffer from the get go
      end

      if host = Coolio::DNSResolver.hosts(addr)
        return connect(host, port, *args) # calls this same function
      end

      precreate(addr, port, *args)
    end

    # Called by precreate during asyncronous DNS resolution
    def preinitialize(addr, port, *args)
      @_write_buffer = ::IO::Buffer.new # allow for writing BEFORE DNS has resolved
      @remote_host, @remote_addr, @remote_port = addr, addr, port
      @_resolver = TCPConnectResolver.new(self, addr, port, *args)
    end

    private :preinitialize

    PEERADDR_FAILED = ["?", 0, "name resolusion failed", "?"]

    def initialize(socket)
      unless socket.is_a?(::TCPSocket) or socket.is_a?(TCPConnectSocket)
        raise TypeError, "socket must be a TCPSocket"
      end

      super

      @address_family, @remote_port, @remote_host, @remote_addr = (socket.peeraddr rescue PEERADDR_FAILED)
    end

    def peeraddr
      [@address_family, @remote_port, @remote_host, @remote_addr]
    end

    #########
    protected
    #########

    class TCPConnectSocket < ::Socket
      def initialize(family, addr, port, host = addr)
        @host, @addr, @port = host, addr, port
        @address_family = nil

        super(family, ::Socket::SOCK_STREAM, 0)
        begin
          connect_nonblock(::Socket.sockaddr_in(port, addr))
        rescue Errno::EINPROGRESS
        end
      end

      def peeraddr
        [
          @address_family == ::Socket::AF_INET ? 'AF_INET' : 'AF_INET6',
          @port,
          @host,
          @addr
        ]
      end
    end

    class TCPConnectResolver < Coolio::DNSResolver
      def initialize(socket, host, port, *args)
        @sock, @host, @port, @args = socket, host, port, args
        super(host)
      end

      def on_success(addr)
        host, port, args = @host, @port, @args

        @sock.instance_eval do
          # DNSResolver only supports IPv4 so we can safely assume IPv4 address
          begin
            socket = TCPConnectSocket.new(::Socket::AF_INET, addr, port, host)
          rescue Errno::ENETUNREACH
            on_connect_failed
            return
          end

          initialize(socket, *args)
          @_connector = Socket::Connector.new(self, socket)
          @_resolver = nil
        end
        @sock.attach(evloop)
      end

      def on_failure
        @sock.__send__(:on_resolve_failed)
        @sock.instance_eval do
          @_resolver = nil
          @_failed = true
        end
        return
      end
    end
  end

  class UNIXSocket < Socket
    attr_reader :path, :address_family

    # Connect to the given UNIX domain socket
    def self.connect(path, *args)
      new(::UNIXSocket.new(path), *args)
    end

    def initialize(socket)
      raise ArgumentError, "socket must be a UNIXSocket" unless socket.is_a? ::UNIXSocket

      super
      @address_family, @path = socket.peeraddr
    end
  end
end
