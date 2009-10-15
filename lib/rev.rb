#--
# Copyright (C)2007 Tony Arcieri
# You can redistribute this under the terms of the Ruby license
# See file LICENSE for details
#++

# Pull in the OpenSSL extension if available
begin
  require 'openssl'
rescue LoadError
end

# Pull in iobuffer gem
require 'rubygems'
require 'iobuffer'

%w(
  /rev_ext /rev/loop /rev/meta /rev/io_watcher /rev/timer_watcher 
  /rev/async_watcher /rev/listener /rev/io /rev/dns_resolver 
  /rev/socket /rev/server /rev/http_client
).each do |file|
  require File.dirname(__FILE__) + file
end

module Rev
  Rev::VERSION = '0.3.2' unless defined? Rev::VERSION
  def self.version() VERSION end
end
