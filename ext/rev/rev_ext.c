/*
 * Copyright (C) 2007 Tony Arcieri
 * You may redistribute this under the terms of the Ruby license.
 * See LICENSE for details
 */

#include "ruby.h"

#define EV_STANDALONE 1
#include "../libev/ev.c"

#include "rev.h"

static VALUE mRev = Qnil;

void Init_rev_ext() 
{
  ev_set_allocator((void *(*)(void *, long))xrealloc);
  mRev = rb_define_module("Rev");

  /* Make libev version available in Ruby */
  rb_define_const(mRev, "LIBEV_VERSION", rb_sprintf("%d.%d", ev_version_major(), ev_version_minor()));

  /* Initializers for other modules */
  Init_rev_loop();
  Init_rev_watcher();
  Init_rev_io_watcher();
  Init_rev_timer_watcher();
  Init_rev_buffer();
}
