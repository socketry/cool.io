require 'socket'
require File.dirname(__FILE__) + '/../rev'

module Rev
  class Listener < Rev::IOWatcher
    # Listener yields new connections to the block passed to it
    def attach(evloop, &callback)
      raise ArgumentError, "no block given" unless block_given?
      @callback = callback
      super(evloop)
    end

    # Rev callback for handling new connections
    def on_readable
      @callback.(@listen_socket.accept)
    end
  end

  class TCPListener < Listener
    # Create a new Rev::TCPListener
    #
    # Accepts the same arguments as TCPServer.new
    def initialize(*args)
      @listen_socket = TCPServer.new(*args)
      @listen_socket.instance_eval { listen(1024) }
      super(@listen_socket)
    end
  end

  class UNIXListener < Listener
    # Create a new Rev::UNIXListener
    #
    # Accepts the same arguments as UNIXServer.new
    def initialize(*args)
      @listen_socket = UNIXServer.new(*args)
      super(@listen_socket)
    end
  end
end
