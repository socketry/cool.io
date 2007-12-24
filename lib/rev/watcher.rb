require File.dirname(__FILE__) + '/../rev'

module Rev
  class Watcher
    # Use an alternate watcher with the attach/detach/enable/disable methods
    # if it is presently assigned.  This is useful if you are waiting for
    # an event to occur before the current watcher can be used in earnest,
    # such as making an outgoing TCP connection.
    def self.watcher_delegate(proxy_var)
      %w{attach detach enable disable}.each do |method|
        module_eval <<-EOD
          def #{method}(*args)
            if #{proxy_var}
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
    def self.event_callback(*methods)
      methods.each do |method|
        module_eval <<-EOD
          def #{method}(*args, &block)
            if block
              @#{method}_callback = block
              return
            end

            @#{method}_callback.(*([self] + args)) if @#{method}_callback
          end
        EOD
      end
    end
  end
end
