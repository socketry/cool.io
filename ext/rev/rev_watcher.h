/*
 * Copyright (C) 2007 Tony Arcieri
 * You may redistribute this under the terms of the Ruby license.
 * See LICENSE for details
 */

#ifndef REV_WATCHER_H
#define REV_WATCHER_H

#define Watcher_Attach(watcher_type, detach_func, watcher, loop) \
  struct Rev_Watcher *watcher_data; \
  struct Rev_Loop *loop_data; \
  \
  if(!rb_obj_is_kind_of(loop, cRev_Loop)) \
    rb_raise(rb_eArgError, "expected loop to be an instance of Rev::Loop"); \
  \
  Data_Get_Struct(watcher, struct Rev_Watcher, watcher_data); \
  Data_Get_Struct(loop, struct Rev_Loop, loop_data); \
  \
  if(watcher_data->loop != Qnil) \
    detach_func(watcher); \
  \
  watcher_data->loop = loop; \
  watcher_data->enabled = 1; \
  loop_data->active_watchers++; \
  \
  ev_##watcher_type##_start(loop_data->ev_loop, &watcher_data->event_types.ev_##watcher_type); \
  Rev_Loop_attach_watcher(loop, watcher)

#define Watcher_Detach(watcher_type, watcher) \
  struct Rev_Watcher *watcher_data; \
  struct Rev_Loop *loop_data; \
  int i; \
  \
  Data_Get_Struct(watcher, struct Rev_Watcher, watcher_data); \
  \
  if(watcher_data->loop == Qnil) \
    rb_raise(rb_eRuntimeError, "not attached to a loop"); \
  \
  Data_Get_Struct(watcher_data->loop, struct Rev_Loop, loop_data); \
  \
  ev_##watcher_type##_stop(loop_data->ev_loop, &watcher_data->event_types.ev_##watcher_type); \
  Rev_Loop_detach_watcher(watcher_data->loop, watcher); \
  \
  if(watcher_data->enabled) loop_data->active_watchers--;\
  watcher_data->enabled = 0;\
  \
  Rev_Watcher_clear_pending_events(loop_data, watcher); \
  watcher_data->loop = Qnil
  
#define Watcher_Enable(watcher_type, watcher) \
  struct Rev_Watcher *watcher_data; \
  struct Rev_Loop *loop_data; \
  \
  Data_Get_Struct(watcher, struct Rev_Watcher, watcher_data); \
  \
  if(watcher_data->loop == Qnil) \
    rb_raise(rb_eRuntimeError, "not attached to a loop"); \
  \
  if(watcher_data->enabled) \
    rb_raise(rb_eRuntimeError, "already enabled"); \
  \
  Data_Get_Struct(watcher_data->loop, struct Rev_Loop, loop_data); \
  \
  watcher_data->enabled = 1; \
  loop_data->active_watchers++;  \
  \
  ev_##watcher_type##_start(loop_data->ev_loop, &watcher_data->event_types.ev_##watcher_type)

#define Watcher_Disable(watcher_type, watcher) \
  struct Rev_Watcher *watcher_data; \
  struct Rev_Loop *loop_data; \
  \
  Data_Get_Struct(watcher, struct Rev_Watcher, watcher_data); \
  \
  if(watcher_data->loop == Qnil) \
    rb_raise(rb_eRuntimeError, "not attached to a loop"); \
  \
  if(!watcher_data->enabled) \
    rb_raise(rb_eRuntimeError, "already disabled"); \
  \
  Data_Get_Struct(watcher_data->loop, struct Rev_Loop, loop_data); \
  \
  watcher_data->enabled = 0; \
  loop_data->active_watchers--; \
  \
  ev_##watcher_type##_stop(loop_data->ev_loop, &watcher_data->event_types.ev_##watcher_type)
  
#endif