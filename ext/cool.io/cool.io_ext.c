/*
 * Copyright (C) 2007-10 Tony Arcieri
 * You may redistribute this under the terms of the Ruby license.
 * See LICENSE for details
 */


#include "ruby.h"

#include "ev_wrap.h"
#include "cool.io.h"

static VALUE mCoolio = Qnil;

/* Initialize the coolness */
void Init_cool() 
{
  /* Initializers for other modules */
  Init_coolio_loop();
  Init_coolio_watcher();
  Init_coolio_iowatcher();
  Init_coolio_timer_watcher();
  Init_coolio_stat_watcher();
  Init_coolio_utils();
}