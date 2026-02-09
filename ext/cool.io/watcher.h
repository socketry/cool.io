/*
 * Copyright (C) 2007-10 Tony Arcieri
 * You may redistribute this under the terms of the Ruby license.
 * See LICENSE for details
 */

#ifndef WATCHER_H
#define WATCHER_H

#define Watcher_Attach(watcher_type, detach_func, watcher, loop) \
  struct Coolio_Watcher *watcher_data; \
  struct Coolio_Loop *loop_data; \
  \
  if(!rb_obj_is_kind_of(loop, cCoolio_Loop)) \
    rb_raise(rb_eArgError, "expected loop to be an instance of Coolio::Loop, not %s", RSTRING_PTR(rb_inspect(loop))); \
  \
  watcher_data = Coolio_Watcher_ptr(watcher); \
  loop_data = Coolio_Loop_ptr(loop); \
  \
  if(watcher_data->loop != Qnil) \
    detach_func(watcher); \
  \
  watcher_data->loop = loop; \
  ev_##watcher_type##_start(loop_data->ev_loop, &watcher_data->event_types.ev_##watcher_type); \
  rb_call_super(1, &loop)

#define Watcher_Detach(watcher_type, watcher) \
  struct Coolio_Watcher *watcher_data; \
  struct Coolio_Loop *loop_data; \
  \
  watcher_data = Coolio_Watcher_ptr(watcher); \
  \
  if(watcher_data->loop == Qnil) \
    rb_raise(rb_eRuntimeError, "not attached to a loop"); \
  \
  if (watcher_data->enabled == 0) { \
    /* Ignore because watcher was already detached. */ \
    return Qnil; \
  } \
  loop_data = Coolio_Loop_ptr(watcher_data->loop); \
  \
  ev_##watcher_type##_stop(loop_data->ev_loop, &watcher_data->event_types.ev_##watcher_type); \
  rb_call_super(0, 0)

#define Watcher_Enable(watcher_type, watcher) \
  struct Coolio_Watcher *watcher_data; \
  struct Coolio_Loop *loop_data; \
  \
  watcher_data = Coolio_Watcher_ptr(watcher); \
  \
  if(watcher_data->loop == Qnil) \
    rb_raise(rb_eRuntimeError, "not attached to a loop"); \
  \
  rb_call_super(0, 0); \
  \
  loop_data = Coolio_Loop_ptr(watcher_data->loop); \
  \
  ev_##watcher_type##_start(loop_data->ev_loop, &watcher_data->event_types.ev_##watcher_type)

#define Watcher_Disable(watcher_type, watcher) \
  struct Coolio_Watcher *watcher_data; \
  struct Coolio_Loop *loop_data; \
  \
  watcher_data = Coolio_Watcher_ptr(watcher); \
  \
  if(watcher_data->loop == Qnil) \
    rb_raise(rb_eRuntimeError, "not attached to a loop"); \
  \
  rb_call_super(0, 0); \
  \
  loop_data = Coolio_Loop_ptr(watcher_data->loop); \
  \
  ev_##watcher_type##_stop(loop_data->ev_loop, &watcher_data->event_types.ev_##watcher_type)

#endif
