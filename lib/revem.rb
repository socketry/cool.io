#--
# Copyright (C)2007 Tony Arcieri
# You can redistribute this under the terms of the Ruby license
# See file LICENSE for details
#++


# This file creates a an EventMachine class
# that is actually using rev as its underlying socket layer, but uses the EM API.
# require it instead of eventmachine
# i.e. 
# require 'revem'
# instead of
# require 'eventmachine'
# Note: you may want to do both a require 'eventmachine' THEN a require 'revem'
# so that eventmachine is loaded once, then rev overrides it, then there's no require confusion 
# (if simple servers hang, requiring 'eventmachine' after 'revem' could be the cause of it).
# drawbacks: slightly slower than EM.
# benefits: timers are more accurate using libev than using EM.  Also rev is sometimes more compatible than EM (ex: 1.9 windows)
# TODO: some things like connection timeouts aren't implemented yet
# DONE: timers and normal socket functions are implemented.
require File.dirname(__FILE__) + '/rev'

module EventMachine
  
  class << self
    # Start the Reactor loop
    def run
      yield if block_given?
      Rev::Loop.default.run
    end

    # Stop the Reactor loop
    def stop_event_loop
      Rev::Loop.default.stop
    end

    class OneShotEMTimer < Rev::TimerWatcher
      def setup(proc)
        @proc = proc
      end

      def on_timer
       @proc.call
      end
    end

    # ltodo: use Rev's PeriodicTimer to wrap EM's two similar to it
    # todo: close all connections on 'stop', I believe

    def add_timer interval, proc = nil, &block
      block ||= proc
      t = OneShotEMTimer.new(interval, false) # non repeating
      t.setup(block) 
      t.attach(Rev::Loop.default) # fire 'er off ltodo: do we keep track of these timers in memory?
      t
    end

    def cancel_timer t
     t.detach if t.attached? # guess there's a case where EM you can say 'cancel' but it's already fired? kind of odd but it happens
    end

    def set_comm_inactivity_timeout *args; end # TODO

    # Make an outgoing connection
    def connect(addr, port, handler = Connection, *args, &block)
	block = args.pop if Proc === args[-1]

       # make sure we're a 'real' class here
       klass = if (handler and handler.is_a?(Class))
         handler
       else
         Class.new( Connection ) {handler and include handler}
       end

      wrapped_child = CallsBackToEM.connect(addr, port, *args) # ltodo: args? what? they're used? also TODOC TODO FIX
      conn = klass.new(wrapped_child) # ltodo [?] addr, port, *args)
      wrapped_child.attach(Rev::Loop.default) # necessary
      conn.heres_your_socket(wrapped_child)
      wrapped_child.call_back_to_this(conn) # calls post_init for us
      yield conn if block_given?
    end
    
    # Start a TCP server on the given address and port
    def start_server(addr, port, handler = Connection, *args, &block)

       # make sure we're a 'real' class here
       klass = if (handler and handler.is_a?(Class))
         handler
       else
         Class.new( Connection ) {handler and include handler}
       end
      server = Rev::TCPServer.new(addr, port, CallsBackToEM, *args) { |wrapped_child| 
	conn = klass.new(wrapped_child)
	conn.heres_your_socket(wrapped_child) # ideally NOT have this :)
	wrapped_child.call_back_to_this(conn) 
	block.call(conn) if block
      }

      server.attach(Rev::Loop.default)
    end

    def stop_server server
	server.close
    end

    # Set the maximum number of descriptors available to this process
    def set_descriptor_table_size(nfds)
      Rev::Utils.maxfds = nfds
    end

    # Compatibility noop.  Handled automatically by libev
    def epoll; end

    # Compatibility noop.  Handled automatically by libev
    def kqueue; end
  end

  class CallsBackToEM < Rev::TCPSocket
    class ConnectTimer < Rev::TimerWatcher
      attr_accessor :parent
      def on_timer
       @parent.connection_has_timed_out
      end
    
    end

    def call_back_to_this parent
      @call_back_to_this = parent
      parent.post_init
    end

    def on_connect
      # @connection_timer.detach if @connection_timer # won't need that anymore :) -- with server connecteds we don't have it, anyway
      @call_back_to_this.connection_completed if @call_back_to_this # TODO should server accepted's call this? They don't currently [and can't, since on_connect gets called basically in the initializer--needs some code love for that to happen :)
    end

    def connection_has_timed_out
      return if closed?
      close unless closed? # wonder if this works when you're within a half-connected phase.  I think it does.  What about TCP state?
      @call_back_to_this.unbind
    end

    def on_write_complete; close if @should_close_after_writing; end

    def should_close_after_writing
      @should_close_after_writing = true;
    end

    def on_close
      @call_back_to_this.unbind # about the same ltodo check if they ARE the same here
    end

    def on_resolve_failed
      fail
    end

    def on_connect_failed
      fail
    end

    def on_read(data)
      @call_back_to_this.receive_data data
    end

    def fail
      #@connection_timer.detch if @connection_timer
      @call_back_to_this.unbind
    end

    def self.connect *args
      a = super *args
      # the connect timer currently kills TCPServer classes.  I'm not sure why.
      #@connection_timer = ConnectTimer.new(14) # needs to be at least higher than 12 :)
      #@connection_timer.parent = a
      #@connection_timer.attach(Rev::Loop.default)
      a
    end
  
  end

  class Connection
    def self.new *args
      allocate#.instance_eval do
      #  initialize *args
      #end
    end
    # we will need to call 'their functions' appropriately -- the commented out ones, here
    # 
    # Callback fired when connection is created
    def post_init; end # I thought we were 'overriding' EM's existing methods, here.  Huh? Why do we have to define these then?

    # Callback fired when connection is closed
    def unbind; end
    
    # Callback fired when data is received
    # def receive_data(data); end
    def heres_your_socket instantiated_rev_socket
      instantiated_rev_socket.call_back_to_this self
      @wrapped_rev = instantiated_rev_socket
    end

    # Send data to the current connection -- called by them
    def send_data(data)
      @wrapped_rev.write data
    end
    
    # Close the connection, optionally after writing
    def close_connection(after_writing = false)
      return close_connection_after_writing if after_writing
      @wrapped_rev.close
    end
    
    # Close the connection after all data has been written
    def close_connection_after_writing
      @wrapped_rev.output_buffer_size.zero? ? @wrapped_rev.close : @wrapped_rev.should_close_after_writing
    end

    def get_peername
        family, port, host_name, host_ip = @wrapped_rev.peeraddr
	Socket.pack_sockaddr_in( port, host_ip) # pack it up :)
    end
    
  end
end
