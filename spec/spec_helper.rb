$LOAD_PATH.unshift File.dirname(__FILE__)
$LOAD_PATH.unshift File.expand_path('../../lib', __FILE__)

require 'rspec'
require 'cool.io'

def unused_port
  s = TCPServer.open(0)
  port = s.addr[1]
  s.close
  port
end

RSpec.configure do |c|
  if RUBY_PLATFORM =~ /mingw|mswin/
    $stderr.puts "Skip some specs on Windows"
    c.filter_run_excluding :env => :exclude_win
  end
end
