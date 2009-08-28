/*
 * Copyright (C) 2007 Tony Arcieri
 * You may redistribute this under the terms of the Ruby license.
 * See LICENSE for details
 */


#include "ruby.h" // allow for an error "OpenFile redeclared" to not exist, somehow.

#include "ev_wrap.h"


#include "rev.h"

static VALUE mRev = Qnil;

void Init_rev_ext() 
{
  /* Initializers for other modules */
  Init_rev_loop();
  Init_rev_watcher();
  Init_rev_io_watcher();
  Init_rev_timer_watcher();
  Init_rev_stat_watcher();
  Init_rev_utils();
  
#ifdef HAVE_OPENSSL_SSL_H
  Init_rev_ssl();
#endif
}
