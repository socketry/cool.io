Cool.io
=======

### If you are interested in Celluloid based IO framework, please check out [Celluloid::IO](http://github.com/celluloid/celluloid-io)

Cool.io is an event library for Ruby, built on the libev event library which 
provides a cross-platform interface to high performance system calls .  This 
includes the epoll system call for Linux, the kqueue system call for BSDs and 
OS X, and the completion ports interface for Solaris.

Cool.io also binds asynchronous wrappers to Ruby's core socket classes so you can
use them in conjunction with Cool.io to build asynchronous event-driven 
applications.

You can include Cool.io in your programs with:

	require 'cool.io'


Anatomy
-------

Cool.io builds on two core classes which bind to the libev API:

* Cool.io::Loop - This class represents an event loop which uses underlying high
  performance system calls to wait for events.

* Cool.io::Watcher - This is the base class for event observers.  Once you attach
  an event observer to a loop and start running it, you will begin receiving
  callbacks to particlar methods when events occur.

Watchers
--------

There are presently four types of watchers:

* Cool.io::IOWatcher - This class waits for an IO object to become readable,
  writable, or both.

* Cool.io::TimerWatcher - This class waits for a specified duration then fires
  an event.  You can also configure it to fire an event at specified intervals.

* Cool.io::StatWatcher - Monitors files or directories for changes

* Cool.io::AsyncWatcher - Can be used to wake up a Cool.io::Loop running in a
  different thread. This allows each thread to run a separate Cool.io::Loop and
  for the different event loops to be able to signal each other.

Using Watchers
--------------

Watchers have five important methods:

* attach(loop) - This binds a watcher to the specified event loop.  If the
  watcher is already bound to a loop it will be detached first, then attached
  to the new one.

* detach - This completely unbinds a watcher from an event loop.

* disable - This stops the watcher from receiving events but does not unbind
  it from the loop.  If you are trying to toggle a watcher on and off, it's
  best to use this method (and enable) as it performs better than completely
  removing the watcher from the event loop.

* enable - This re-enables a watcher which has been disabled in the past.
  The watcher must still be bound to an event loop.

* evloop - This returns the Cool.io::Loop object which the watcher is currently
  bound to.

Asynchronous Wrappers
---------------------

Several classes which provide asynchronous event-driven wrappers for Ruby's
core socket classes are also provided.  Among these are:

* Cool.io::TCPSocket - A buffered wrapper to core Ruby's Socket class for use with
  TCP sockets.  You can asynchronously create outgoing TCP connections using 
  its Cool.io::TCPSocket.connect method.  Cool.io::TCPSocket provides write buffering
  to ensure that writing never blocks, and has asynchronous callbacks for
  several events, including when the connection is opened (or failed), when
  data is received, when the write buffer has been written out completely,
  and when the connection closes.

* Cool.io::TCPServer - A wrapper for TCPServer which creates new instances of
  Cool.io::TCPSocket (or any subclass you wish to provide) whenever an incoming
  connection is received.

Example Program
---------------

Cool.io provides a Sinatra-like DSL for authoring event-driven programs:

    require 'cool.io'
    require 'cool.io/dsl'

    ADDR = '127.0.0.1'
    PORT = 4321

    cool.io.connection :echo_server_connection do
      on_connect do
        puts "#{remote_addr}:#{remote_port} connected"
      end

      on_close do
        puts "#{remote_addr}:#{remote_port} disconnected"
      end

      on_read do |data|
        write data
      end
    end

    puts "Echo server listening on #{ADDR}:#{PORT}"
    cool.io.server ADDR, PORT, :echo_server_connection
    cool.io.run
    
This creates a new connection class called :echo_server_connection and defines
a set of callbacks for when various events occur.

We then create a new server on the given address and port. When this server
receives new connections, it will create new instances of the given connection
class for each connection.

Finally, we kick everything off with cool.io.run. Calling cool.io.run will 
block, listening for events on our server.
    
Using Cool.io subclasses directly
---------------------------------

Below is an example of how to write an echo server using a subclass instead of
the DSL:

	require 'cool.io'
	HOST = 'localhost'
	PORT = 4321

	class EchoServerConnection < Cool.io::TCPSocket
	  def on_connect
	    puts "#{remote_addr}:#{remote_port} connected"
	  end

	  def on_close
	    puts "#{remote_addr}:#{remote_port} disconnected"
	  end

	  def on_read(data)
	    write data
	  end
	end

	server = Cool.io::TCPServer.new(HOST, PORT, EchoServerConnection)
	server.attach(Cool.io::Loop.default)

	puts "Echo server listening on #{HOST}:#{PORT}"
	Cool.io::Loop.default.run

Here a new observer type (EchoServerConnection) is made by subclassing an
existing one and adding new implementations to existing event handlers.

A new event loop is created, and a new Cool.io::TCPServer (whose base class is
Cool.io::Watcher) is created and attached to the event loop.

Once this is done, the event loop is started with event_loop.run.  This method
will block until there are no active watchers for the loop or the loop is
stopped explicitly with event_loop.stop.
