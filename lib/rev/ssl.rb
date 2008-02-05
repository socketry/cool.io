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
  #       ssl_start
  #     end
  #   end
  #
  module SSL
    # Obtain an OpenSSL::SSL::SSLContext to initialize the socket with
    # Load the SSLContext with your desired certificates if you wish to
    # verify the certificate of the remote server.
    def ssl_context
      OpenSSL::SSL::SSLContext.new
    end
    
    # Start SSL explicitly.  This isn't required, but allows you to do
    # things like verify certificates.  After calling this, callbacks
    # will fire for checking the peer certificate (ssl_peer_cert) and
    # its validity (ssl_verify_result)
    def ssl_start
      raise "ssl already started" if @ssl_socket
      
      @ssl_socket = OpenSSL::SSL::SSLSocket.new(@io, ssl_context).connect
      ssl_peer_cert(@ssl_socket.peer_cert)
      ssl_verify_result(@ssl_socket.verify_result)
    end
    
    # Callback for checking the peer certificate directly.
    # Equivalent to OpenSSL::SSL::SSLSocket#peer_cert
    def ssl_peer_cert(cert)
    end
    
    # Call for verifying the validity of the peer certificate.
    # Equivalent to OpenSSL::SSL::SSLSocket#verify_result
    def ssl_verify_result(result)
    end
    
    #########
    protected
    #########
    
    def on_readable
      begin
        on_read ssl_socket.sysread(IO::INPUT_SIZE)
      rescue Errno::ECONNRESET, EOFError
        close
      end
    end
    
    def write_output_buffer
      begin
        nbytes = ssl_socket.syswrite @write_buffer.to_str
      rescue Errno::EPIPE
        close
      end
      
      @write_buffer.read(nbytes)
      
      if @write_buffer.empty?
        @writer.disable if @writer and @writer.enabled?
        on_write_complete
      end
    end
    
    def ssl_socket
      @ssl_socket ||= OpenSSL::SSL::SSLSocket.new(@io, ssl_context).connect
    end
  end
end
