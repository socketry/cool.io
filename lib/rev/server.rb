require File.dirname(__FILE__) + '/../rev'

module Rev
  class Server
    def initialize(listener, klass = Socket, *args)
      raise TypeError, "can't convert #{listener.class} into Rev::Listener" unless listener.is_a? Listener
      
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
      
      @listener, @klass, @args = listener, klass, args
    end

    def attach(evloop)
      @listener.attach(evloop) { |socket| @klass.new(socket, *@args).attach(evloop).on_connect }
    end
  end

  class TCPServer < Server
    def initialize(host, port, klass = TCPSocket, *args)
      super(TCPListener.new(host, port), klass, *args)
    end
  end

  class UNIXServer < Server
    def initialize(path, klass = UNIXSocket, *args)
      super(UNIXListener.new(path), klass, *args)
    end
  end
end
