#--
# Copyright (C)2007 Tony Arcieri
# Includes portions originally Copyright (C)2005 Zed Shaw
# You can redistribute this under the terms of the Ruby license
# See file LICENSE for details
#++

require File.dirname(__FILE__) + '/../rev'
require File.dirname(__FILE__) + '/../http11_client'

module Rev
  # A simple hash is returned for each request made by HttpClient with
  # the headers that were given by the server for that request.
  class HttpResponse < Hash
    # The reason returned in the http response ("OK","File not found",etc.)
    attr_accessor :http_reason

    # The HTTP version returned.
    attr_accessor :http_version

    # The status code (as a string!)
    attr_accessor :http_status

    # When parsing chunked encodings this is set
    attr_accessor :http_chunk_size

    def chunk_size
      return @chunk_size unless @chunk_size.nil?
      @chunk_size = @http_chunk_size ? @http_chunk_size.to_i(base=16) : 0
    end

    def last_chunk?
      chunk_size == 0
    end
  end

  class HttpClient < TCPSocket
    TRANSFER_ENCODING="TRANSFER_ENCODING"
    CONTENT_LENGTH="CONTENT_LENGTH"
    SET_COOKIE="SET_COOKIE"
    LOCATION="LOCATION"
    HOST="HOST"
    HTTP_REQUEST_HEADER="%s %s HTTP/1.1\r\n"
    FIELD_ENCODING = "%s: %s\r\n"
    REQ_CONTENT_LENGTH="Content-Length"
    REQ_HOST="Host"
    CHUNK_SIZE=1024 * 16
    CRLF="\r\n"

    def self.connect(addr, port = 80, *args)
      super
    end

    def initialize(socket)
      super
      
      @parser = HttpClientParser.new
      @parser_nbytes = 0

      @header_data = ''
      @header_parsed = false
      @response = HttpResponse.new

      @chunk_header_data = ''
      @chunk_header_parsed = false
      @chunk_header = HttpResponse.new
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

    # Called when response header has been received
    def on_response_header(response)
      puts response.http_reason, response.http_version
      puts response.http_status, response.inspect
      puts chunked_encoding?.to_s
    end

    # Called when part of the body has been read
    def on_body_data(data)
      puts data
    end

    # Called when the request has completed
    def on_request_complete
    end
    
    #########
    protected
    #########
    
    def on_connect
      @connected = true
      send_request if @method and @uri
    end
    
    def on_read(data)
      return parse_response_header(data) unless @header_parsed
      decode_body(data)
    end

    def parse_response_header(data)
      @header_data << data
      @parser_nbytes = @parser.execute(@response, @header_data, @parser_nbytes)
      return unless @parser.finished?

      @header_parsed = true
      process_response_header

      # The remaining data is part of the body, so process it as such
      @header_data.slice!(0, @parser_nbytes)
      @parser_nbytes = 0
      @parser.reset

      decode_body(@header_data)
      @header_data = ''
    end

    def process_response_header
      on_response_header(@response)
    end

    def chunked_encoding?
      /chunked/i === @response[TRANSFER_ENCODING]
    end

    def decode_body(data)
      return on_body_data(data) unless chunked_encoding?
      return parse_chunk_header(data) unless @chunk_header_parsed
      return if @chunk_remaining.zero?

      if data.size < @chunk_remaining
        @chunk_remaining -= data.size
        return on_body_data data
      end

      on_body_data data.slice!(0, @chunk_remaining)
      @chunk_header_parsed = false
      
      parse_chunk_header data
    end

    # This is really the same as parse_response_header and should be DRYed out
    def parse_chunk_header(data)
      @chunk_header_data << data
      @parser_nbytes = @parser.execute(@chunk_header, @chunk_header_data, @parser_nbytes)
      return unless @parser.finished?

      @chunk_header_parsed = true
      @chunk_remaining = @chunk_header.chunk_size

      @chunk_header_data.slice!(0, @parser_nbytes)
      @parser_nbytes = 0
      @parser.reset

      decode_body(@chunk_header_data)
      @chunk_header_data = ''
      @chunk_header = HttpResponse.new
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

      # Default to Connection: close
      head['connection'] ||= 'close'
      
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
