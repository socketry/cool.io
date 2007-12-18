require File.dirname(__FILE__) + '/../rev'

module Rev
  class TCPSocket < BufferedIO
    attr_reader :remote_host, :remote_addr, :remote_port, :address_family
    
    def initialize(socket)
      raise ArgumentError, "socket must be a TCPSocket" unless socket.is_a? ::TCPSocket
      
      super
      @address_family, @remote_port, @remote_host, @remote_addr = socket.peeraddr
    end
  end
  
  class UNIXSocket < BufferedIO
    attr_reader :path, :address_family
    
    def initialize(socket)
      raise ArgumentError, "socket must be a UNIXSocket" unless socket.is_a? ::UNIXSocket
      
      super
      @address_family, @path = socket.peeraddr
    end
  end
end