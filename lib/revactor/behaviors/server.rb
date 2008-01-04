module Revactor
  module Behavior
    # The Server behavior provides a callback-driven class which eases the
    # creation of standard synchronous "blocking" calls by abstracting away 
    # inter-Actor communication and also providing baked-in state management.
    #
    # This behavior module provides the base set of callbacks necessary
    # to implement the behavior.  It also provides descriptions for what
    # certain callback methods should do.
    #
    # When used properly, the server behavior can implement transactional
    # semantics, ensuring only successful calls mutate the previous state
    # and erroneous/exception-raising ones do not.
    #
    # The design is modeled off Erlang/OTP's gen_server
    module Server
      # Initialize the server state.  Can return:
      #
      #   start(*args) 
      #     -> Tuple[:ok, state]
      #     -> Tuple[:ok, state, timeout]
      #     -> Tuple[:stop, reason]
      #
      # The state variable allows you to provide a set of state whose mutation
      # can be controlled a lot more closely than is possible with standard
      # object oriented behavior.  The latest version of state is passed
      # to all Revactor::Server callbacks and is only mutated upon a
      # successful return (without exceptions)
      #
      def start(*args)
        return :ok
      end

      # Handle any calls made to a Reactor::Server object, which are captured
      # via method_missing and dispatched here.  Calls provide synchronous
      # behavior: the callee will block until this method completss and a
      # reply is sent back to them.  Can return:
      #
      #   handle_call(state, from, message, *args)
      #     -> [:reply, reply, new_state]
      #     -> [:reply, reply, new_state, timeout]
      #     -> [:noreply, new_state]
      #     -> [:noreply, new_state, timeout]
      #     -> [:stop, reason, reply, state]
      #
      def handle_call(message, from, state)
        return :reply, :ok, state
      end

      # Handle calls without return values
      #
      #   handle_cast(message, state)
      #     -> [:noreply, new_state]
      #     -> [:noreply, new_state, timeout]
      #     -> [:stop, reason, state]
      #
      def handle_cast(message, state)
        return :noreply, state
      end

      # Handle any spontaneous messages to the server which are not calls
      # or casts made from Rev::Server.  Can return:
      #
      #   handle_info(info, state)
      #     -> [:noreply, new_state]
      #     -> [:noreply, new_state, timeout]
      #     -> [:stop, reason, state]
      #
      def handle_info(info, state)
        return :noreply, state
      end

      # Method called when the server is about to terminate, for example when 
      # any of the handle_* routines above return :stop.  The return value of
      # terminate is discarded.
      #
      def terminate(reason, state)
      end
    end
  end
end
