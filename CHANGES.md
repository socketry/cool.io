1.5.3
-----

* Fix invalid IOWatcher's rb_funcall that causes ArgumentError with ruby 2.5 and clang

1.5.2
-----

* Fix invalid TimerWatcher's rb_funcall that causes ArgumentError with ruby 2.5 and clang

1.5.1
-----

* Don't raise an exception when peername failed

 1.5.0
-----

* Update libev to 4.24

1.4.6
-----

* Add ruby 2.4.0 to windows binary gem

 1.4.5
-----

* Increase FD_SETSIZE to 1024 on Windows

1.4.4
-----

* Suppress lots of warnings

1.4.3
-----

* Use accept instead of accept_nonblock on Windows to avoid thundering held problem
* Fix compilation error on Solaris and Ruby 2.3.0

1.4.2
-----

* Add unexpected object info to attach exception message

1.4.1
-----

* Use SleepEx instead of Sleep for better fix of process hung problem on windows environment
* Use rake-compiler-dock for cross compilation

1.4.0
-----

* Update libev to 4.20
* Sleep in timeout instead of select on Windows

1.3.1
-----

* Fix several bugs for JRuby support enhancement
* Fix deadlock bug on Windows environment
* Use RSpec3

1.3.0
-----

* Block evaluation doesn't change self for keeping consistency with Ruby block
* Remove EventMachine emulation module
* Remove HttpClient
* DSL syntax is no longer available by default. Need to require 'cool.io/dsl' in user code
* Update libev to 4.19

1.2.4
-----

