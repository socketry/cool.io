#include "ruby.h"

#define EV_STANDALONE 1
#include "../libev/ev.c"

#include "rev.h"

VALUE Rev = Qnil;

void Init_rev_ext() 
{
  ev_set_allocator((void *(*)(void *, long))xrealloc);
  Rev = rb_define_module("Rev");

  /* Make libev version available in Ruby */
  rb_define_const(Rev, "LIBEV_VERSION", rb_sprintf("%d.%d", ev_version_major(), ev_version_minor()));

  /* Initializers for other modules */
  Init_rev_loop();
  Init_rev_watcher();
  Init_rev_io_watcher();
  Init_rev_timer_watcher();
}
