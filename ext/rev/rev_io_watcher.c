#include "ruby.h"
#include "rubyio.h"

#define EV_STANDALONE 1
#include "../libev/ev.h"

#include "rev.h"
#include "rev_watcher.h"

/* Module and object handles */
extern VALUE Rev;
extern VALUE Rev_Loop;
extern VALUE Rev_Watcher;
VALUE Rev_IOWatcher = Qnil;

/* Method implementations */
static VALUE Rev_IOWatcher_initialize(VALUE self, VALUE io);
static VALUE Rev_IOWatcher_attach(VALUE self, VALUE loop);
static VALUE Rev_IOWatcher_detach(VALUE self);
static VALUE Rev_IOWatcher_on_readable(VALUE self);
static VALUE Rev_IOWatcher_on_writable(VALUE self);

/* Callbacks */
static void Rev_IOWatcher_libev_callback(struct ev_loop *ev_loop, struct ev_io *io, int revents);
static void Rev_IOWatcher_dispatch_callback(VALUE self, int revents);

void Init_rev_io_watcher()
{	
  Rev_IOWatcher = rb_define_class_under(Rev, "IOWatcher", Rev_Watcher);
  rb_define_method(Rev_IOWatcher, "initialize", Rev_IOWatcher_initialize, 1);
  rb_define_method(Rev_IOWatcher, "attach", Rev_IOWatcher_attach, 1);
  rb_define_method(Rev_IOWatcher, "detach", Rev_IOWatcher_detach, 0);
  rb_define_method(Rev_IOWatcher, "on_readable", Rev_IOWatcher_on_readable, 0);
  rb_define_method(Rev_IOWatcher, "on_writable", Rev_IOWatcher_on_writable, 0);
}

/**
 *  call-seq:
 *    Rev::IOWatcher.initialize(IO) -> Rev::IOWatcher
 * 
 * Create a new Rev::IOWatcher for the given IO object and add it to the given Rev::Loop
 */
static VALUE Rev_IOWatcher_initialize(VALUE self, VALUE io)
{
  rb_io_t *fptr;
  struct Rev_Watcher *watcher_data;

  /* Try to convert the IO argument into an IO object if it isn't already */
  io = rb_funcall(rb_cIO, rb_intern("try_convert"), 1, io);
  GetOpenFile(io, fptr);

  Data_Get_Struct(self, struct Rev_Watcher, watcher_data);

  watcher_data->dispatch_callback = Rev_IOWatcher_dispatch_callback;
  ev_io_init(&watcher_data->event_types.ev_io, Rev_IOWatcher_libev_callback, fptr->fd, EV_READ);	
  watcher_data->event_types.ev_io.data = (void *)self;	
}

static VALUE Rev_IOWatcher_attach(VALUE self, VALUE loop)
{
  Watcher_Attach(io, Rev_IOWatcher_detach, self, loop);

  return self;	
}

static VALUE Rev_IOWatcher_detach(VALUE self)
{
  Watcher_Detach(io, self);

  return self;
}

/**
 *  call-seq:
 *    Rev::IOWatcher#on_readable -> nil
 * 
 * Called whenever the IO object associated with the IOWatcher is readable
 */
static VALUE Rev_IOWatcher_on_readable(VALUE self)
{
  return Qnil;
}

/**
 *  call-seq:
 *    Rev::IOWatcher#on_writable -> nil
 * 
 * Called whenever the IO object associated with the IOWatcher is writable
 */

static VALUE Rev_IOWatcher_on_writable(VALUE self)
{
  return Qnil;
}

/* libev callback */
static void Rev_IOWatcher_libev_callback(struct ev_loop *ev_loop, struct ev_io *io, int revents)
{
  Rev_Loop_process_event((VALUE)io->data, revents);
}

/* Rev::Loop dispatch callback */
static void Rev_IOWatcher_dispatch_callback(VALUE self, int revents)
{	
  if(revents & EV_READ)
    rb_funcall(self, rb_intern("on_readable"), 0, 0);
  else if(revents & EV_WRITE)
    rb_funcall(self, rb_intern("on_writable"), 0, 0);
  else
    rb_raise(rb_eRuntimeError, "unknown revents value for ev_io: %d", revents);
}
