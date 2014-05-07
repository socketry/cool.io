#--
# Copyright (C)2010 Tony Arcieri
# You can redistribute this under the terms of the Ruby license
# See file LICENSE for details
#++

module Coolio
  # A module we stash all the connections defined by the DSL under
  module Connections; end

  # A DSL for defining Cool.io connection types and servers
  module DSL
    # Define all methods on the metaclass
    module_function

    # Run the default Cool.io event loop
    def run
      Cool.io::Loop.default.run
    end

    # Connect to the given host and port using the given connection class
    def connect(host, port, connection_name = nil, *initializer_args, &block)
      if block_given?
        initializer_args.unshift connection_name if connection_name

        klass = Class.new Cool.io::TCPSocket
        connection_builder = ConnectionBuilder.new klass
        connection_builder.instance_eval(&block)
      else
        raise ArgumentError, "no connection name or block given" unless connection_name
        klass = self[connection_name]
      end

      client = klass.connect host, port, *initializer_args
      client.attach Cool.io::Loop.default
      client
    end

    # Create a new Cool.io::TCPServer
    def server(host, port, connection_name = nil, *initializer_args, &block)
      if block_given?
        initializer_args.unshift connection_name if connection_name

        klass = Class.new Cool.io::TCPSocket
        connection_builder = ConnectionBuilder.new klass
        connection_builder.instance_eval(&block)
      else
        raise ArgumentError, "no connection name or block given" unless connection_name
        klass = self[connection_name]
      end

      server = Cool.io::TCPServer.new host, port, klass, *initializer_args
      server.attach Cool.io::Loop.default
      server
    end

    # Create a new Cool.io::TCPSocket class
    def connection(name, &block)
      # Camelize class name
      class_name = name.to_s.split('_').map { |s| s.capitalize }.join

      connection = Class.new Cool.io::TCPSocket
      connection_builder = ConnectionBuilder.new connection
      connection_builder.instance_eval(&block)

      Coolio::Connections.const_set class_name, connection
    end

    # Look up a connection class by its name
    def [](connection_name)
      class_name = connection_name.to_s.split('_').map { |s| s.capitalize }.join

      begin
        Coolio::Connections.const_get class_name
      rescue NameError
        raise NameError, "No connection type registered for #{connection_name.inspect}"
      end
    end

    # Builder for Cool.io::TCPSocket classes
    class ConnectionBuilder
      def initialize(klass)
        @klass = klass
      end

      # Declare an initialize function
      def initializer(&action)
        @klass.send :define_method, :initialize, &action
      end

      # Declare the on_connect callback
      def on_connect(&action)
        @klass.send :define_method, :on_connect, &action
      end

      # Declare a callback fired if we failed to connect
      def on_connect_failed(&action)
        @klass.send :define_method, :on_connect_failed, &action
      end

      # Declare a callback fired if DNS resolution failed
      def on_resolve_failed(&action)
        @klass.send :define_method, :on_resolve_failed, &action
      end

      # Declare the on_close callback
      def on_close(&action)
        @klass.send :define_method, :on_close, &action
      end

      # Declare the on_read callback
      def on_read(&action)
        @klass.send :define_method, :on_read, &action
      end

      # Declare the on_write_complete callback
      def on_write_complete(&action)
        @klass.send :define_method, :on_write_complete, &action
      end
    end
  end
end

# The Cool module containing all our coolness
module Cool
  module Coolness
    def cool
      Cool::IOThunk
    end
  end

  module IOThunk
    def self.io
      Coolio::DSL
    end
  end
end

extend Cool::Coolness
