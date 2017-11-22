/*
 * Copyright (C) 2007-10 Tony Arcieri
 * You may redistribute this under the terms of the Ruby license.
 * See LICENSE for details
 */

#include "ruby.h"
#include "rubyio.h"

#include "ev_wrap.h"

#include "cool.io.h"
#include "watcher.h"

static VALUE mCoolio = Qnil;
static VALUE cCoolio_Watcher = Qnil;
static VALUE cCoolio_Loop = Qnil;
static VALUE cCoolio_IOWatcher = Qnil;

static VALUE Coolio_IOWatcher_initialize(int argc, VALUE *argv, VALUE self);
static VALUE Coolio_IOWatcher_attach(VALUE self, VALUE loop);
static VALUE Coolio_IOWatcher_detach(VALUE self);
static VALUE Coolio_IOWatcher_enable(VALUE self);
static VALUE Coolio_IOWatcher_disable(VALUE self);
static VALUE Coolio_IOWatcher_on_readable(VALUE self);
static VALUE Coolio_IOWatcher_on_writable(VALUE self);

static void Coolio_IOWatcher_libev_callback(struct ev_loop *ev_loop, struct ev_io *io, int revents);
static void Coolio_IOWatcher_dispatch_callback(VALUE self, int revents);

/*
 * Coolio::IOWatcher monitors Ruby IO objects for readability or writability.
 * This allows your application to block while the kernel is writing out
 * data and fill the read or write buffer whenever there is space available.
 */
void Init_coolio_iowatcher()
{
  mCoolio = rb_define_module("Coolio");
  cCoolio_Watcher = rb_define_class_under(mCoolio, "Watcher", rb_cObject);
  cCoolio_IOWatcher = rb_define_class_under(mCoolio, "IOWatcher", cCoolio_Watcher);
  cCoolio_Loop = rb_define_class_under(mCoolio, "Loop", rb_cObject);

  rb_define_method(cCoolio_IOWatcher, "initialize", Coolio_IOWatcher_initialize, -1);
  rb_define_method(cCoolio_IOWatcher, "attach", Coolio_IOWatcher_attach, 1);
  rb_define_method(cCoolio_IOWatcher, "detach", Coolio_IOWatcher_detach, 0);
  rb_define_method(cCoolio_IOWatcher, "enable", Coolio_IOWatcher_enable, 0);
  rb_define_method(cCoolio_IOWatcher, "disable", Coolio_IOWatcher_disable, 0);
  rb_define_method(cCoolio_IOWatcher, "on_readable", Coolio_IOWatcher_on_readable, 0);
  rb_define_method(cCoolio_IOWatcher, "on_writable", Coolio_IOWatcher_on_writable, 0);
}

/**
 *  call-seq:
 *    Coolio::IOWatcher.initialize(IO, events = 'r') -> Coolio::IOWatcher
 * 
 * Create a new Coolio::IOWatcher for the given IO object and add it to the given Coolio::Loop
 */
static VALUE Coolio_IOWatcher_initialize(int argc, VALUE *argv, VALUE self)
{
  VALUE io, flags;
  char *flags_str;
  int events;
  struct Coolio_Watcher *watcher_data;
#if HAVE_RB_IO_T
  rb_io_t *fptr;
#else
  OpenFile *fptr;
#endif

  rb_scan_args(argc, argv, "11", &io, &flags);

  if(flags != Qnil)
    flags_str = RSTRING_PTR(rb_String(flags));
  else
    flags_str = "r";

  if(!strcmp(flags_str, "r"))
    events = EV_READ;
  else if(!strcmp(flags_str, "w"))
    events = EV_WRITE;
  else if(!strcmp(flags_str, "rw"))
    events = EV_READ | EV_WRITE;
  else
    rb_raise(rb_eArgError, "invalid event type: '%s' (must be 'r', 'w', or 'rw')", flags_str);

  Data_Get_Struct(self, struct Coolio_Watcher, watcher_data);
  GetOpenFile(rb_convert_type(io, T_FILE, "IO", "to_io"), fptr);

  watcher_data->dispatch_callback = Coolio_IOWatcher_dispatch_callback;
  ev_io_init(&watcher_data->event_types.ev_io, Coolio_IOWatcher_libev_callback, FPTR_TO_FD(fptr), events);
  watcher_data->event_types.ev_io.data = (void *)self;

  return Qnil;
}

/**
 *  call-seq:
 *    Coolio::IOWatcher.attach(loop) -> Coolio::IOWatcher
 * 
 * Attach the IO watcher to the given Coolio::Loop.  If the watcher is already attached
 * to a loop, detach it from the old one and attach it to the new one.
 */
static VALUE Coolio_IOWatcher_attach(VALUE self, VALUE loop)
{
  Watcher_Attach(io, Coolio_IOWatcher_detach, self, loop);

  return self;  
}

/**
 *  call-seq:
 *    Coolio::IOWatcher.detach -> Coolio::IOWatcher
 * 
 * Detach the IO watcher from its current Coolio::Loop.
 */
static VALUE Coolio_IOWatcher_detach(VALUE self)
{
  Watcher_Detach(io, self);

  return self;
}

/**
 *  call-seq:
 *    Coolio::IOWatcher.enable -> Coolio::IOWatcher
 * 
 * Re-enable an IO watcher which has been temporarily disabled.  See the
 * disable method for a more thorough explanation.
 */
static VALUE Coolio_IOWatcher_enable(VALUE self)
{
  Watcher_Enable(io, self);

  return self;  
}

/**
 *  call-seq:
 *    Coolio::IOWatcher.disable -> Coolio::IOWatcher
 * 
 * Temporarily disable an IO watcher which is attached to a loop.  
 * This is useful if you wish to toggle event monitoring on and off.  
 */
static VALUE Coolio_IOWatcher_disable(VALUE self)
{
  Watcher_Disable(io, self);

  return self;
}

/**
 *  call-seq:
 *    Coolio::IOWatcher#on_readable -> nil
 * 
 * Called whenever the IO object associated with the IOWatcher is readable
 */
static VALUE Coolio_IOWatcher_on_readable(VALUE self)
{
  return Qnil;
}

/**
 *  call-seq:
 *    Coolio::IOWatcher#on_writable -> nil
 * 
 * Called whenever the IO object associated with the IOWatcher is writable
 */

static VALUE Coolio_IOWatcher_on_writable(VALUE self)
{
  return Qnil;
}

/* libev callback */
static void Coolio_IOWatcher_libev_callback(struct ev_loop *ev_loop, struct ev_io *io, int revents)
{
  Coolio_Loop_process_event((VALUE)io->data, revents);
}

/* Coolio::Loop dispatch callback */
static void Coolio_IOWatcher_dispatch_callback(VALUE self, int revents)
{   
  if(revents & EV_READ)
    rb_funcall(self, rb_intern("on_readable"), 0);
  else if(revents & EV_WRITE)
    rb_funcall(self, rb_intern("on_writable"), 0);
  else
    rb_raise(rb_eRuntimeError, "unknown revents value for ev_io: %d", revents);
}
