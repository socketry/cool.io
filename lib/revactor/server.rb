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
    def initialize(obj, *args)
      @obj = obj
      @actor = Actor.new(&method(:start).to_proc)
      
      @timeout = nil
      @state = obj.start(*args)
    end
    
    def start(*args)
      @running = true
      while @running do
        Actor.receive do |filter|
          filter.when(Actor::ANY) { |message| handle_message(message) }
          filter.after(@timeout) { handle_timeout }
        end
      end
    end
    
    #########
    protected
    #########
    
    def handle_message(message)
    end
    
    def handle_timeout
    end
  end
end