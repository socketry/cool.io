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
        raise RuntimeError, "connection failed" if @connector.failed?
        
        unless @connector.complete?
          @connector.attach(evloop)
          return self
        end
        
        # Unset the connector and allow it to be GCed
        @connector = nil
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
      
      def failed? 
        @failed
      end
      
      def on_writable
        l = evloop
        detach
                
        if(@ruby_socket.getsockopt(::Socket::SOL_SOCKET, ::Socket::SO_ERROR).unpack('i').first == 0)
          @complete = true
          @rev_socket.attach(l)
          @rev_socket.on_connect
        else
          @failed = true
          @rev_socket.on_connect_failed
        end
      end      
    end
  end
  
  class TCPSocket < Socket
    attr_reader :remote_host, :remote_addr, :remote_port, :address_family
    
    # Perform a non-blocking connect to the given host and port
    def self.connect(addr, port, *args)
      super(TCPConnectSocket.new(addr, port), *args)
    end
    
    def initialize(socket)
      unless socket.is_a?(::TCPSocket) or socket.is_a?(TCPConnectSocket)
        raise TypeError, "socket must be a TCPSocket"
      end
      
      super
      
      @address_family, @remote_port, @remote_host, @remote_addr = socket.peeraddr  
    end
    
    #########
    protected
    #########
    
    class TCPConnectSocket < ::Socket
      def initialize(addr, port)
        @addr, @port = addr, port
        @address_family = nil

        if (Resolv::IPv4.create(addr) rescue nil)
          @address_family = ::Socket::AF_INET
        elsif(Resolv::IPv6.create(addr) rescue nil)
          @address_family = ::Socket::AF_INET6
        else raise ArgumentError, "address #{addr} not recognized (DNS is not yet supported)"
        end

        socket = super(@address_family, ::Socket::SOCK_STREAM, 0)

        begin
          socket.connect_nonblock(::Socket.sockaddr_in(port, addr))
        rescue Errno::EINPROGRESS
        end
      end
      
      def peeraddr
        [
          @address_family == ::Socket::AF_INET ? 'AF_INET' : 'AF_INET6',
          @port,
          @addr,
          @addr
        ]
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