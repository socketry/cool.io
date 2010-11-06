#--
# Copyright (C)2007-10 Tony Arcieri
# You can redistribute this under the terms of the Ruby license
# See file LICENSE for details
#++

require 'iobuffer'
require "cool.io_ext"

# Legacy constant
Rev = Coolio

require "rev/loop"
require "rev/meta"
require "rev/io_watcher"
require "rev/timer_watcher"
require "rev/async_watcher"
require "rev/listener"
require "rev/io"
require "rev/dns_resolver"
require "rev/socket"
require "rev/server"
require "rev/http_client"

module Coolio
  VERSION = File.read File.expand_path('../../VERSION', __FILE__)
  def self.version; VERSION; end
end
