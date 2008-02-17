#--
# Copyright (C)2007 Tony Arcieri
# You can redistribute this under the terms of the Ruby license
# See file LICENSE for details
#++

require 'socket'
require 'resolv'

module Rev
  class Socket < IO    
    def self.connect(socket, *args)
      new(socket, *args).instance_eval {
        @connector = Connector.new(self, socket)
        self
      }
    end
    
    watcher_delegate :@connector

    def attach(evloop)
      raise RuntimeError, "connection failed" if @failed
      
      if @connector
        @connector.attach(evloop)
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
    # Defaults to being aliased to on_connect_failed
    alias_method :on_resolve_failed, :on_connect_failed
    
    #########
    protected
    #########
    
    class Connector < IOWatcher
      def initialize(rev_socket, ruby_socket)
        @rev_socket, @ruby_socket = rev_socket, ruby_socket
        super(ruby_socket, :w)
      end
      
      def on_writable
        evl = evloop
        detach

        if connect_successful?
          @rev_socket.instance_eval { @connector = nil }
          @rev_socket.attach(evl)
          @rev_socket.__send__(:on_connect)
        else
          @rev_socket.instance_eval { @failed = true }
          @rev_socket.__send__(:on_connect_failed)
        end
      end      

      #######
      private
      #######

      def connect_successful?
        @ruby_socket.getsockopt(::Socket::SOL_SOCKET, ::Socket::SO_ERROR).unpack('i').first == 0
      end
    end
  end
  
  class TCPSocket < Socket
    attr_reader :remote_host, :remote_addr, :remote_port, :address_family
    watcher_delegate :@resolver
    
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
    def self.connect(addr, port, *args)
      family = nil

      if (Resolv::IPv4.create(addr) rescue nil)
        family = ::Socket::AF_INET
      elsif(Resolv::IPv6.create(addr) rescue nil)
        family = ::Socket::AF_INET6
      end
 
      if family
        return super(TCPConnectSocket.new(family, addr, port), *args)
      end

      if host = Rev::DNSResolver.hosts(addr)
        return connect(host, port, *args)
      end

      precreate(addr, port, *args)
    end
    
    # Called by precreate during asyncronous DNS resolution
    def preinitialize(addr, port, *args)
      @remote_host, @remote_addr, @remote_port = addr, addr, port
      @resolver = TCPConnectResolver.new(self, addr, port, *args)
    end
    
    private :preinitialize
    
    def initialize(socket)
      unless socket.is_a?(::TCPSocket) or socket.is_a?(TCPConnectSocket)
        raise TypeError, "socket must be a TCPSocket"
      end
      
      super
      
      @address_family, @remote_port, @remote_host, @remote_addr = socket.peeraddr  
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

        @socket = super(family, ::Socket::SOCK_STREAM, 0)
        begin
          @socket.connect_nonblock(::Socket.sockaddr_in(port, addr))
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

    class TCPConnectResolver < Rev::DNSResolver
      def initialize(socket, host, port, *args)
        @sock, @host, @port, @args = socket, host, port, args
        super(host)
      end

      def on_success(addr)
        host, port, args = @host, @port, @args

        @sock.instance_eval {
          # DNSResolver only supports IPv4 so we can safely assume an IPv4 address
          socket = TCPConnectSocket.new(::Socket::AF_INET, addr, port, host)
          initialize(socket, *args)
          @connector = Socket::Connector.new(self, socket)
          @resolver = nil
        }
        @sock.attach(evloop)
      end

      def on_failure
        @sock.__send__(:on_resolve_failed)
        @sock.instance_eval { 
          @resolver = nil 
          @failed = true
        }
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
