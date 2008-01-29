#--
# Copyright (C)2007 Tony Arcieri
# You can redistribute this under the terms of the Ruby license
# See file LICENSE for details
#++

require File.dirname(__FILE__) + '/../revem'

module EventMachine
  class Connection < Rev::TCPSocket
    def post_init; end

    def unbind; end

    def send_data(data)
      write data
    end
    
    #########
    protected
    #########
    
    def on_connect
      post_init
    end

    def on_close
      unbind
    end
    
    def on_read(data)
      receive_data data
    end
  end
end
