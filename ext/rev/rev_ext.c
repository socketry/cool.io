/*
 * Copyright (C) 2007 Tony Arcieri
 * You may redistribute this under the terms of the Ruby license.
 * See LICENSE for details
 */


#include "ruby.h"
#include "ev_wrap.h"
#include "../libev/ev.c"

#include "rev.h"

static VALUE mRev = Qnil;
static VALUE cRev_Watcher = Qnil;
static VALUE Rev_IOWatcher_attach(VALUE self, VALUE loop)
{
          unsigned long arg;
	      printf("in a watcher muhaha %d",  NUM2INT(loop));
	          int answer = ioctlsocket(   NUM2INT(loop), FIONREAD, &arg);
		      printf("got answer %d", answer);
		          return self;
 }

void Init_rev_ext() 
{
  ev_set_allocator((void *(*)(void *, long))xrealloc);
    mRev = rb_define_module("Rev2");
          cRev_Watcher = rb_define_class_under(mRev, "Watchah", rb_cObject);
	            rb_define_method(cRev_Watcher, "attach", Rev_IOWatcher_attach, 1);
  /* Initializers for other modules */
  Init_rev_loop();
  Init_rev_watcher();
  Init_rev_io_watcher();
  Init_rev_timer_watcher();
  Init_rev_buffer();
  Init_rev_utils();
  
#ifdef HAVE_OPENSSL_SSL_H
  Init_rev_ssl();
#endif
}
