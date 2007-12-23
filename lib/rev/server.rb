require File.dirname(__FILE__) + '/../rev'

module Rev
  class Server < Listener
    def initialize(listen_socket, klass = Socket, *args, &block)
      # Ensure the provided class responds to attach
      unless (klass.instance_method(:attach) rescue nil)
        raise ArgumentError, "provided class must respond to 'attach'"
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
      connection.on_connect
      @block.(connection) if @block
    end
  end

  class TCPServer < Server
    def initialize(host, port, klass = TCPSocket, *args, &block)
      listen_socket = ::TCPServer.new(host, port)
      listen_socket.instance_eval { listen(1024) } # Change listen backlog to 1024
      super(listen_socket, klass, *args, &block)
    end
  end

  class UNIXServer < Server
    def initialize(path, klass = UNIXSocket, *args, &block)
      super(::UNIXServer.new(*args), klass, *args, &block)
    end
  end
end
