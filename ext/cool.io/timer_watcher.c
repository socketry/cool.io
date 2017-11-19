/*
 * Copyright (C) 2007 Tony Arcieri
 * You may redistribute this under the terms of the Ruby license.
 * See LICENSE for details
 */

#include "ruby.h"
#include "ev_wrap.h"

#include "cool.io.h"
#include "watcher.h"

static VALUE mCoolio = Qnil;
static VALUE cCoolio_Watcher = Qnil;
static VALUE cCoolio_TimerWatcher = Qnil;
static VALUE cCoolio_Loop = Qnil;

static VALUE Coolio_TimerWatcher_initialize(int argc, VALUE *argv, VALUE self);
static VALUE Coolio_TimerWatcher_attach(VALUE self, VALUE loop);
static VALUE Coolio_TimerWatcher_detach(VALUE self);
static VALUE Coolio_TimerWatcher_enable(VALUE self);
static VALUE Coolio_TimerWatcher_disable(VALUE self);
static VALUE Coolio_TimerWatcher_reset(VALUE self);
static VALUE Coolio_TimerWatcher_on_timer(VALUE self);

static void Coolio_TimerWatcher_libev_callback(struct ev_loop *ev_loop, struct ev_timer *timer, int revents);
static void Coolio_TimerWatcher_dispatch_callback(VALUE self, int revents);

/*
 * Coolio::TimerWatcher lets you create either one-shot or periodic timers which
 * run within Coolio's event loop.  It's useful for creating timeouts or
 * events which fire periodically.
 */
void Init_coolio_timer_watcher()
{ 
  mCoolio = rb_define_module("Coolio");
  cCoolio_Watcher = rb_define_class_under(mCoolio, "Watcher", rb_cObject);
  cCoolio_TimerWatcher = rb_define_class_under(mCoolio, "TimerWatcher", cCoolio_Watcher);
  cCoolio_Loop = rb_define_class_under(mCoolio, "Loop", rb_cObject);

  rb_define_method(cCoolio_TimerWatcher, "initialize", Coolio_TimerWatcher_initialize, -1);
  rb_define_method(cCoolio_TimerWatcher, "attach", Coolio_TimerWatcher_attach, 1);
  rb_define_method(cCoolio_TimerWatcher, "detach", Coolio_TimerWatcher_detach, 0);
  rb_define_method(cCoolio_TimerWatcher, "enable", Coolio_TimerWatcher_enable, 0);
  rb_define_method(cCoolio_TimerWatcher, "disable", Coolio_TimerWatcher_disable, 0);
  rb_define_method(cCoolio_TimerWatcher, "reset", Coolio_TimerWatcher_reset, 0);
  rb_define_method(cCoolio_TimerWatcher, "on_timer", Coolio_TimerWatcher_on_timer, 0);
}

/**
 *  call-seq:
 *    Coolio::TimerWatcher.initialize(interval, repeating = false) -> Coolio::TimerWatcher
 * 
 * Create a new Coolio::TimerWatcher for the given IO object and add it to the 
 * given Coolio::Loop.  Interval defines a duration in seconds to wait for events,
 * and can be specified as an Integer or Float.  Repeating is a boolean 
 * indicating whether the timer is one shot or should fire on the given 
 * interval.
 */
static VALUE Coolio_TimerWatcher_initialize(int argc, VALUE *argv, VALUE self)
{
	VALUE interval, repeating;
  struct Coolio_Watcher *watcher_data;

	rb_scan_args(argc, argv, "11", &interval, &repeating);
  interval = rb_convert_type(interval, T_FLOAT, "Float", "to_f");

  rb_iv_set(self, "@interval", interval);
  rb_iv_set(self, "@repeating", repeating);

  Data_Get_Struct(self, struct Coolio_Watcher, watcher_data);

  watcher_data->dispatch_callback = Coolio_TimerWatcher_dispatch_callback;
  ev_timer_init(
      &watcher_data->event_types.ev_timer, 
      Coolio_TimerWatcher_libev_callback, 
      NUM2DBL(interval), 
      repeating == Qtrue ? NUM2DBL(interval) : 0
  );  
  watcher_data->event_types.ev_timer.data = (void *)self;

  return Qnil;
}

/**
 *  call-seq:
 *    Coolio::TimerWatcher.attach(loop) -> Coolio::TimerWatcher
 * 
 * Attach the timer watcher to the given Coolio::Loop.  If the watcher is already
 * attached to a loop, detach it from the old one and attach it to the new one.
 */
