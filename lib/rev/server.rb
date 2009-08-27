#--
# Copyright (C)2007 Tony Arcieri
# You can redistribute this under the terms of the Ruby license
# See file LICENSE for details
#++

module Rev
  class Server < Listener
    # Servers listen for incoming connections and create new connection objects
    # whenever incoming connections are received.  The default class for new
    # connections is a Socket, but any subclass of IOWatcher is acceptable.
    def initialize(listen_socket, klass = Socket, *args, &block)
      # Ensure the provided class responds to attach
      unless klass.allocate.is_a? IO
        raise ArgumentError, "can't convert #{klass} to Rev::IO"
      end

      # Verify the arity of the provided arguments
      arity = klass.instance_method(:initialize).arity
      expected = arity >= 0 ? arity : -(arity + 1)

      if (arity >= 0 and args.size + 1 != expected) or (arity < 0 and args.size + 1 < expected)
        raise ArgumentError, "wrong number of arguments for #{klass}#initialize (#{args.size+1} for #{expected})" 
      end
     
      @klass, @args, @block = klass, args, block
      super(listen_socket)
    end

    #########
    protected
    #########
    
    def on_connection(socket)
      connection = @klass.new(socket, *@args).attach(evloop)
      connection.__send__(:on_connect)
      @block.call(connection) if @block
    end
  end

  # TCP server class.  Listens on the specified host and port and creates
  # new connection objects of the given class. This is the most common server class.
  # Note that the new connection objects will be bound by default to the same event loop that the server is attached to.
  class TCPServer < Server
    def initialize(host, port, klass = TCPSocket, *args, &block)
      listen_socket = ::TCPServer.new(host, port)
      listen_socket.instance_eval { listen(1024) } # Change listen backlog to 1024
      super(listen_socket, klass, *args, &block)
    end
  end

  # UNIX server class.  Listens on the specified UNIX domain socket and
  # creates new connection objects of the given class.
  class UNIXServer < Server
    def initialize(path, klass = UNIXSocket, *args, &block)
      super(::UNIXServer.new(path), klass, *args, &block)
    end
  end
end
