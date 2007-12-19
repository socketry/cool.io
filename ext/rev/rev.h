#ifndef REV_H
#define REV_H

#include "ruby.h"

struct Rev_Event
{
  /* These values are used to extract events from libev callbacks */
  VALUE watcher;
  int revents;
};

struct Rev_Loop 
{
  struct ev_loop *ev_loop;
  int default_loop;

  int events_received;
  int eventbuf_size;
  struct Rev_Event *eventbuf;
};

struct Rev_Watcher
{
  union {
    struct ev_io ev_io;
    struct ev_timer ev_timer;
  } event_types;

  VALUE loop;
  void (*dispatch_callback)(VALUE self, int revents);
};

void Rev_Loop_process_event(VALUE watcher, int revents);

#endif
