#--
# Copyright (C)2007 Tony Arcieri
# You can redistribute this under the terms of the Ruby license
# See file LICENSE for details
#++

require File.dirname(__FILE__) + '/../rev'

module Rev
  class BufferedIO < IOWatcher
    def initialize(io)
      # Output buffer
      @write_buffer = ''

      # Coerce the argument into an IO object if possible
      @io = IO.try_convert(io)
      super(@io)
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
      # Attempt a zero copy write
      if @write_buffer.empty?
        written = write_nonblock data

        # If we lucked out and wrote out the whole buffer, return
        if written == data.size
          on_write_complete
          return data.size
        end

        # Otherwise slice what we wrote out and begin buffered writing
        data.slice!(0, written) if written
      end
      
      @write_buffer << data
      schedule_write
      data.size
    end

    # Number of bytes are currently in the output buffer
    def output_buffer_size
      @write_buffer.size
    end

    # Attempt to write the contents of the output buffer
    def write_output_buffer
      return if @write_buffer.empty?

      written = write_nonblock @write_buffer
      @write_buffer.slice!(0, written) if written

      return unless @write_buffer.empty?

      @writer.disable if @writer and @writer.enabled?
      on_write_complete
    end
    
    # Close the BufferedIO stream
    def close
      detach if attached?
      @writer.detach if @writer and @writer.attached?
      @io.close

      on_close
    end

    #########
    protected
    #########
    
    # Wrapper for handling reset connections and EAGAIN
    def write_nonblock(data)
      begin
        @io.write_nonblock(data)
      rescue Errno::ECONNRESET
        close
      rescue Errno::EAGAIN
      end
    end

    # Inherited callback from IOWatcher
    def on_readable
      begin
        on_read @io.read_nonblock(4096)
      rescue EOFError
        close
      end
    end
     
    def schedule_write
      return if @writer and @writer.enabled?
      if @writer 
        @writer.enable
      else 
        @writer = Writer.new(@io, self)
        @writer.attach(evloop)
      end
    end

    class Writer < IOWatcher
      def initialize(io, buffered_io)
        @buffered_io = buffered_io
        super(io, :w)
      end

      def on_writable
        @buffered_io.write_output_buffer
      end
    end
  end
end
