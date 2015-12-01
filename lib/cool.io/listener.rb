#--
# Copyright (C)2007-10 Tony Arcieri
# You can redistribute this under the terms of the Ruby license
# See file LICENSE for details
#++

require 'socket'

module Coolio
  # Listeners wait for incoming connections.  When a listener receives a
  # connection it fires the on_connection event with the newly accepted
  # socket as a parameter.
  class Listener < IOWatcher
    def initialize(listen_socket)
      @listen_socket = listen_socket
      super(@listen_socket)
    end

    # Returns an integer representing the underlying numeric file descriptor
    def fileno
      @listen_socket.fileno
    end

    def listen(backlog)
      @listen_socket.listen(backlog)
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

    # Coolio callback for handling new connections
    unless RUBY_PLATFORM =~ /mingw|mswin/
      def on_readable
        begin
          on_connection @listen_socket.accept_nonblock
        rescue Errno::EAGAIN, Errno::ECONNABORTED
          # EAGAIN can be triggered here if the socket is shared between
          # multiple processes and a thundering herd is woken up to accept
          # one connection, only one process will get the connection and
          # the others will be awoken.
          # ECONNABORTED is documented in accept() manpages but modern TCP
          # stacks with syncookies and/or accept()-filtering for DoS
          # protection do not see it.  In any case this error is harmless
          # and we should instead spend our time with clients that follow
          # through on connection attempts.
        end
      end
    else
      def on_readable
        begin
          # In Windows, accept_nonblock() with multiple processes
          # causes thundering herd problem.
          # To avoid this, we need to use accept().
          on_connection @listen_socket.accept
        rescue Errno::EAGAIN, Errno::ECONNABORTED
        end
      end
    end
  end

  DEFAULT_BACKLOG = 1024

  class TCPListener < Listener
    # Create a new Coolio::TCPListener on the specified address and port.
    # Accepts the following options:
    #
    #  :backlog - Max size of the pending connection queue (default 1024)
    #  :reverse_lookup - Retain BasicSocket's reverse DNS functionality (default false)
    #
    # If the specified address is an TCPServer object, it will ignore
    # the port and :backlog option and create a new Coolio::TCPListener out
    # of the existing TCPServer object.
    def initialize(addr, port = nil, options = {})
      BasicSocket.do_not_reverse_lookup = true unless options[:reverse_lookup]
      options[:backlog] ||= DEFAULT_BACKLOG

      listen_socket = if ::TCPServer === addr
        addr
      else
        raise ArgumentError, "port must be an integer" if nil == port
        ::TCPServer.new(addr, port)
      end
      listen_socket.instance_eval { listen(options[:backlog]) }
      super(listen_socket)
    end
  end

  class UNIXListener < Listener
    # Create a new Coolio::UNIXListener
    #
    # Accepts the same arguments as UNIXServer.new
    # Optionally, it can also take anyn existing UNIXServer object
    # and create a Coolio::UNIXListener out of it.
    def initialize(*args)
      s = ::UNIXServer === args.first ? args.first : ::UNIXServer.new(*args)
      s.instance_eval { listen(DEFAULT_BACKLOG) }
      super(s)
    end
  end
end
