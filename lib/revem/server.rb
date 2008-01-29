#--
# Copyright (C)2007 Tony Arcieri
# You can redistribute this under the terms of the Ruby license
# See file LICENSE for details
#++

require File.dirname(__FILE__) + '/../revem'

module EventMachine
  def self.start_server(addr, port, klass = Connection, *args, &block)
    server = Rev::TCPServer.new(addr, port, klass, *args, &block)
    server.attach(Rev::Loop.default)
  end
end
