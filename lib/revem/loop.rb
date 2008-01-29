#--
# Copyright (C)2007 Tony Arcieri
# You can redistribute this under the terms of the Ruby license
# See file LICENSE for details
#++

require File.dirname(__FILE__) + '/../revem'

module EventMachine
  # Start the Reactor loop
  def self.run
    yield
    Rev::Loop.default.run
  end

  def self.stop
    Rev::Loop.default.stop
  end
end
