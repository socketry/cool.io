$LOAD_PATH.unshift File.expand_path('../../lib', __FILE__)

require 'rubygems'
require 'cool.io'

ADDR = '127.0.0.1'
PORT = 4321

cool.io.connect ADDR, PORT do
  on_connect do
    puts "Connected to #{remote_host}:#{remote_port}"
    write "bounce this back to me"
  end

  on_close do
    puts "Disconnected from #{remote_host}:#{remote_port}"
  end

  on_read do |data|
    puts "Got: #{data}"
    close
  end

  on_resolve_failed do
    puts "Error: Couldn't resolve #{remote_host}"
  end

  on_connect_failed do
    puts "Error: Connection refused to #{remote_host}:#{remote_port}"
  end
end

puts "Echo client connecting to #{ADDR}:#{PORT}..."
cool.io.run 
