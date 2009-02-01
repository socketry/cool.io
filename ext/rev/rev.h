/*
 * Copyright (C) 2007 Tony Arcieri
 * You may redistribute this under the terms of the Ruby license.
 * See LICENSE for details
 */

#ifndef REV_H
#define REV_H

#include "ruby.h"
#include "rubyio.h"

#ifdef GetReadFile
#define FPTR_TO_FD(fptr) (fileno(GetReadFile(fptr)))
#else

#if !HAVE_RB_IO_T || (RUBY_VERSION_MAJOR == 1 && RUBY_VERSION_MINOR == 8)
#define FPTR_TO_FD(fptr) fileno(fptr->f)
#else
#define FPTR_TO_FD(fptr) fptr->fd
#endif

#endif

struct Rev_Event
{
  /* These values are used to extract events from libev callbacks */
  VALUE watcher;
  int revents;
};

struct Rev_Loop 
{
  struct ev_loop *ev_loop;

  int running;
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

  int enabled;
  VALUE loop;

  void (*dispatch_callback)(VALUE self, int revents);
};

void Rev_Loop_process_event(VALUE watcher, int revents);

#endif
