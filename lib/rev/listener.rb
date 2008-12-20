#--
# Copyright (C)2007 Tony Arcieri
# You can redistribute this under the terms of the Ruby license
# See file LICENSE for details
#++

require 'socket'

module Rev
  # Listeners wait for incoming connections.  When a listener receives a
  # connection it fires the on_connection event with the newly accepted
  # socket as a parameter.
  class Listener < IOWatcher
    def initialize(listen_socket)
      @listen_socket = listen_socket
      super(@listen_socket)
    end
    
    # Close the listener
    def close
      detach if attached?
      @listen_socket.close
    end

    # Called whenever the server receives a new connection
    def on_connection(socket); end
    event_callback :on_connection

    #########
    protected
    #########

    # Rev callback for handling new connections
    def on_readable
      begin
        on_connection @listen_socket.accept_nonblock
      rescue Errno::EAGAIN
        STDERR.puts "warning: listener socket spuriously readable"
      end
    end
  end

  class TCPListener < Listener
    DEFAULT_BACKLOG = 1024
    
    # Create a new Rev::TCPListener on the specified address and port.
    # Accepts the following options:
    #
    #  :backlog - Max size of the pending connection queue (default 1024)
    #  :reverse_lookup - Retain BasicSocket's reverse DNS functionality (default false)
    #
    def initialize(addr, port, options = {})
      BasicSocket.do_not_reverse_lookup = true unless options[:reverse_lookup]
      options[:backlog] ||= DEFAULT_BACKLOG
      
      listen_socket = ::TCPServer.new(addr, port)
      listen_socket.instance_eval { listen(options[:backlog]) }
      super(listen_socket)
    end
  end

  class UNIXListener < Listener
    # Create a new Rev::UNIXListener
    #
    # Accepts the same arguments as UNIXServer.new
    def initialize(*args)
      super(::UNIXServer.new(*args))
    end
  end
end
