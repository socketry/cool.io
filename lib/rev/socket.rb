require 'socket'
require 'resolv'
require File.dirname(__FILE__) + '/../rev'

module Rev
  class Socket < BufferedIO
    def self.connect(socket, *args)
      new(socket, *args).instance_eval {
        @connector = Connector.new(self, socket)
        self
      }
    end
    
    def attach(evloop)
      if @connector        
        raise RuntimeError, "connection failed" if @failed
        
        unless @connector.complete?
          @connector.attach(evloop)
          return self
        end
        
        # Unset the connector and allow it to be GCed
        @connector = nil
      end
      
      super
    end

    def detach
      if @connector
        @connector.detach
        return self
      end

      super
    end

    def enable
      if @connector
        @connector.enable
        return self
      end

      super
    end

    def disable
      if @connector
        @connector.disable
        return self
      end

      super
    end

    # Called upon completion of a socket connection
    def on_connect
    end
    
    # Called if a socket connection failed to complete
    def on_connect_failed
    end
    
    #########
    protected
    #########
    
    class Connector < IOWatcher
      def initialize(rev_socket, ruby_socket)
        @rev_socket, @ruby_socket = rev_socket, ruby_socket
        @failed = @complete = false        
        super(ruby_socket, :w)
      end
      
      def complete?
        @complete
      end
      
      def on_writable
        l = evloop
        detach
                
        if(@ruby_socket.getsockopt(::Socket::SOL_SOCKET, ::Socket::SO_ERROR).unpack('i').first == 0)
          @complete = true
          @rev_socket.attach(l)
          @rev_socket.on_connect
        else
          @rev_socket.on_connect_failed
          @rev_socket.instance_eval { @failed = true }
        end
      end      
    end
  end
  
  class TCPSocket < Socket
    attr_reader :remote_host, :remote_addr, :remote_port, :address_family
    
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

      return allocate.instance_eval {
        @remote_host, @remote_addr, @remote_port = addr, addr, port
        @resolver = TCPConnectResolver.new(self, addr, port, *args)
      }
    end
    
    def initialize(socket)
      unless socket.is_a?(::TCPSocket) or socket.is_a?(TCPConnectSocket)
        raise TypeError, "socket must be a TCPSocket"
      end
      
      super
      
      @address_family, @remote_port, @remote_host, @remote_addr = socket.peeraddr  
    end

    def attach(evloop)
      if @resolver
        @resolver.attach(evloop)
        return self
      end

      super
    end

    def detach
      if @resolver
        @resolver.detach
        return self
      end

      super
    end

    def enable(evloop)
      if @resolver
        @resolver.enable
        return self
      end

      super
    end

    def disable(evloop)
      if @resolver
        @resolver.disable
        return self
      end

      super
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
        # DNSResolver only supports IPv4 so we can safely assume an IPv4 address

        host, port, args = @host, @port, @args
        @sock.instance_eval {
          socket = TCPConnectSocket.new(::Socket::AF_INET, addr, port, host)
          initialize(socket, *args)
          @connector = Connector.new(self, socket)
          @resolver = nil
        }
        @sock.attach(evloop)
      end

      def on_failure
        @sock.on_connect_failed
        @sock.instance_eval { 
          @resolver = nil 
          @failed = true
        }
        return
      end

      alias_method :on_timeout, :on_failure
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
