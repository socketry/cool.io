#include <assert.h>

#include "ruby.h"

#define EV_STANDALONE 1
#include "../libev/ev.h"

#include "rev.h"

/* Module and object handles */
extern VALUE Rev;
VALUE Rev_Loop = Qnil;

/* Data allocators and deallocators */
static VALUE Rev_Loop_allocate(VALUE klass);
static void Rev_Loop_mark(struct Rev_Loop *loop);
static void Rev_Loop_free(struct Rev_Loop *loop);

/* Method implementations */
static VALUE Rev_Loop_ev_loop_new(VALUE self, VALUE flags);
static VALUE Rev_Loop_run_once(VALUE self);
static VALUE Rev_Loop_run_nonblock(VALUE self);

static void Rev_Loop_dispatch_event(struct Rev_Loop *loop_data);

void Init_rev_loop()
{		
  Rev_Loop = rb_define_class_under(Rev, "Loop", rb_cObject);
  rb_define_alloc_func(Rev_Loop, Rev_Loop_allocate);

  rb_define_private_method(Rev_Loop, "ev_loop_new", Rev_Loop_ev_loop_new, 1);
  rb_define_method(Rev_Loop, "run_once", Rev_Loop_run_once, 0);
  rb_define_method(Rev_Loop, "run_nonblock", Rev_Loop_run_nonblock, 0);
}

static VALUE Rev_Loop_allocate(VALUE klass)
{
  struct Rev_Loop *loop = (struct Rev_Loop *)xmalloc(sizeof(struct Rev_Loop));

  loop->ev_loop = 0;
  loop->default_loop = 0;

  return Data_Wrap_Struct(klass, Rev_Loop_mark, Rev_Loop_free, loop);
}

static void Rev_Loop_mark(struct Rev_Loop *loop)
{
}

static void Rev_Loop_free(struct Rev_Loop *loop)
{
  if(!loop->ev_loop)
    return;

  if(loop->default_loop)
    ev_default_destroy();
  else
    ev_loop_destroy(loop->ev_loop);

  xfree(loop);
}

/* Wrapper for populating a Rev_Loop struct with a new event loop */
VALUE Rev_Loop_ev_loop_new(VALUE self, VALUE flags)
{
  struct Rev_Loop *loop;
  Data_Get_Struct(self, struct Rev_Loop, loop);

  if(loop->ev_loop)
    rb_raise(rb_eRuntimeError, "loop already initialized");

  loop->ev_loop = ev_loop_new(NUM2INT(flags));
  loop->default_loop = 0;

  return Qnil;
}

/* libev callback for receiving events */
void Rev_Loop_process_event(VALUE watcher, int revents)
{
  struct Rev_Loop *loop_data;
  struct Rev_Watcher *watcher_data;

  Data_Get_Struct(watcher, struct Rev_Watcher, watcher_data);
  Data_Get_Struct(watcher_data->loop, struct Rev_Loop, loop_data);

  /*
     And here's where things start to get a little nasty...

     We need to store the watcher and returned events to dispatch them to Ruby.

     Right now we're inside a callback being made from ev_loop().  In the
     general case, this is probably being made from the rev_loop_blocking
     function below.

     This function is in turn a callback from rb_thread_blocking_region, which
     has released Ruby's Global VM Lock.  This means we're pretty limited in
     what we can do.

     To return the event so it can be dispatched back into Ruby when the
     Global VM Lock is held by the current thread, it's packed up into the
     Rev_Loop struct, then immediately dispatched into Ruby as soon as the
     blocking call completes and the lock is held again.

     Obviously this is a lousy way to couple and a whole can of worms in terms 
     of thread safety.  Maybe there's a better approach...
     */

  assert(!loop_data->active_watcher); /* We expect only one event per iteration of the ev_loop */

  loop_data->active_watcher = watcher;
  loop_data->revents = revents;
}

static VALUE rev_loop_blocking(void *ptr) {

  struct ev_loop *loop = (struct ev_loop *)ptr;

  ev_loop(loop, EVLOOP_ONESHOT);
  return Qnil;
}

/**
 *  call-seq:
 *    Rev::Loop.run_once -> nil
 * 
 * Run the Rev::Loop once, blocking until the next event is received.
 */
static VALUE Rev_Loop_run_once(VALUE self)
{
  struct Rev_Loop *loop_data;

  Data_Get_Struct(self, struct Rev_Loop, loop_data);

  if(!loop_data->ev_loop)
    rb_raise(rb_eRuntimeError, "loop not initialized");

  loop_data->active_watcher = 0;
  rb_thread_blocking_region(rev_loop_blocking, loop_data->ev_loop, RB_UBF_DFL, 0);

  Rev_Loop_dispatch_event(loop_data);

  return Qnil;
}

/**
 *  call-seq:
 *    Rev::Loop.run_once -> nil
 * 
 * Run the Rev::Loop once, but return immediately if there are no pending events.
 */
static VALUE Rev_Loop_run_nonblock(VALUE self)
{
  struct Rev_Loop *loop_data;

  Data_Get_Struct(self, struct Rev_Loop, loop_data);

  if(!loop_data->ev_loop)
    rb_raise(rb_eRuntimeError, "loop not initialized");

  loop_data->active_watcher = 0;
  ev_loop(loop_data->ev_loop, EVLOOP_NONBLOCK);

  Rev_Loop_dispatch_event(loop_data);

  return Qnil;
}

static void Rev_Loop_dispatch_event(struct Rev_Loop *loop_data)
{
  struct Rev_Watcher *watcher_data;

  if(!loop_data->active_watcher)
    return;

  Data_Get_Struct(loop_data->active_watcher, struct Rev_Watcher, watcher_data);
  watcher_data->dispatch_callback(loop_data->active_watcher, loop_data->revents);
}
