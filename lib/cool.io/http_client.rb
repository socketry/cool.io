#--
# Copyright (C)2007-10 Tony Arcieri
# Includes portions originally Copyright (C)2005 Zed Shaw
# You can redistribute this under the terms of the Ruby license
# See file LICENSE for details
#++

require 'http11_client'

module Coolio
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

    # Size of the chunk as an integer
    def chunk_size
      return @chunk_size unless @chunk_size.nil?
      @chunk_size = @http_chunk_size ? @http_chunk_size.to_i(base=16) : 0
    end
  end

  # Methods for building HTTP requests
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
      head.inject({}) { |h, (k, v)| h[k.to_s.downcase] = v; h }
    end

    # HTTP is kind of retarded that you have to specify
    # a Host header, but if you include port 80 then further
    # redirects will tack on the :80 which is annoying.
    def encode_host
      remote_host + (remote_port.to_i != 80 ? ":#{remote_port}" : "")
    end

    def encode_request(method, path, query)
      HTTP_REQUEST_HEADER % [method.to_s.upcase, encode_query(path, query)]
    end

    def encode_query(path, query)
      return path unless query
      path + "?" + query.map { |k, v| encode_param(k, v) }.join('&')
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
      head.inject('') do |result, (key, value)|
        # Munge keys from foo-bar-baz to Foo-Bar-Baz
        key = key.split('-').map { |k| k.capitalize }.join('-')
      result << encode_field(key, value)
      end
    end

    def encode_cookies(cookies)
      cookies.inject('') { |result, (k, v)| result << encode_field('Cookie', encode_param(k, v)) }
    end
  end

  # HTTP client class implemented as a subclass of Coolio::TCPSocket.  Encodes
  # requests and allows streaming consumption of the response.  Response is
  # parsed with a Ragel-generated whitelist parser which supports chunked
  # HTTP encoding.
  #
  # == Example
  #
  #   loop = Coolio::Loop.default
  #   client = Coolio::HttpClient.connect("www.google.com").attach
  #   client.get('/search', query: {q: 'foobar'})
  #   loop.run
  #
  class HttpClient < TCPSocket
    include HttpEncoding

    ALLOWED_METHODS=[:put, :get, :post, :delete, :head]
    TRANSFER_ENCODING="TRANSFER_ENCODING"
    CONTENT_LENGTH="CONTENT_LENGTH"
    SET_COOKIE="SET_COOKIE"
    LOCATION="LOCATION"
    HOST="HOST"
    CRLF="\r\n"

    # Connect to the given server, with port 80 as the default
    def self.connect(addr, port = 80, *args)
      super
    end

    def initialize(socket)
      super

      @parser = HttpClientParser.new
      @parser_nbytes = 0

      @state = :response_header
      @data = ::IO::Buffer.new

      @response_header = HttpResponseHeader.new
      @chunk_header = HttpChunkHeader.new
    end

    # Send an HTTP request and consume the response.
    # Supports the following options:
    #
    #   head: {Key: Value}
    #     Specify an HTTP header, e.g. {'Connection': 'close'}
    #
    #   query: {Key: Value}
    #     Specify query string parameters (auto-escaped)
    #
    #   cookies: {Key: Value}
    #     Specify hash of cookies (auto-escaped)
    #
    #   body: String
    #     Specify the request body (you must encode it for now)
    #
    def request(method, path, options = {})
      raise ArgumentError, "invalid request path" unless /^\// === path
      raise RuntimeError, "request already sent" if @requested

      @method, @path, @options = method, path, options
      @requested = true

      return unless @connected
      send_request
    end

    # Enable the HttpClient if it has been disabled
    def enable
      super
      dispatch unless @data.empty?
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
      @state == :finished ? close : @state = :finished
    end

    # called by close
    def on_close
      if @state != :finished and @state == :body
        on_request_complete
      end
    end

    # Called when an error occurs dispatching the request
    def on_error(reason)
      close
      raise RuntimeError, reason
    end

    #########
    protected
    #########

    #
    # Coolio callbacks
    #

    def on_connect
      @connected = true
      send_request if @method and @path
    end

    def on_read(data)
      @data << data
      dispatch
    end

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
      head['user-agent'] ||= "Coolio #{Coolio::VERSION}"

      # Default to Connection: close
      head['connection'] ||= 'close'

      # Build the request
      request_header = encode_request(@method, @path, query)
      request_header << encode_headers(head)
      request_header << encode_cookies(cookies) if cookies
      request_header << CRLF

      write request_header
    end

    def send_request_body
      write @options[:body] if @options[:body]
    end

    #
    # Response processing
    #

    def dispatch
      while enabled? and case @state
        when :response_header
          parse_response_header
        when :chunk_header
          parse_chunk_header
        when :chunk_body
          process_chunk_body
        when :chunk_footer
          process_chunk_footer
        when :response_footer
          process_response_footer
        when :body
          process_body
        when :finished, :invalid
          break
        else raise RuntimeError, "invalid state: #{@state}"
        end
      end
    end

    def parse_header(header)
      return false if @data.empty?

      begin
        @parser_nbytes = @parser.execute(header, @data.to_str, @parser_nbytes)
      rescue Coolio::HttpClientParserError
        on_error "invalid HTTP format, parsing fails"
        @state = :invalid
      end

      return false unless @parser.finished?

      # Clear parsed data from the buffer
      @data.read(@parser_nbytes)
      @parser.reset
      @parser_nbytes = 0

      true
    end

    def parse_response_header
      return false unless parse_header(@response_header)

      unless @response_header.http_status and @response_header.http_reason
        on_error "no HTTP response"
        @state = :invalid
        return false
      end

      on_response_header(@response_header)

      if @response_header.chunked_encoding?
        @state = :chunk_header
      else
        @state = :body
        @bytes_remaining = @response_header.content_length
      end

      true
    end

    def parse_chunk_header
      return false unless parse_header(@chunk_header)

      @bytes_remaining = @chunk_header.chunk_size
      @chunk_header = HttpChunkHeader.new

      @state = @bytes_remaining > 0 ? :chunk_body : :response_footer
      true
    end

    def process_chunk_body
      if @data.size < @bytes_remaining
        @bytes_remaining -= @data.size
        on_body_data @data.read
        return false
      end

      on_body_data @data.read(@bytes_remaining)
      @bytes_remaining = 0

      @state = :chunk_footer
      true
    end

    def process_chunk_footer
      return false if @data.size < 2

      if @data.read(2) == CRLF
        @state = :chunk_header
      else
        on_error "non-CRLF chunk footer"
        @state = :invalid
      end

      true
    end

    def process_response_footer
      return false if @data.size < 2

      if @data.read(2) == CRLF
        if @data.empty?
          on_request_complete
          @state = :finished
        else
          on_error "garbage at end of chunked response"
          @state = :invalid
        end
      else
        on_error "non-CRLF response footer"
        @state = :invalid
      end

      false
    end

    def process_body
      if @bytes_remaining.nil?
        on_body_data @data.read
        return false
      end

      if @bytes_remaining.zero?
        on_request_complete
        @state = :finished
        return false
      end

      if @data.size < @bytes_remaining
        @bytes_remaining -= @data.size
        on_body_data @data.read
        return false
      end

      on_body_data @data.read(@bytes_remaining)
      @bytes_remaining = 0

      if @data.empty?
        on_request_complete
        @state = :finished
      else
        on_error "garbage at end of body"
        @state = :invalid
      end

      false
    end
  end
end
