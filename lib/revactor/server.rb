module Revactor
  # Revactor::Server wraps an Actor's receive loop and issues callbacks to
  # a class which implements Revactor::Behaviors::Server.  It eases the
  # creation of standard synchronous "blocking" calls by abstracting away 
  # inter-Actor communication and also providing baked-in state management.
  #
  # When used properly, Revactor::Server can implement transactional
  # semantics, ensuring only successful calls mutate the previous state
  # and erroneous/exception-raising ones do not.
  #
  # The design is modeled off Erlang/OTP's gen_server
  class Server
    # How long to wait for a response to a call before timing out
    # This value also borrowed from Erlang
    DEFAULT_CALL_TIMEOUT = 5
    
    def initialize(obj, *args)
      @obj = obj
      @actor = Actor.new(&method(:start).to_proc)
      
      @timeout = nil
      @state = obj.start(*args)
    end
    
    def call(message, options = {})
      options[:timeout] ||= DEFAULT_CALL_TIMEOUT
      
      @actor << [:call, Actor.current, message]
      Actor.receive do |filter|
        filter.when(proc {|m| m[0] == :call_reply and m[1] == @actor}) { |m| m[3] }
        filter.when(proc {|m| m[0] == :call_error and m[1] == @actor}) { |m| raise m[3] }
        filter.after(options[:timeout]) { raise RuntimeError, 'timeout' }
      end
    end
    
    def cast(message)
      @actor << [:cast, message]
    end
    
    def method_missing(message, *args)
      call(args.empty? ? message : [message, *args])
    end
    
    def start
      @running = true
      while @running do
        Actor.receive do |filter|
          filter.when(Actor::ANY) { |message| handle_message(message) }
          filter.after(@timeout) { stop(:timeout) } if @timeout
        end
      end
    end
    
    #########
    protected
    #########
    
    # Dispatch the incoming message to the appropriate handler
    def handle_message(message)
      case message.first
      when :call then handle_call(message)
      when :cast then handle_cast(message)
      else handle_info(message)
      end
    end
    
    # Wrapper for calling the provided object's handle_call method
    def handle_call(message)
      _, from, body = message
      
      begin
        result = @obj.handle_call(body, from, @state)
        case result.first
        when :reply
          _, reply, @state, @timeout = result
          from << [:call_reply, Actor.current, reply]
        when :noreply
          _, @state, @timeout = result
        when :stop
          _, reason, @state = result
          stop(reason)
        end
      rescue Exception => e
        log_exception(e)
        from << [:call_error, Actor.current, e]
      end
    end
    
    # Wrapper for calling the provided object's handle_cast method
    def handle_cast(message)
      _, body = message
    
      begin
        result = @obj.handle_cast(body, @state)
        case result.first
        when :noreply
          _, @state, @timeout = result
        when :stop
          _, reason, @state = result
          stop(reason)
        end
      rescue Exception => e
        log_exception(e)
      end
    end
    
    # Wrapper for calling the provided object's handle_info method
    def handle_info(message)
      begin
        result = @obj.handle_info(message, @state)
        case result.first
        when :noreply
          _, @state, @timeout = result
        when :stop
          _, reason, @state = result
          stop(reason)
        end
      rescue Exception => e
        log_exception(e)
      end
    end
    
    # Stop the server
    def stop(reason)
      @running = false
      @obj.terminate(reason, @state)
    end
    
    # Log an exception
    # FIXME this should really go to a logger, not STDERR
    def log_exception(exception)
      STDERR.write "Rev::Server exception: #{exception}\n"
      STDERR.write exception.backtrace + "\n"
    end
  end
end