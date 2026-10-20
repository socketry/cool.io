/*
 * Copyright (C) 2007-10 Tony Arcieri
 * You may redistribute this under the terms of the Ruby license.
 * See LICENSE for details
 */

#ifndef WATCHER_H
#define WATCHER_H

#define Watcher_Detach(watcher_type, watcher) \
  struct Coolio_Watcher *watcher_data; \
  \
  watcher_data = Coolio_Watcher_ptr(watcher); \
  \
  if(watcher_data->loop == Qnil) \
    rb_raise(rb_eRuntimeError, "not attached to a loop"); \
  \
  if (watcher_data->enabled) { \
    rb_funcall(watcher, rb_intern("disable"), 0); \
  } \
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
  if (watcher_data->enabled) { \
    loop_data = Coolio_Loop_ptr(watcher_data->loop); \
    ev_##watcher_type##_stop(loop_data->ev_loop, &watcher_data->event_types.ev_##watcher_type); \
  } \
  rb_call_super(0, 0);

#endif
