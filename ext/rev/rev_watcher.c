/*
 * Copyright (C) 2007 Tony Arcieri
 * You may redistribute this under the terms of the Ruby license.
 * See LICENSE for details
 */

#include "ruby.h"

#define EV_STANDALONE 1
#include "../libev/ev.h"

#include "rev.h"

/* Module and object handles */
static VALUE mRev = Qnil;
static VALUE cRev_Watcher = Qnil;

/* Data allocators and deallocators */
static VALUE Rev_Watcher_allocate(VALUE klass);
static void Rev_Watcher_mark(struct Rev_Watcher *watcher);
static void Rev_Watcher_free(struct Rev_Watcher *watcher);

/* Method implementations */
static VALUE Rev_Watcher_initialize(VALUE self);
static VALUE Rev_Watcher_attach(VALUE self, VALUE loop);
static VALUE Rev_Watcher_detach(VALUE self);
static VALUE Rev_Watcher_enable(VALUE self);
static VALUE Rev_Watcher_disable(VALUE self);
static VALUE Rev_Watcher_evloop(VALUE self);
static VALUE Rev_Watcher_attached(VALUE self);
static VALUE Rev_Watcher_enabled(VALUE self);

/* 
 * Watchers are Rev's event observers.  They contain a set of callback
 * methods prefixed by on_* which fire whenever events occur.
 *
 * In order for a watcher to fire events it must be attached to a running
 * loop.  Every watcher has an attach and detach method to control which
 * loop it's associated with.
 *
 * Watchers also have an enable and disable method.  This allows a watcher
 * to temporarily ignore certain events while remaining attached to a given
 * loop.  This is good for watchers which need to be toggled on and off.
 */
void Init_rev_watcher()
{
  mRev = rb_define_module("Rev");
  cRev_Watcher = rb_define_class_under(mRev, "Watcher", rb_cObject);
  rb_define_alloc_func(cRev_Watcher, Rev_Watcher_allocate);

  rb_define_method(cRev_Watcher, "initialize", Rev_Watcher_initialize, 0);
  rb_define_method(cRev_Watcher, "attach", Rev_Watcher_attach, 1);
  rb_define_method(cRev_Watcher, "detach", Rev_Watcher_detach, 0);
  rb_define_method(cRev_Watcher, "enable", Rev_Watcher_enable, 0);
  rb_define_method(cRev_Watcher, "disable", Rev_Watcher_disable, 0);
  rb_define_method(cRev_Watcher, "evloop", Rev_Watcher_evloop, 0);
  rb_define_method(cRev_Watcher, "attached?", Rev_Watcher_attached, 0);
  rb_define_method(cRev_Watcher, "enabled?", Rev_Watcher_enabled, 0);
}

static VALUE Rev_Watcher_allocate(VALUE klass)
{
  struct Rev_Watcher *watcher_data = (struct Rev_Watcher *)xmalloc(sizeof(struct Rev_Watcher));

  watcher_data->loop = Qnil;
  watcher_data->enabled = 0;

  return Data_Wrap_Struct(klass, Rev_Watcher_mark, Rev_Watcher_free, watcher_data);
}

static void Rev_Watcher_mark(struct Rev_Watcher *watcher_data)
{
  if(watcher_data->loop != Qnil)
    rb_gc_mark(watcher_data->loop);
}

static void Rev_Watcher_free(struct Rev_Watcher *watcher_data)
{
  xfree(watcher_data);
}

static VALUE Rev_Watcher_initialize(VALUE self)
{
  rb_raise(rb_eRuntimeError, "watcher base class should not be initialized directly");
}

/**
 *  call-seq:
 *    Rev::Watcher.attach(loop) -> Rev::Watcher
 * 
 * Attach the watcher to the given Rev::Loop.  If the watcher is already attached
 * to a loop, detach it from the old one and attach it to the new one.
 */
static VALUE Rev_Watcher_attach(VALUE self, VALUE loop)
{
  VALUE loop_watchers, active_watchers;
  struct Rev_Watcher *watcher_data;

  Data_Get_Struct(self, struct Rev_Watcher, watcher_data);
  watcher_data->enabled = 1;
    
  loop_watchers = rb_iv_get(loop, "@watchers");

  if(loop_watchers == Qnil) {
    loop_watchers = rb_ary_new();
    rb_iv_set(loop, "@watchers", loop_watchers);
  }

  /* Add us to the loop's array of active watchers.  This is mainly done
   * to keep the VM from garbage collecting watchers that are associated
   * with a loop (and also lets you see within Ruby which watchers are
   * associated with a given loop), but isn't really necessary for any 
   * other reason */
  rb_ary_push(loop_watchers, self);

  active_watchers = rb_iv_get(loop, "@active_watchers");
  if(active_watchers == Qnil)
    active_watchers = INT2NUM(1);
  else
    active_watchers = INT2NUM(NUM2INT(active_watchers) + 1);
  rb_iv_set(loop, "@active_watchers", active_watchers);

  return self;
}

