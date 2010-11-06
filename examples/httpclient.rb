$LOAD_PATH.unshift File.expand_path('../../lib', __FILE__)

require 'rubygems'
require 'cool.io'

class MyHttpClient < Coolio::HttpClient
  def on_connect
    super
    STDERR.puts "Connected to #{remote_host}:#{remote_port}"
  end

  def on_connect_failed
    super
    STDERR.puts "Connection failed"
  end

  def on_response_header(header)
    STDERR.puts "Response: #{header.http_version} #{header.status} #{header.http_reason}"
  end

  def on_body_data(data)
    STDOUT.write data
    STDOUT.flush
  end

  def on_request_complete
    STDERR.puts "Request complete!"
  end

  def on_error(reason)
    STDERR.puts "Error: #{reason}"
  end
end

l = Coolio::Loop.default
c = MyHttpClient.connect("www.google.com", 80).attach(l)
c.request('GET', '/search', :query => { :q => 'foobar' })
l.run
