require File.dirname(__FILE__) + '/../rev'

module Rev
  class Server
    def initialize(listener)
      raise ArgumentError, "no listener provided" unless listener.is_a? Listener
      @listener = listener
    end

    def attach(evloop, klass = BufferedIO, *args)
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

      @listener.attach(evloop) { |socket| klass.new(socket, *args).attach(evloop) }
    end
  end

  class TCPServer < Server
    def initialize(*args)
      super(TCPListener.new(*args))
    end
  end

  class UNIXServer < Server
    def initialize(*args)
      super(UNIXListener.new(*args))
    end
  end
end
