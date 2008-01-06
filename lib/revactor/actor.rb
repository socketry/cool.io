#--
# Copyright (C)2007 Tony Arcieri
# You can redistribute this under the terms of the Ruby license
# See file LICENSE for details
#++

require File.dirname(__FILE__) + '/../revactor'
require 'fiber'

class ActorError < StandardError; end

# Actors are lightweight concurrency primitives which communiucate via message
# passing.  Each actor has a mailbox which it scans for matching messages.
# An actor sleeps until it receives a message, at which time it scans messages
# against its filter set and then executes appropriate callbacks.
#
# The Actor class is definined in the global scope in hopes of being generally
# useful for Ruby 1.9 users while also attempting to be as compatible as
# possible with the Rubinius Actor implementation.  In this way it should
# be possible to run programs written using Rev on top of Rubinius and hopefully
# get some better performance.
#
# Rev Actor implements some features that Rubinius does not, however, such as
# receive timeouts, receive filter-by-proc, arguments passed to spawn, and an
# actor dictionary (used for networking functionality).  Hopefully these 
# additional features will not get in the way of Rubinius / Rev compatibility.
#
class Actor < Fiber  
  # Actor::ANY can be used in a filter match any message
  ANY = Object unless defined? Actor::ANY
  @@registered = {}

  class << self
    include Enumerable

    # Create a new Actor with the given block and arguments
    def new(*args, &block)
      raise ArgumentError, "no block given" unless block
      actor = super do 
        block.call(*args)
        Actor.current.instance_eval { @dead = true }
      end

      # For whatever reason #initialize is never called in subclasses of Fiber
      actor.instance_eval do 
        @dead = false
        @mailbox = Mailbox.new
      end

      Scheduler << actor
      actor
    end
    
    alias_method :spawn, :new
    
    # This will be defined differently in the future, but now the two are the same
    alias_method :start, :new
    
    # Obtain a handle to the current Actor
    def current
      actor = super
      raise ActorError, "current fiber is not an actor" unless actor.is_a? Actor
      
      actor 
    end
    
    # Wait for messages matching a given filter.  The filter object is yielded
    # to be block passed to receive.  You can then invoke the when argument
    # which takes a parameter and a block.  Messages are compared (using ===)
    # against the parameter, or if the parameter is a proc it is called with
    # a message and matches if the proc returns true.
    #
    # The first filter to match a message in the mailbox is executed.  If no
    # filters match then the actor sleeps.
    def receive(&filter)
      unless current.is_a?(Actor)
        raise ActorError, "receive must be called in the context of an Actor"
      end

      current.instance_eval { @mailbox.receive(&filter) }
    end

    # Register this actor in the global dictionary
    def []=(key, actor)
      unless actor.is_a?(Actor)
        raise ArgumentError, "only actors may be registered"
      end

      @@registered[key] = actor
    end

    # Look up an actor in the global dictionary
    def [](key)
      @@registered[key]
    end

    # Delete an actor from the global dictionary
    def delete(key, &block)
      @@registered.delete(key, &block)
    end

    # Iterate over the actors in the global dictionary
    def each(&block)
      @@registered.each(&block)
    end
  end
  
  # Is the current actor dead?
  def dead?; @dead; end
  
  # Send a message to an actor
  def <<(message)
    # Erlang discards messages sent to dead actors, and if Erlang does it,
    # it must be the right thing to do, right?
    return message if dead?
    
    @mailbox << message
    Scheduler << self
    message
  end

  alias_method :send, :<<

  # Actor scheduler class, maintains a run queue of actors with outstanding
  # messages who have not yet processed their mailbox.  If all actors have
  # processed their mailboxes then the scheduler waits for any outstanding
  # Rev events.  If there are no active Rev watchers then the scheduler exits.
  class Scheduler
    @@queue = []
    @@running = false

    class << self
      def <<(actor)
        @@queue << actor
        run unless @@running
      end
      
      def run
        return if @@running
        
        @@running = true
        until @@queue.empty? and Rev::Loop.default.watchers.empty?
          run_queue = @@queue
          @@queue = []
          
          run_queue.each do |actor|
            begin
              actor.resume
            rescue FiberError # Fiber may have died since being scheduled 
            end
          end
          
          # Don't run Rev if there are still outstanding messages
          next unless @@queue.empty?
          
          puts Rev::Loop.default.watchers.inspect
          Rev::Loop.default.run_once unless Rev::Loop.default.watchers.empty?
        end
        @@running = false
      end
    end
  end

  # Actor mailbox.  For purposes of efficiency the mailbox also handles 
  # suspending and resuming an actor when no messages match its filter set.
  class Mailbox
    attr_accessor :timer

    def initialize
      @timer = nil
      @queue = []
    end

    def <<(message)
      @queue << message
    end

    def receive
      raise ArgumentError, "no filter block given" unless block_given?

      filter = Filter.new(self)
      yield filter
      raise ArgumentError, "empty filter" if filter.empty?

      matched_index = matched_message = action = nil
      last_index = 0

      while action.nil?
        @queue[last_index..@queue.size].each_with_index do |message, index|
          next unless (action = filter.match message)
          matched_index = index
          matched_message = message

          break
        end

        unless action
          last_index = @queue.size
          Actor.yield
        end
      end

      if @timer
        @timer.detach if @timer.attached?
        @timer = nil
      end
      
      @queue.delete_at matched_index
      return action.(matched_message)
    end

    # Timeout class, used to implement receive timeouts
    class Timer < Rev::TimerWatcher
      def initialize(timeout, actor)
        @actor = actor
        super(timeout)
      end

      def on_timer
        @actor << :timeout
        detach
      end
    end
 
    # Mailbox filterset.  Takes patterns or procs to match messages with
    # and returns the associated proc when a pattern matches.
    class Filter
      def initialize(mailbox)
        @mailbox = mailbox
        @ruleset = []
      end

      def when(pattern, &action)
        raise ArgumentError, "no block given" unless action
        @ruleset << [pattern, action]
      end

      def after(timeout, &action)
        raise ArgumentError, "timeout already specified" if @mailbox.timer
        @mailbox.timer = Timer.new(timeout, Actor.current)
        @mailbox.timer.attach(Rev::Loop.default)
        @ruleset << [:timeout, action]
      end

      def match(message)
        _, action = @ruleset.find do |pattern, _|
          case pattern
          when Proc then pattern.(message)
          else pattern === message
          end
        end

        action
      end

      def empty?
        @ruleset.empty?
      end
    end
  end
end
