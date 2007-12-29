#--
# Copyright (C)2007 Tony Arcieri
# Includes portions originally Copyright (C)2005 Zed Shaw
# You can redistribute this under the terms of the Ruby license
# See file LICENSE for details
#++

require File.dirname(__FILE__) + '/../rev'
require File.dirname(__FILE__) + '/../http11_client'

module Rev
  class HTTPClient < TCPSocket
    HTTP_REQUEST_HEADER="%s %s HTTP/1.1\r\n"
    FIELD_ENCODING = "%s: %s\r\n"
    CRLF = "\r\n"
    
    def initialize(socket)
      super
      
      @parser = HttpClientParser.new
      @requested = false
    end
    
    def request(method, uri, options = {})
      raise RuntimeError, "request already sent" if @requested
      
      @allowed_methods = options[:allowed_methods] || [:put, :get, :post, :delete, :head]
      raise ArgumentError, "method not supported" unless @allowed_methods.include? method.to_sym
      
      @method, @uri, @options = method, uri, options
      @requested = true
      
      return unless @connected
      send_request
    end
    
    #########
    protected
    #########
    
    def on_connect
      @connected = true
      send_request if @method and @uri
    end
    
    def on_read(data)
      puts data
    end
    
    def send_request
      query   = @options[:query]
      head    = @options[:head] ? munge_header_keys(@options[:head]) : {}
      cookies = @options[:cookies]
      body    = @options[:body]
      
      # Set the Host header if it hasn't been specified already
      head['host'] ||= encode_host
      
      # Set the Content-Length if it hasn't been specified already and a body was given
      head['content-length'] ||= body ? body.length : 0
      
      # Set the User-Agent if it hasn't been specified
      head['user-agent'] ||= "Rev #{Rev::VERSION}"
      
      # Build the request
      request = encode_request(@method, @uri, query)
      request << encode_headers(head)
      request << encode_cookies(cookies) if cookies
      request << CRLF
      request << body if body
      
      write request
    end
    
    # Escapes a URI.
    def escape(s)
      s.to_s.gsub(/([^ a-zA-Z0-9_.-]+)/n) {
        '%'+$1.unpack('H2'*$1.size).join('%').upcase
      }.tr(' ', '+') 
    end

    # Unescapes a URI escaped string.
    def unescape(s)
      s.tr('+', ' ').gsub(/((?:%[0-9a-fA-F]{2})+)/n){
        [$1.delete('%')].pack('H*')
      } 
    end
    
    # Map all header keys to a downcased string version
    def munge_header_keys(head)
      head.reduce({}) { |h, (k, v)| h[k.to_s.downcase] = v; h }
    end
    
    # HTTP is kind of retarded that you have to specify
    # a Host header, but if you include port 80 then further
    # redirects will tack on the :80 which is annoying.
    def encode_host
      remote_host + (remote_port.to_i != 80 ? ":#{remote_port}" : "")
    end
    
    def encode_request(method, uri, query)
      HTTP_REQUEST_HEADER % [method.to_s.upcase, encode_query(uri, query)]
    end
    
    def encode_query(uri, query)
      return uri unless query
      uri + "?" + query.map { |k, v| encode_param(k, v) }.join('&')
    end
    
    # URL encodes a single k=v parameter.
    def encode_param(k, v)
      escape(k) + "=" + escape(v)
    end
    
    # Encode a field in an HTTP header
    def encode_field(k, v)
      FIELD_ENCODING % [k, v]
    end
    
    def encode_headers(head)
      head.reduce('') do |result, (k, v)|
        # Munge keys from foo-bar-baz to Foo-Bar-Baz
        k = k.split('-').map(&:capitalize).join('-')
        result << encode_field(k, v)
      end
    end
    
    def encode_cookies(cookies)
      cookies.reduce('') { |result, (k, v)| result << encode_field('Cookie', encode_param(k, v)) }
    end
  end
end