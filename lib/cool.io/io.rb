#--
# Copyright (C)2007-10 Tony Arcieri
# You can redistribute this under the terms of the Ruby license
# See file LICENSE for details
#++

module Coolio
  # A buffered I/O class witch fits into the Coolio Watcher framework.
  # It provides both an observer which reads data as it's received
  # from the wire and a buffered write watcher which stores data and writes
  # it out each time the socket becomes writable.
  #
  # This class is primarily meant as a base class for other streams
  # which need non-blocking writing, and is used to implement Coolio's
  # Socket class and its associated subclasses.
  class IO
    extend Meta

    # Maximum number of bytes to consume at once
    INPUT_SIZE = 16384

    def initialize(io)
      @_io = io
      @_write_buffer  ||= ::IO::Buffer.new
      @_read_watcher  = Watcher.new(io, self, :r)
      @_write_watcher = Watcher.new(io, self, :w)
    end

    #
    # Watcher methods, delegated to @_read_watcher
    #

    # Attach to the event loop
    def attach(loop)
      @_read_watcher.attach(loop)
      schedule_write if !@_write_buffer.empty?
      self
    end

    # Detach from the event loop
    def detach
      # TODO should these detect write buffers, as well?
      @_read_watcher.detach
      self
    end

    # Enable the watcher
    def enable
      @_read_watcher.enable
      self
    end

    # Disable the watcher
    def disable
      @_read_watcher.disable
      self
    end

    # Is the watcher attached?
    def attached?
      @_read_watcher.attached?
    end

    # Is the watcher enabled?
    def enabled?
      @_read_watcher.enabled?
    end

    # Obtain the event loop associated with this object
    def evloop
      @_read_watcher.evloop
    end

    #
    # Callbacks for asynchronous events
    #

    # Called whenever the IO object receives data
    def on_read(data); end
    event_callback :on_read

    # Called whenever a write completes and the output buffer is empty
    def on_write_complete; end
    event_callback :on_write_complete

    # Called whenever the IO object hits EOF
    def on_close; end
    event_callback :on_close

    #
    # Write interface
    #

    # Write data in a buffered, non-blocking manner
    def write(data)
      @_write_buffer << data
      schedule_write
      data.size
    end

    # Close the IO stream
    def close
      detach if attached?
      detach_write_watcher
      @_io.close unless closed?

      on_close
      nil
    end

    # Is the IO object closed?
    def closed?
      @_io.nil? or @_io.closed?
    end

    #########
    protected
    #########

    # Read from the input buffer and dispatch to on_read
    def on_readable
      begin
        on_read @_io.read_nonblock(INPUT_SIZE)
      rescue Errno::EAGAIN, Errno::EINTR
        return

      # SystemCallError catches Errno::ECONNRESET amongst others.
      rescue SystemCallError, EOFError, IOError, SocketError
        close
      end
    end

    # Write the contents of the output buffer
    def on_writable
      begin
        @_write_buffer.write_to(@_io)
      rescue Errno::EINTR
        return

      # SystemCallError catches Errno::EPIPE & Errno::ECONNRESET amongst others.
      rescue SystemCallError, IOError, SocketError
        return close
      end

      if @_write_buffer.empty?
        disable_write_watcher
        on_write_complete
      end
    end

    # Schedule a write to be performed when the IO object becomes writable
    def schedule_write
      return unless @_io # this would mean 'we are still pre DNS here'
      return unless @_read_watcher.attached? # this would mean 'currently unattached' -- ie still pre DNS, or just plain not attached, which is ok
      begin
        enable_write_watcher
      rescue IOError
      end
    end

    def enable_write_watcher
      if @_write_watcher.attached?
        @_write_watcher.enable unless @_write_watcher.enabled?
      else
        @_write_watcher.attach(evloop)
      end
    end

    def disable_write_watcher
      @_write_watcher.disable if @_write_watcher and @_write_watcher.enabled?
    end

    def detach_write_watcher
      @_write_watcher.detach if @_write_watcher and @_write_watcher.attached?
    end

    # Internal class implementing watchers used by Coolio::IO
    class Watcher < IOWatcher
      def initialize(ruby_io, coolio_io, flags)
        @coolio_io = coolio_io
        super(ruby_io, flags)
      end

      # Configure IOWatcher event callbacks to call the method passed to #initialize
      def on_readable
        @coolio_io.__send__(:on_readable)
      end

      def on_writable
        @coolio_io.__send__(:on_writable)
      end
    end
  end
end
