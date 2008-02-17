#--
# Copyright (C)2007 Tony Arcieri
# You can redistribute this under the terms of the Ruby license
# See file LICENSE for details
#++

module Rev
  # Class for working with Files asynchronously
  class File < IO
    # Open the given file.  Accepts the same arguments as File.open
    def self.open(*args, &block)
      new(::File.open(*args, &block))
    end
  end
end