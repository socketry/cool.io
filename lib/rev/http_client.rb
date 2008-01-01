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
  class HttpResponseHeader < Hash
    # The reason returned in the http response ("OK","File not found",etc.)
    attr_accessor :http_reason

    # The HTTP version returned.
    attr_accessor :http_version

    # The status code (as a string!)
    attr_accessor :http_status

    # HTTP response status as an integer
    def status
      Integer(http_status) rescue nil
    end

    # Length of content as an integer, or nil if chunked/unspecified
    def content_length
      Integer(self[HttpClient::CONTENT_LENGTH]) rescue nil
    end
 
    # Is the transfer encoding chunked?
    def chunked_encoding?
      /chunked/i === self[HttpClient::TRANSFER_ENCODING]
    end
 end

  class HttpChunkHeader < Hash
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

  module HttpEncoding
    HTTP_REQUEST_HEADER="%s %s HTTP/1.1\r\n"
    FIELD_ENCODING = "%s: %s\r\n"

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

  class HttpClient < TCPSocket
    include HttpEncoding

    TRANSFER_ENCODING="TRANSFER_ENCODING"
    CONTENT_LENGTH="CONTENT_LENGTH"
    SET_COOKIE="SET_COOKIE"
    LOCATION="LOCATION"
    HOST="HOST"
    CRLF="\r\n"

    def self.connect(addr, port = 80, *args)
      super
    end

    def initialize(socket)
      super

      @parser = HttpClientParser.new
      @parser_nbytes = 0

      @state = :response_header
      @data = ''

      @response_header = HttpResponseHeader.new
      @chunk_header = HttpChunkHeader.new
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
    def on_response_header(response_header)
    end

    # Called when part of the body has been read
    def on_body_data(data)
      STDOUT.write data
      STDOUT.flush
    end

    # Called when the request has completed
    def on_request_complete
      close
    end

    # Called when an error occurs during the request
    def on_error(reason)
      raise RuntimeError, reason
    end

    #########
    protected
    #########

    #
    # Request sending
    #

    def send_request
      send_request_header
      send_request_body
    end

    def send_request_header
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
      request_header = encode_request(@method, @uri, query)
      request_header << encode_headers(head)
      request_header << encode_cookies(cookies) if cookies
      request_header << CRLF

      write request_header
    end

    def send_request_body
      write @options[:body] if @options[:body]
    end  
 
    #
    # Rev callbacks
    #
    
    def on_connect
      @connected = true
      send_request if @method and @uri
    end

    def on_read(data)
      until @state == :finished or @state == :invalid or data.empty?
        @state, data = dispatch_data(@state, data)
      end
    end

    #
    # Response processing
    #

    def dispatch_data(state, data)
      case state
      when :response_header then parse_response_header(data)
      when :chunk_header then parse_chunk_header(data)
      when :chunk_body then process_chunk_body(data)
      when :chunk_footer then process_chunk_footer(data)
      when :response_footer then process_response_footer(data)
      when :body then process_body(data)
      when :finished
      when :invalid
      else raise RuntimeError, "invalid state: #{@state}"
      end
    end

    def parse_header(header, data)
      @data << data
      @parser_nbytes = @parser.execute(header, @data, @parser_nbytes)
      return unless @parser.finished?

      remainder = @data.slice(@parser_nbytes, @data.size)
      @data = ''
      @parser.reset
      @parser_nbytes = 0

      remainder
    end

    def parse_response_header(data)
      data = parse_header(@response_header, data)
      return :response_header, '' if data.nil?

      unless @response_header.http_status and @response_header.http_reason
        on_error "no HTTP response"
        return :invalid
      end

      on_response_header(@response_header)

      if @response_header.chunked_encoding?
        return :chunk_header, data
      else
        @bytes_remaining = @response_header.content_length
        return :body, data
      end
    end

    def parse_chunk_header(data)
      data = parse_header(@chunk_header, data)
      return :chunk_header, '' if data.nil?

      @bytes_remaining = @chunk_header.chunk_size
      @chunk_header = HttpChunkHeader.new

      if @bytes_remaining > 0
        return :chunk_body, data
      else
        @bytes_remaining = 2
        return :response_footer, data
      end
    end

    def process_chunk_body(data)
      if data.size < @bytes_remaining
        @bytes_remaining -= data.size
        on_body_data data
        return :chunk_body, ''
      end

      on_body_data data.slice!(0, @bytes_remaining)
      @bytes_remaining = 2
      return :chunk_footer, data
    end

    def process_crlf(data)
      @data << data.slice!(0, @bytes_remaining)
      @bytes_remaining = 2 - @data.size
      return unless @bytes_remaining == 0

      matches_crlf = (@data == CRLF)
      @data = ''

      return matches_crlf, data
    end

    def process_chunk_footer(data)
      result, data = process_crlf(data)
      return :chunk_footer, '' if result.nil?

      if result
        return :chunk_header, data
      else
        on_error "non-CRLF chunk footer"
        return :invalid
      end
    end

    def process_response_footer(data)
      result, data = process_crlf(data)
      return :response_footer, '' if result.nil?
      if result
        unless data.empty?
          on_error "garbage at end of chunked response"
          return :invalid
        end

        on_request_complete
        return :finished
      else
        on_error "non-CRLF response footer"
        return :invalid
      end
    end

    def process_body(data)
      if data.size < @bytes_remaining
        @bytes_remaining -= data.size
        on_body_data data
        return :body, ''
      end

      on_body_data data.slice!(0, @bytes_remaining)

      unless data.empty?
        on_error "garbage at end of body"
        return :invalid
      end

      on_request_complete
      return :finished
    end
  end
end
