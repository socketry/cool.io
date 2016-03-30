#--
# Copyright (C)2007-10 Tony Arcieri
# You can redistribute this under the terms of the Ruby license
# See file LICENSE for details
#++

module Coolio
  module Meta
    # Use an alternate watcher with the attach/detach/enable/disable methods
    # if it is presently assigned.  This is useful if you are waiting for
    # an event to occur before the current watcher can be used in earnest,
    # such as making an outgoing TCP connection.
    def watcher_delegate(proxy_var)
      %w{attach attached? detach enable disable}.each do |method|
        module_eval <<-EOD
          def #{method}(*args)
            if defined? #{proxy_var} and #{proxy_var}
              #{proxy_var}.#{method}(*args)
              return self
            end

            super
          end
        EOD
      end
    end

    # Define callbacks whose behavior can be changed on-the-fly per instance.
    # This is done by giving a block to the callback method, which is captured
    # as a proc and stored for later.  If the method is called without a block,
    # the stored block is executed if present, otherwise it's a noop.
    def event_callback(*methods)
      methods.each do |method|
        module_eval <<-EOD
          remove_method "#{method}"
          def #{method}(*args, &block)
            if block
              @#{method}_callback = block
              return
            end

            if defined? @#{method}_callback and @#{method}_callback
              @#{method}_callback.call(*args)
            end
          end
        EOD
      end
    end
  end
end
