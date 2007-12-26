#--
# Copyright (C)2007 Tony Arcieri
# You can redistribute this under the terms of the Ruby license
# See file LICENSE for details
#++

require File.dirname(__FILE__) + '/rev_ext'
require File.dirname(__FILE__) + '/rev/loop'
require File.dirname(__FILE__) + '/rev/watcher'
require File.dirname(__FILE__) + '/rev/io_watcher'
require File.dirname(__FILE__) + '/rev/timer_watcher'
require File.dirname(__FILE__) + '/rev/listener'
require File.dirname(__FILE__) + '/rev/buffered_io'
require File.dirname(__FILE__) + '/rev/dns_resolver'
require File.dirname(__FILE__) + '/rev/socket'
require File.dirname(__FILE__) + '/rev/server'

module Rev
  Rev::VERSION = '0.1.1' unless defined? Rev::VERSION
  def self.version() VERSION end
end
