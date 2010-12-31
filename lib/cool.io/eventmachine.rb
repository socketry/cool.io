#--
# Copyright (C)2007-10 Tony Arcieri, Roger Pack
# You can redistribute this under the terms of the Ruby license
# See file LICENSE for details
#++

require 'cool.io'

# EventMachine emulation for Cool.io:
#
#   require 'coolio/eventmachine'
#
# Drawbacks: slightly slower than EM.
# Benefits: timers are more accurate using libev than using EM
# TODO: some things like connection timeouts aren't implemented yet
# DONE: timers and normal socket functions are implemented.
module EventMachine
  class << self
    # Start the Reactor loop
    def run
      yield if block_given?
      Coolio::Loop.default.run
    end

    # Stop the Reactor loop
    def stop_event_loop
      Coolio::Loop.default.stop
    end

    class OneShotEMTimer < Coolio::TimerWatcher
      def setup(proc)
        @proc = proc
      end

      def on_timer
       @proc.call
      end
    end

    # ltodo: use Coolio's PeriodicTimer to wrap EM's two similar to it
    # todo: close all connections on 'stop', I believe

    def add_timer(interval, proc = nil, &block)
      block ||= proc
      t = OneShotEMTimer.new(interval, false) # non repeating
      t.setup(block)

      # fire 'er off ltodo: do we keep track of these timers in memory?
      t.attach(Coolio::Loop.default)
      t
    end

    def cancel_timer(t)
      # guess there's a case where EM you can say 'cancel' but it's already fired?
      # kind of odd but it happens
     t.detach if t.attached?
    end

    def set_comm_inactivity_timeout(*args); end # TODO

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
      wrapped_child.attach(Coolio::Loop.default) # necessary
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

      server = Coolio::TCPServer.new(addr, port, CallsBackToEM, *args) do |wrapped_child|
        conn = klass.new(wrapped_child)
        conn.heres_your_socket(wrapped_child) # ideally NOT have this :)
        wrapped_child.call_back_to_this(conn)
        block.call(conn) if block
      end

      server.attach(Coolio::Loop.default)
    end

    def stop_server(server)
      server.close
    end

    # Set the maximum number of descriptors available to this process
    def set_descriptor_table_size(nfds)
      Coolio::Utils.maxfds = nfds
    end

    # Compatibility noop.  Handled automatically by libev
    def epoll; end

    # Compatibility noop.  Handled automatically by libev
    def kqueue; end
  end

  class CallsBackToEM < Coolio::TCPSocket
    class ConnectTimer < Coolio::TimerWatcher
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
      # @connection_timer.detach if @connection_timer
      # won't need that anymore :) -- with server connecteds we don't have it, anyway

      # TODO should server accepted's call this? They don't currently
      # [and can't, since on_connect gets called basically in the initializer--needs some code love for that to happen :)
      @call_back_to_this.connection_completed if @call_back_to_this
    end

    def connection_has_timed_out
      return if closed?

      # wonder if this works when you're within a half-connected phase.
      # I think it does.  What about TCP state?
      close unless closed?
      @call_back_to_this.unbind
    end

    def on_write_complete
      close if @should_close_after_writing
    end

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

    def self.connect(*args)
      a = super *args
      # the connect timer currently kills TCPServer classes.  I'm not sure why.
      #@connection_timer = ConnectTimer.new(14) # needs to be at least higher than 12 :)
      #@connection_timer.parent = a
      #@connection_timer.attach(Coolio::Loop.default)
      a
    end
  end

  class Connection
    def self.new(*args)
      allocate#.instance_eval do
      #  initialize *args
      #end
    end

    # we will need to call 'their functions' appropriately -- the commented out ones, here
    #
    # Callback fired when connection is created
    def post_init
      # I thought we were 'overriding' EM's existing methods, here.
      # Huh? Why do we have to define these then?
    end

    # Callback fired when connection is closed
    def unbind; end

    # Callback fired when data is received
    # def receive_data(data); end
    def heres_your_socket(instantiated_coolio_socket)
      instantiated_coolio_socket.call_back_to_this self
      @wrapped_coolio = instantiated_coolio_socket
    end

    # Send data to the current connection -- called by them
    def send_data(data)
      @wrapped_coolio.write data
    end

    # Close the connection, optionally after writing
    def close_connection(after_writing = false)
      return close_connection_after_writing if after_writing
      @wrapped_coolio.close
    end

    # Close the connection after all data has been written
    def close_connection_after_writing
      @wrapped_coolio.output_buffer_size.zero? ? @wrapped_coolio.close : @wrapped_coolio.should_close_after_writing
    end

    def get_peername
      family, port, host_name, host_ip = @wrapped_coolio.peeraddr
      Socket.pack_sockaddr_in(port, host_ip) # pack it up :)
    end
  end
end

# Shortcut constant
EM = EventMachine