* Fix a bug that #close for unconnected Socket doesn't detach all watchers (#33)
* Remove 1.8 support code
* Use standard library instead of own hosts list (#34)

1.2.3
-----

* Fix CPU consuming issue on Windows.

1.2.2
-----

* Add timeout option to Loop#run and Loop#run_once. Default by nil
* Support Ruby 2.2.0

1.2.1
-----

* Release the GIL when libev polls (#24)
* Add Listener#listen method to change backlog size

1.2.0
-----

* Support Windows environment via cross compilation
* Include iobuffer library
* Update to libev 4.15
* Remove Ruby 1.8 support

1.1.0
-----

* Switch from Jeweler to Bundler for the gem boilerplate
* Fix firing of Coolio::HttpClient#on_request_complete (#15)
* Fix failure to resolve Init_cool symbol on win32 mingw (#14)
* Fix closing /etc/hosts in the DNS resolver (#12)
* Refactor StatWatcher to pass pervious and current path state ala Node.js
* spec:valgrind Rake task to run specs under valgrind
* Use rake-compiler to build cool.io
* Upgrade to libev 4.04

1.0.0
-----

* Fancy new DSL

0.9.0
-----

* Rename the project to cool.io
* Bump the version all the way to 0.9! Hell yeah! 1.0 soon!
* Rename the main module from Rev to Coolio, with deprecation warnings for Rev
* Use Jeweler to manage the gem
* Update to RSpec 2.0
* Update to libev 4.01
* Initial Rubinius support

0.3.2
-----

* Perform a blocking system call if we're the only thread running (1.8 only)
* Run in non-blocking mode if we're the only thread in the process (1.8 only)
* Make Rev::Loop#run_nonblock signal-safe
* Fix spurious firing of Rev::AsyncWatchers

0.3.1
-----

* Configurable intervals for Rev::StatWatcher
* Fix broken version number :(
* Removed warning about spuriously readable sockets from Rev::Listener
* Rev::Listener ignores ECONNABORTED from accept_nonblock
* Document rationale for EAGAIN/ECONNABORTED handling in Rev::Listener

0.3.0
-----

* Add Rev::StatWatcher to monitor filesystem changes
* Add Rev::Listener#fileno for accessing the underlying file descriptor
* Support for creating Rev::Listeners from existing TCPServers/UNIXServers
* Upgrade to libev 3.8
* Simplified code loading
* Pull in iobuffer gem and change outstanding uses of Rev::Buffer to IO::Buffer
* Fix memory leaks resulting from strange semantics of Ruby's xrealloc
* Rev::UNIXServer: use path instead of the first argument
* Rev::Server-based classes can build off ::*Server objects

0.2.4
-----

* Ugh, botched my first release from the git repo.  Oh well.  Try, try again.

0.2.3
-----

* Initial Windows support
* Initial Ruby 1.8.7 and 1.9.1 support
* Upgrade to libev 3.52
* Add checks for sys/resource.h and don't allow getting/setting maxfds if it
  isn't present

0.2.2
-----

* Correct a pointer arithmetic error in the buffering code that could result
  in data corruption.
* Upgrade to libev 3.41
* Relax HTTP/1.1 reponse parser to allow the "reason" portion of the response
  header to be omitted

0.2.1
-----

* Upgrade to libev 3.31
* Rev::Loop#run_once and Rev::Loop#run_nonblock now return the number of events
  received when they were running
* Remove inheritence relationship between Rev::IO and Rev::IOWatcher
* Loosen HTTP/1.1 response parser to accept a common malformation in HTTP/1.1
  chunk headers
* Add underscore prefix to instance variables to avoid conflicts in subclasses
* Remove Rev::SSLServer until it can be made more useful

0.2.0
-----

* Initial Ruby 1.8.6 support
* Omit Rev::LIBEV_VERSION constant
* Catch Errno::ECONNRESET when writing to sockets
* SSL support via Rev::SSL, with a small C extension subclassing Ruby's
  OpenSSL::SSL::SSLSocket allowing for non-blocking SSL handshakes
* Initial Rev::Utils implementation with #ncpus and methods to query and
  change the maximum number of file descriptors for the current process.
* Initial Rev::AsyncWatcher implementation for cross-thread signaling
* Handle unspecified Content-Length when encoding is identity in HttpClient
* Fix bug in HttpClient processing zero Content-Length
* Get rid of method_missing stuff in Rev::HttpClient
* Have Rev::HttpClient close the connection on error
* Allow Rev::TCPSocket#on_connect to be private when accepting connections
  from a Rev::TCPServer

0.1.4
-----

* Calibrate Rev::TimerWatchers against ev_time() and ev_now() when the watcher
  is attached to the loop to ensure that the timeout interval is correct.
* Add check to ensure that a Rev::Loop cannot be run from within a callback
* Store Rev::Loop.default in a Thread-specific instance variable
* Upgrade libev to 0.3.0
* Rename BufferedIO to IO
* Fixed bug in BufferedIO#write_output_buffer causing it to spin endlessly on
  an empty buffer.
* Added has_active_watchers? to Rev::Loop to check for active watchers

0.1.3
-----

* Fixed bug in Rev::Buffer read_from and write_to: now rb_sys_fail on failed
  reads/writes.
* Change Rev::Buffer memory pools to purge on a periodic interval, rather than
  whenever the GC marks the object.
* Fix bug in tracking the active watcher count.  Factor shared watcher behavior
  from rev_watcher.h to rev_watcher.c.

0.1.2
-----

* Commit initial specs
* Improve RDoc for the library
* Eliminate "zero copy" writes as they bypass the event loop
* Added Rev::Buffer C extension to provide high speed buffered writes
* Implement Rev::TCPSocket#peeraddr to improve compatibility with Ruby sockets
* Added Rev::Listener.close for clean shutdown of a listener
* Rev::Loop.default used to call ev_loop_default() (in C).  However, this
  registers signal handlers which conflict with Ruby's own.  Now the behavior
  has been changed to return a thread-local singleton of Rev::Loop.
* Creating a new Rev::TCPListener will disable reverse lookups in BasicSocket
* Made backlog for Rev::TCPListener user-definable
* Rev::TCPSocket now implements an on_resolve_failed callback for failed DNS
  resolution.  By default it's aliased to on_connect_failed.
* Changed event_callbacks to use instance_exec rather than passing the
  watcher object as an argument.  Documented use of defining an event
  callback as a block
* Subsecond precision for Rev::TimerWatchers

0.1.1
-----

* Added Rev::HttpClient, an asynchronous HTTP/1.1 client written on top of
  the Rev::TCPSocket class
* Imported HTTP response parser from the RFuzz project
* Added exception handling for Errno::ECONNRESET and Errno::EAGAIN
* Fixed bugs in buffered writer which resulted in exceptions if all data
  couldn't be written with a nonblocking write.

0.1.0
-----

* Initial public release