#--
# Copyright (C)2007 Tony Arcieri
# You can redistribute this under the terms of the Ruby license
# See file LICENSE for details
#++

module Rev
  # A buffered I/O class witch fits into the Rev Watcher framework.
  # It provides both an observer which reads data as it's received
  # from the wire and a buffered write watcher which stores data and writes
  # it out each time the socket becomes writable.
  #
  # This class is primarily meant as a base class for other streams
  # which need non-blocking writing, and is used to implement Rev's
  # Socket class and its associated subclasses.
  class IO < IOWatcher
    # Maximum number of bytes to consume at once
    INPUT_SIZE = 16384

    def initialize(io)
      @_io = io
      @_write_buffer = Rev::Buffer.new
      super(@_io)
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

    # Number of bytes are currently in the output buffer
    def output_buffer_size
      @_write_buffer.size
    end

    # Close the IO stream
    def close
      detach if attached?
      detach_write_watcher
      @_io.close unless @_io.closed?

      on_close
      nil
    end

    # Is the IO object closed?
    def closed?
      @_io.closed?
    end

    #########
    protected
    #########

    # Read from the input buffer and dispatch to on_read
    def on_readable
      begin
        on_read @_io.read_nonblock(INPUT_SIZE)
      rescue Errno::ECONNRESET, EOFError
        close
      end
    end
    
    # Write the contents of the output buffer
    def on_writable
      begin
        @_write_buffer.write_to(@_io)
      rescue Errno::EPIPE, Errno::ECONNRESET
        return close
      end
      
      if @_write_buffer.empty?
        disable_write_watcher
        on_write_complete
      end
    end

    # Schedule a write to be performed when the IO object becomes writable 
    def schedule_write
      begin
        enable_write_watcher      
      rescue IOError
      end
    end
    
    # Return a handle to the writing IOWatcher
    def write_watcher
      @_write_watcher ||= WriteWatcher.new(@_io, self)
    end
    
    def enable_write_watcher
      if write_watcher.attached?
        write_watcher.enable unless write_watcher.enabled?
      else
        return detach_write_watcher unless evloop # socket closed
        write_watcher.attach(evloop)
      end
    end
    
    def disable_write_watcher
      @_write_watcher.disable if @_write_watcher and @_write_watcher.enabled?
    end
    
    def detach_write_watcher
      @_write_watcher.detach if @_write_watcher and @_write_watcher.attached?
    end

    class WriteWatcher < IOWatcher
      def initialize(ruby_io, rev_io)
        @rev_io = rev_io
        super(ruby_io, :w)
      end

      # Delegate on_writable to the Rev::IO object
      def on_writable
        @rev_io.__send__(:on_writable)
      end
    end
  end
end