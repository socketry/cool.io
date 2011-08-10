#--
# Copyright (C)2011 Tony Arcieri
# You can redistribute this under the terms of the Ruby license
# See file LICENSE for details
#++

require 'iobuffer'

require "cool.io/version"
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
require "cool.io/dsl"

module Coolio
  def self.inspect; "Cool.io"; end
end

module Cool
  # Allow Coolio module to be referenced as Cool.io
  def self.io; Coolio; end
end