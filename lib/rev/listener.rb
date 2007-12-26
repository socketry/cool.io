#--
# Copyright (C)2007 Tony Arcieri
# You can redistribute this under the terms of the Ruby license
# See file LICENSE for details
#++

require 'socket'
require File.dirname(__FILE__) + '/../rev'

module Rev
  class Listener < IOWatcher
    def initialize(listen_socket)
      @listen_socket = listen_socket
      super(@listen_socket)
    end

    # Called whenever the server receives a new connection
    def on_connection(socket); end
    event_callback :on_connection

    #########
    protected
    #########

    # Rev callback for handling new connections
    def on_readable
      on_connection @listen_socket.accept_nonblock
    end
  end

  class TCPListener < Listener
    # Create a new Rev::TCPListener
    #
    # Accepts the same arguments as TCPServer.new
    def initialize(*args)
      listen_socket = ::TCPServer.new(*args)
      listen_socket.instance_eval { listen(1024) } # Change listen backlog to 1024
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