static VALUE Coolio_TimerWatcher_attach(VALUE self, VALUE loop)
{
  ev_tstamp interval, timeout;
  struct Coolio_Loop *loop_data;
  struct Coolio_Watcher *watcher_data;
    
  if(!rb_obj_is_kind_of(loop, cCoolio_Loop))
    rb_raise(rb_eArgError, "expected loop to be an instance of Coolio::Loop, not %s", RSTRING_PTR(rb_inspect(loop)));

  Data_Get_Struct(loop, struct Coolio_Loop, loop_data);
  Data_Get_Struct(self, struct Coolio_Watcher, watcher_data);

  if(watcher_data->loop != Qnil)
    Coolio_TimerWatcher_detach(self);

  watcher_data->loop = loop;
  
  /* Calibrate timeout to account for potential drift */
	interval = NUM2DBL(rb_iv_get(self, "@interval"));
  timeout = interval + ev_time() - ev_now(loop_data->ev_loop);
  
  ev_timer_set(
    &watcher_data->event_types.ev_timer, 
    timeout, 
    rb_iv_get(self, "@repeating") == Qtrue ? interval : 0
  );

  ev_timer_start(loop_data->ev_loop, &watcher_data->event_types.ev_timer);
	rb_call_super(1, &loop);

  return self;  
}

/**
 *  call-seq:
 *    Coolio::TimerWatcher.detach -> Coolio::TimerWatcher
 * 
 * Detach the timer watcher from its current Coolio::Loop.
 */
static VALUE Coolio_TimerWatcher_detach(VALUE self)
{
  Watcher_Detach(timer, self);

  return self;
}

/**
 *  call-seq:
 *    Coolio::TimerWatcher.enable -> Coolio::TimerWatcher
 * 
 * Re-enable a timer watcher which has been temporarily disabled.  See the
 * disable method for a more thorough explanation.
 */
static VALUE Coolio_TimerWatcher_enable(VALUE self)
{
  Watcher_Enable(timer, self);

  return self;  
}

/**
 *  call-seq:
 *    Coolio::TimerWatcher.disable -> Coolio::TimerWatcher
 * 
 * Temporarily disable a timer watcher which is attached to a loop.  
 * This is useful if you wish to toggle event monitoring on and off.  
 */
static VALUE Coolio_TimerWatcher_disable(VALUE self)
{
  Watcher_Disable(timer, self);

  return self;
}

/**
 *  call-seq:
 *    Coolio::TimerWatcher#reset -> Coolio::TimerWatcher
 * 
 * Reset the TimerWatcher.  This behaves differently depending on if it's repeating.
 *
 * If the timer is pending, its pending status is cleared.
 * 
 * If the timer is attached but nonrepeating, stop it (as if it timed out)
 *
 * If the timer is repeating, reset it so it will fire again after its given interval
 */
static VALUE Coolio_TimerWatcher_reset(VALUE self)
{
  struct Coolio_Watcher *watcher_data;
  struct Coolio_Loop *loop_data;

  Data_Get_Struct(self, struct Coolio_Watcher, watcher_data);

  if(watcher_data->loop == Qnil)
    rb_raise(rb_eRuntimeError, "not attached to a loop");

  Data_Get_Struct(watcher_data->loop, struct Coolio_Loop, loop_data);

  ev_timer_again(loop_data->ev_loop, &watcher_data->event_types.ev_timer);

  return self;
}

/**
 *  call-seq:
 *    Coolio::TimerWatcher#on_timer -> nil
 * 
 * Called whenever the TimerWatcher fires
 */
static VALUE Coolio_TimerWatcher_on_timer(VALUE self)
{
  return Qnil;
}

/* libev callback */
static void Coolio_TimerWatcher_libev_callback(struct ev_loop *ev_loop, struct ev_timer *timer, int revents)
{
  Coolio_Loop_process_event((VALUE)timer->data, revents);
}

/* Coolio::Loop dispatch callback */
static void Coolio_TimerWatcher_dispatch_callback(VALUE self, int revents)
{ 
  if(revents & EV_TIMEOUT)
    rb_funcall(self, rb_intern("on_timer"), 0);
  else
    rb_raise(rb_eRuntimeError, "unknown revents value for ev_timer: %d", revents);
}
