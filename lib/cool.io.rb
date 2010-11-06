#--
# Copyright (C)2007-10 Tony Arcieri
# You can redistribute this under the terms of the Ruby license
# See file LICENSE for details
#++

require 'iobuffer'

require "cool.io_ext"
require "cool.io/loop"
require "cool.io/meta"
require "cool.io/io"
require "cool.io/iowatcher"
require "cool.io/timer_watcher"
require "cool.io/async_watcher"
require "cool.io/listener"
require "cool.io/dns_resolver"
require "cool.io/socket"
require "cool.io/server"
require "cool.io/http_client"

module Coolio
  VERSION = File.read(File.expand_path('../../VERSION', __FILE__)).strip
  def self.version; VERSION; end
end