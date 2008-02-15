#--
# Copyright (C)2007 Tony Arcieri
# You can redistribute this under the terms of the Ruby license
# See file LICENSE for details
#++

require 'openssl'
require File.dirname(__FILE__) + '/../rev'

module Rev
  # Monkeypatch Rev::IO to include SSL support.  This can be accomplished
  # by extending any Rev:IO (or subclass) object with Rev::SSL after the
  # connection has completed, e.g.
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
      raise "ssl already started" if @ssl_socket
      
      ssl_context = respond_to?(:ssl_context) ? ssl_context : OpenSSL::SSL::SSLContext.new
      @ssl_socket = SSL::IO.new(@io, ssl_context)
      @ssl_init = proc { @ssl_socket.connect_nonblock }
      
      ssl_init
    end
    
    # Start SSL explicitly in server mode. After calling this, callbacks
    # will fire for checking the peer certificate (ssl_peer_cert) and
    # its validity (ssl_verify_result)
    def ssl_server_start
      raise "ssl already started" if @ssl_socket
      
      @ssl_socket = SSL::IO.new(@io, ssl_context)
      @ssl_init = proc { @ssl_socket.accept_nonblock }
      
      ssl_init
    end
    
    #########
    protected
    #########
    
    def ssl_init
      begin
        @ssl_init.()
        ssl_init_complete
      rescue SSL::IO::ReadAgain
        enable unless enabled?
      rescue SSL::IO::WriteAgain
        enable_write_watcher
      rescue OpenSSL::SSL::SSLError, Errno::ECONNRESET
        close
      rescue => ex
        if respond_to? :on_ssl_error
          on_ssl_error(ex)
        else raise ex
        end
      end
    end
    
    def ssl_init_complete
      @ssl_init = nil
      enable unless enabled?
      
      on_peer_cert(@ssl_socket.peer_cert) if respond_to? :on_peer_cert
      on_ssl_result(@ssl_socket.verify_result) if respond_to? :on_ssl_result
      on_ssl_connect if respond_to? :on_ssl_connect
    end
    
    def on_readable
      if @ssl_init
        disable
        ssl_init
        return
      end
      
      begin
        on_read @ssl_socket.read_nonblock(Rev::IO::INPUT_SIZE)
      rescue Errno::AGAIN, SSL::IO::ReadAgain
        return
      rescue OpenSSL::SSL::SSLError, Errno::ECONNRESET, EOFError
        close
      end
    end
    
    def on_writable
      if @ssl_init
        disable_write_watcher
        ssl_init
        return
      end
      
      begin
        nbytes = @ssl_socket.write_nonblock @write_buffer.to_str
      rescue Errno::EAGAIN, SSL::IO::WriteAgain
        return
      rescue OpenSSL::SSL::SSLError, Errno::EPIPE, Errno::ECONNRESET
        close
        return
      end
      
      @write_buffer.read(nbytes)
      
      if @write_buffer.empty?
        disable_write_watcher
        on_write_complete
      end
    end
  end
end