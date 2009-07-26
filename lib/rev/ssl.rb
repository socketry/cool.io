#--
# Copyright (C)2007 Tony Arcieri
# You can redistribute this under the terms of the Ruby license
# See file LICENSE for details
#++

require 'openssl'

module Rev
  # The easiest way to add SSL support to your Rev applications is to use
  # the SSLSocket class.  However, the SSL module is provided for cases where
  # you've already subclassed TCPSocket and want to optionally provide
  # SSL support in that class.
  #
  # This module monkeypatches Rev::IO to include SSL support.  This can be 
  # accomplished by extending any Rev:IO (or subclass) object with Rev::SSL 
  # after the connection has completed, e.g.
  #
  #   class MySocket < Rev::TCPSocket
  #     def on_connect
  #       extend Rev::SSL
  #       ssl_client_start
  #     end
  #   end
  # 
  module SSL
    # Start SSL explicitly in client mode.  After calling this, callbacks
    # will fire for checking the peer certificate (ssl_peer_cert) and
    # its validity (ssl_verify_result)
    def ssl_client_start
      raise "ssl already started" if @_ssl_socket
      
      context = respond_to?(:ssl_context) ? ssl_context : OpenSSL::SSL::SSLContext.new
      
      @_ssl_socket = SSL::IO.new(@_io, context)
      @_ssl_init = proc { @_ssl_socket.connect_nonblock }
      
      ssl_init
    end
    
    # Start SSL explicitly in server mode. After calling this, callbacks
    # will fire for checking the peer certificate (ssl_peer_cert) and
    # its validity (ssl_verify_result)
    def ssl_server_start
      raise "ssl already started" if @_ssl_socket
      
      @_ssl_socket = SSL::IO.new(@_io, ssl_context)
      @_ssl_init = proc { @_ssl_socket.accept_nonblock }
      
      ssl_init
    end
    
    #########
    protected
    #########
    
    def ssl_init
      begin
        @_ssl_init.call
        ssl_init_complete
      rescue SSL::IO::ReadAgain
        enable unless enabled?
      rescue SSL::IO::WriteAgain
        enable_write_watcher
      rescue OpenSSL::SSL::SSLError, Errno::ECONNRESET, Errno::EPIPE
        close
      rescue => ex
        if respond_to? :on_ssl_error
          on_ssl_error(ex)
        else raise ex
        end
      end
    end
    
    def ssl_init_complete
      @_ssl_init = nil
      enable unless enabled?
      
      on_peer_cert(@_ssl_socket.peer_cert) if respond_to? :on_peer_cert
      # FIXME Rev::SSL::IO#verify_result needs to be adapted to non-blocking
      #on_ssl_result(@_ssl_socket.verify_result) if respond_to? :on_ssl_result
      on_ssl_connect if respond_to? :on_ssl_connect
    end
    
    def on_readable
      if @_ssl_init
        disable
        ssl_init
        return
      end
      
      begin
        on_read @_ssl_socket.read_nonblock(Rev::IO::INPUT_SIZE)
      rescue Errno::EAGAIN, SSL::IO::ReadAgain
        return
      rescue OpenSSL::SSL::SSLError, Errno::ECONNRESET, EOFError
        close
      end
    end
    
    def on_writable
      if @_ssl_init
        disable_write_watcher
        ssl_init
        return
      end
      
      begin
        nbytes = @_ssl_socket.write_nonblock @_write_buffer.to_str
      rescue Errno::EAGAIN, SSL::IO::WriteAgain
        return
      rescue OpenSSL::SSL::SSLError, Errno::EPIPE, Errno::ECONNRESET
        close
        return
      end
      
      @_write_buffer.read(nbytes)
      
      if @_write_buffer.empty?
        disable_write_watcher
        on_write_complete
      end
    end
  end
  
  # A socket class for SSL connections.  Please note that this class 
  # internally uses the on_connect callback for doing SSL setup.  If
  # you would like a callback when the SSL connection is completed,
  # please use the on_ssl_connect callback instead.  If you really need
  # a callback which fires before SSL setup begins, use on_connect but
  # be sure to call super.
  class SSLSocket < TCPSocket
    # Perform a non-blocking connect to the given host and port
    def self.connect(addr, port, *args)
      sock = super
      sock.instance_variable_set(:@_connecting, true)
      sock
    end
    
    # Returns the OpenSSL::SSL::SSLContext for to use for the session.
    # By default no certificates will be checked.  If you would like 
    # any certificate checking to be performed, please override this 
    # method and return a context loaded with the appropriate certificates.
    def ssl_context
      @_ssl_context ||= OpenSSL::SSL::SSLContext.new
    end
    
    # Called when SSL handshaking has successfully completed
    def on_ssl_connect; end
    event_callback :on_ssl_connect
    
    # Called when peer certificate has successfully been received.
    # Equivalent to OpenSSL::SSL::SSLSocket#peer_cert
    def on_peer_cert(peer_cert); end
    event_callback :on_peer_cert
    
    # Called when SSL handshaking has been completed successfully.
    # Equivalent to OpenSSL::SSL::SSLSocket#verify_result
    def on_ssl_result(result);  end
    event_callback :on_ssl_result
    
    # Called if an error occurs during SSL session initialization
    def on_ssl_error(exception); end
    event_callback :on_ssl_error
    
    #########
    protected
    #########
    
    def on_connect
      extend SSL
      @_connecting ? ssl_client_start : ssl_server_start
    end
  end
end