/**
 *  call-seq:
 *    Rev::Watcher.detach -> Rev::Watcher
 * 
 * Detach the watcher from its current Rev::Loop.
 */
static VALUE Rev_Watcher_detach(VALUE self)
{
  struct Rev_Watcher *watcher_data;
  struct Rev_Loop *loop_data;
  VALUE loop_watchers;
  int i;

  Data_Get_Struct(self, struct Rev_Watcher, watcher_data);

  if(watcher_data->loop == Qnil)
    rb_raise(rb_eRuntimeError, "not attached to a loop");

  loop_watchers = rb_iv_get(watcher_data->loop, "@watchers");

  /* Remove us from the loop's array of active watchers.  This likely
   * has negative performance and scalability characteristics as this
   * isn't an O(1) operation.  Hopefully there's a better way... */
  rb_ary_delete(loop_watchers, self);

  if(watcher_data->enabled) {
    rb_iv_set(
        watcher_data->loop, 
        "@active_watchers",
        INT2NUM(NUM2INT(rb_iv_get(watcher_data->loop, "@active_watchers")) - 1)
    );
  }

  watcher_data->enabled = 0;

  Data_Get_Struct(watcher_data->loop, struct Rev_Loop, loop_data);

  /* Iterate through the events in the loop's event buffer.  If there
   * are any pending events from this watcher, mark them NULL.  The
   * dispatch loop will skip them.  This prevents watchers earlier
   * in the event buffer from detaching others which may have pending
   * events in the buffer but get garbage collected in the meantime */
  for(i = 0; i < loop_data->events_received; i++) {
    if(loop_data->eventbuf[i].watcher == self)
      loop_data->eventbuf[i].watcher = Qnil;
  }

  watcher_data->loop = Qnil;

  return self;
}

/**
 *  call-seq:
 *    Rev::Watcher.enable -> Rev::Watcher
 * 
 * Re-enable a watcher which has been temporarily disabled.  See the
 * disable method for a more thorough explanation.
 */
static VALUE Rev_Watcher_enable(VALUE self)
{
  struct Rev_Watcher *watcher_data;
  Data_Get_Struct(self, struct Rev_Watcher, watcher_data);

  if(watcher_data->enabled)
    rb_raise(rb_eRuntimeError, "already enabled");

  watcher_data->enabled = 1;

  rb_iv_set(
      watcher_data->loop, 
      "@active_watchers",
      INT2NUM(NUM2INT(rb_iv_get(watcher_data->loop, "@active_watchers")) + 1)
  );

  return self;
}

/**
 *  call-seq:
 *    Rev::Watcher.disable -> Rev::Watcher
 * 
 * Temporarily disable an event watcher which is attached to a loop.  
 * This is useful if you wish to toggle event monitoring on and off.  
 */
static VALUE Rev_Watcher_disable(VALUE self)
{
  struct Rev_Watcher *watcher_data;
  Data_Get_Struct(self, struct Rev_Watcher, watcher_data);

  if(!watcher_data->enabled)
    rb_raise(rb_eRuntimeError, "already disabled");

  watcher_data->enabled = 0;

  rb_iv_set(
      watcher_data->loop, 
      "@active_watchers",
      INT2NUM(NUM2INT(rb_iv_get(watcher_data->loop, "@active_watchers")) - 1)
  );

  return self;
}

/**
 *  call-seq:
 *    Rev::Watcher.evloop -> Rev::Loop
 * 
 * Return the loop to which we're currently attached
 */
static VALUE Rev_Watcher_evloop(VALUE self)
{
  struct Rev_Watcher *watcher_data;

  Data_Get_Struct(self, struct Rev_Watcher, watcher_data);
  return watcher_data->loop;
}

/**
 *  call-seq:
 *    Rev::Watcher.attached? -> Boolean
 * 
 * Is the watcher currently attached to an event loop?
 */
static VALUE Rev_Watcher_attached(VALUE self)
{
  return Rev_Watcher_evloop(self) != Qnil;
}

/**
 *  call-seq:
 *    Rev::Watcher.enabled? -> Boolean
 * 
 * Is the watcher currently enabled?
 */
static VALUE Rev_Watcher_enabled(VALUE self)
{
	struct Rev_Watcher *watcher_data;
  Data_Get_Struct(self, struct Rev_Watcher, watcher_data);
  
	return watcher_data->enabled ? Qtrue : Qfalse;
}
