$LOAD_PATH.unshift File.dirname(__FILE__)
$LOAD_PATH.unshift File.expand_path('../../lib', __FILE__)

require 'rspec'
require 'cool.io'

RSpec.configure do |c|
  if RUBY_PLATFORM =~ /mingw|win32/
    $stderr.puts "Skip some specs on Windows"
    c.filter_run_excluding :env => :win
  end
end
