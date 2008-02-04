/*
 * Copyright (C) 2007 Tony Arcieri
 * You may redistribute this under the terms of the Ruby license.
 * See LICENSE for details
 */

#include "ruby.h"

#define EV_STANDALONE 1
#include "../libev/ev.h"

#include "rev.h"
#include "rev_watcher.h"

/* Module and object handles */
static VALUE mRev = Qnil;
static VALUE cRev_Watcher = Qnil;
static VALUE cRev_AsyncWatcher = Qnil;
static VALUE cRev_Loop = Qnil;

/* Method implementations */
static VALUE Rev_AsyncWatcher_initialize(VALUE self);
static VALUE Rev_AsyncWatcher_attach(VALUE self, VALUE loop);
static VALUE Rev_AsyncWatcher_detach(VALUE self);
static VALUE Rev_AsyncWatcher_enable(VALUE self);
static VALUE Rev_AsyncWatcher_disable(VALUE self);
static VALUE Rev_AsyncWatcher_signal(VALUE self);
static VALUE Rev_AsyncWatcher_on_signal(VALUE self);

/* Callbacks */
static void Rev_AsyncWatcher_libev_callback(struct ev_loop *ev_loop, struct ev_async *async, int revents);
static void Rev_AsyncWatcher_dispatch_callback(VALUE self, int revents);

/*
 * Rev::AsyncWatcher lets you feed events directly into the event loop.  If
 * the event is fed to a thread which is sleeping it will be woken up.
 */
void Init_rev_async_watcher()
{ 
  mRev = rb_define_module("Rev");
  cRev_Watcher = rb_define_class_under(mRev, "Watcher", rb_cObject);
  cRev_AsyncWatcher = rb_define_class_under(mRev, "AsyncWatcher", cRev_Watcher);
  cRev_Loop = rb_define_class_under(mRev, "Loop", rb_cObject);

  rb_define_method(cRev_AsyncWatcher, "initialize", Rev_AsyncWatcher_initialize, 0);
  rb_define_method(cRev_AsyncWatcher, "attach", Rev_AsyncWatcher_attach, 1);
  rb_define_method(cRev_AsyncWatcher, "detach", Rev_AsyncWatcher_detach, 0);
  rb_define_method(cRev_AsyncWatcher, "enable", Rev_AsyncWatcher_enable, 0);
  rb_define_method(cRev_AsyncWatcher, "disable", Rev_AsyncWatcher_disable, 0);
  rb_define_method(cRev_AsyncWatcher, "signal", Rev_AsyncWatcher_signal, 0);
  rb_define_method(cRev_AsyncWatcher, "on_signal", Rev_AsyncWatcher_on_signal, 0);
}

/**
 *  call-seq:
 *    Rev::AsyncWatcher.initialize(interval, repeating = false) -> Rev::AsyncWatcher
 * 
 * Create a new Rev::AsyncWatcher
 */
static VALUE Rev_AsyncWatcher_initialize(VALUE self)
{
  struct Rev_Watcher *watcher_data;
  Data_Get_Struct(self, struct Rev_Watcher, watcher_data);

  watcher_data->dispatch_callback = Rev_AsyncWatcher_dispatch_callback;
  ev_async_init(
      &watcher_data->event_types.ev_async, 
      Rev_AsyncWatcher_libev_callback
  );  
  watcher_data->event_types.ev_async.data = (void *)self;

  return Qnil;
}

/**
 *  call-seq:
 *    Rev::AsyncWatcher.attach(loop) -> Rev::AsyncWatcher
 * 
 * Attach the async watcher to the given Rev::Loop.  If the watcher is already
 * attached to a loop, detach it from the old one and attach it to the new one.
 */
static VALUE Rev_AsyncWatcher_attach(VALUE self, VALUE loop)
{
  Watcher_Attach(async, Rev_AsyncWatcher_detach, self, loop);
  return self;  
}

/**
 *  call-seq:
 *    Rev::AsyncWatcher.detach -> Rev::AsyncWatcher
 * 
 * Detach the async watcher from its current Rev::Loop.
 */
static VALUE Rev_AsyncWatcher_detach(VALUE self)
{
  Watcher_Detach(async, self);
  return self;
}

/**
 *  call-seq:
 *    Rev::AsyncWatcher.enable -> Rev::AsyncWatcher
 * 
 * Re-enable a async watcher which has been temporarily disabled.  See the
 * disable method for a more thorough explanation.
 */
static VALUE Rev_AsyncWatcher_enable(VALUE self)
{
  Watcher_Enable(async, self);
  return self;  
}

/**
 *  call-seq:
 *    Rev::AsyncWatcher.disable -> Rev::AsyncWatcher
 * 
 * Temporarily disable a async watcher which is attached to a loop.  
 * This is useful if you wish to toggle event monitoring on and off.  
 */
static VALUE Rev_AsyncWatcher_disable(VALUE self)
{
  Watcher_Disable(async, self);

  return self;
}

/**
 *  call-seq:
 *    Rev::AsyncWatcher#signal -> Rev::AsyncWatcher
 *
 * Send a signal to the given AsyncWatcher.  This method is thread safe
 * and can be used to wake up another thread.
 */
static VALUE Rev_AsyncWatcher_signal(VALUE self)
{
  struct Rev_Watcher *watcher_data;
  struct Rev_Loop *loop_data;

  Data_Get_Struct(self, struct Rev_Watcher, watcher_data);

  if(watcher_data->loop == Qnil)
    rb_raise(rb_eRuntimeError, "not attached to a loop");

  Data_Get_Struct(watcher_data->loop, struct Rev_Loop, loop_data);
  ev_async_send(loop_data->ev_loop, &watcher_data->event_types.ev_async);

  return self;
}

/**
 *  call-seq:
 *    Rev::AsyncWatcher#on_signal -> nil
 * 
 * Called whenever the AsyncWatcher fires
 */
static VALUE Rev_AsyncWatcher_on_signal(VALUE self)
{
  return Qnil;
}

/* libev callback */
static void Rev_AsyncWatcher_libev_callback(struct ev_loop *ev_loop, struct ev_async *async, int revents)
{
  Rev_Loop_process_event((VALUE)async->data, revents);
}

/* Rev::Loop dispatch callback */
static void Rev_AsyncWatcher_dispatch_callback(VALUE self, int revents)
{ 
  if(revents & EV_ASYNC)
    rb_funcall(self, rb_intern("on_signal"), 0, 0);
  else
    rb_raise(rb_eRuntimeError, "unknown revents value for ev_async: %d", revents);
}
