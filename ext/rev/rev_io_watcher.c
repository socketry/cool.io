/*
 * Copyright (C) 2007 Tony Arcieri
 * You may redistribute this under the terms of the Ruby license.
 * See LICENSE for details
 */

#include "ruby.h"
#include "rubyio.h"

#define EV_STANDALONE 1
#include "../libev/ev.h"

#include "rev.h"
#include "rev_watcher.h"

/* Module and object handles */
static VALUE mRev = Qnil;
static VALUE cRev_Watcher = Qnil;
static VALUE cRev_Loop = Qnil;
static VALUE cRev_IOWatcher = Qnil;

/* Method implementations */
static VALUE Rev_IOWatcher_initialize(int argc, VALUE *argv, VALUE self);
static VALUE Rev_IOWatcher_attach(VALUE self, VALUE loop);
static VALUE Rev_IOWatcher_detach(VALUE self);
static VALUE Rev_IOWatcher_enable(VALUE self);
static VALUE Rev_IOWatcher_disable(VALUE self);
static VALUE Rev_IOWatcher_on_readable(VALUE self);
static VALUE Rev_IOWatcher_on_writable(VALUE self);

/* Callbacks */
static void Rev_IOWatcher_libev_callback(struct ev_loop *ev_loop, struct ev_io *io, int revents);
static void Rev_IOWatcher_dispatch_callback(VALUE self, int revents);

void Init_rev_io_watcher()
{
  mRev = rb_define_module("Rev");
  cRev_Watcher = rb_define_class_under(mRev, "Watcher", rb_cObject);
  cRev_IOWatcher = rb_define_class_under(mRev, "IOWatcher", cRev_Watcher);
  cRev_Loop = rb_define_class_under(mRev, "Loop", rb_cObject);

  rb_define_method(cRev_IOWatcher, "initialize", Rev_IOWatcher_initialize, -1);
  rb_define_method(cRev_IOWatcher, "attach", Rev_IOWatcher_attach, 1);
  rb_define_method(cRev_IOWatcher, "detach", Rev_IOWatcher_detach, 0);
  rb_define_method(cRev_IOWatcher, "enable", Rev_IOWatcher_enable, 0);
  rb_define_method(cRev_IOWatcher, "disable", Rev_IOWatcher_disable, 0);
  rb_define_method(cRev_IOWatcher, "on_readable", Rev_IOWatcher_on_readable, 0);
  rb_define_method(cRev_IOWatcher, "on_writable", Rev_IOWatcher_on_writable, 0);
}

/**
 *  call-seq:
 *    Rev::IOWatcher.initialize(IO, events = 'r') -> Rev::IOWatcher
 * 
 * Create a new Rev::IOWatcher for the given IO object and add it to the given Rev::Loop
 */
static VALUE Rev_IOWatcher_initialize(int argc, VALUE *argv, VALUE self)
{
	VALUE io, flags;
	char *flags_str;
	int events;
  rb_io_t *fptr;
  struct Rev_Watcher *watcher_data;

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

  GetOpenFile(rb_convert_type(io, T_FILE, "IO", "to_io"), fptr);
  Data_Get_Struct(self, struct Rev_Watcher, watcher_data);

  watcher_data->dispatch_callback = Rev_IOWatcher_dispatch_callback;
  ev_io_init(&watcher_data->event_types.ev_io, Rev_IOWatcher_libev_callback, fptr->fd, events);
  watcher_data->event_types.ev_io.data = (void *)self;

	return Qnil;
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

static VALUE Rev_IOWatcher_enable(VALUE self)
{
  Watcher_Enable(io, self);

  return self;  
}

static VALUE Rev_IOWatcher_disable(VALUE self)
{
  Watcher_Disable(io, self);

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
