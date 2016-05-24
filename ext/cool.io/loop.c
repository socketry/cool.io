/*
 * Copyright (C) 2007-10 Tony Arcieri
 * You may redistribute this under the terms of the Ruby license.
 * See LICENSE for details
 */

#include <assert.h>
#include "ruby.h"
#include "rubysig.h"
#include "ev_wrap.h"

#include "cool.io.h"

static VALUE mCoolio = Qnil;
static VALUE cCoolio_Loop = Qnil;

static VALUE Coolio_Loop_allocate(VALUE klass);
static void Coolio_Loop_mark(struct Coolio_Loop *loop);
static void Coolio_Loop_free(struct Coolio_Loop *loop);

static VALUE Coolio_Loop_ev_loop_new(VALUE self, VALUE flags);
static VALUE Coolio_Loop_run_once(int argc, VALUE *argv, VALUE self);
static VALUE Coolio_Loop_run_nonblock(VALUE self);

static void Coolio_Loop_timeout_callback(struct ev_loop *ev_loop, struct ev_timer *timer, int revents);
static void Coolio_Loop_dispatch_events(struct Coolio_Loop *loop_data);

#define DEFAULT_EVENTBUF_SIZE 32
#define RUN_LOOP(loop_data, options) \
  loop_data->running = 1; \
  ev_loop(loop_data->ev_loop, options); \
  loop_data->running = 0;

/* 
 * Coolio::Loop represents an event loop.  Event watchers can be attached and
 * unattached.  When an event loop is run, all currently attached watchers
 * are monitored for events, and their respective callbacks are signaled
 * whenever events occur.
 */
void Init_coolio_loop()
{
  mCoolio = rb_define_module("Coolio");
  cCoolio_Loop = rb_define_class_under(mCoolio, "Loop", rb_cObject);
  rb_define_alloc_func(cCoolio_Loop, Coolio_Loop_allocate);

  rb_define_private_method(cCoolio_Loop, "ev_loop_new", Coolio_Loop_ev_loop_new, 1);
  rb_define_method(cCoolio_Loop, "run_once", Coolio_Loop_run_once, -1);
  rb_define_method(cCoolio_Loop, "run_nonblock", Coolio_Loop_run_nonblock, 0);
}

static VALUE Coolio_Loop_allocate(VALUE klass)
{
  struct Coolio_Loop *loop = (struct Coolio_Loop *)xmalloc(sizeof(struct Coolio_Loop));

  loop->ev_loop = 0;
  ev_init(&loop->timer, Coolio_Loop_timeout_callback);
  loop->running = 0;
  loop->events_received = 0;
  loop->eventbuf_size = DEFAULT_EVENTBUF_SIZE;
  loop->eventbuf = (struct Coolio_Event *)xmalloc(sizeof(struct Coolio_Event) * DEFAULT_EVENTBUF_SIZE);

  return Data_Wrap_Struct(klass, Coolio_Loop_mark, Coolio_Loop_free, loop);
}

static void Coolio_Loop_mark(struct Coolio_Loop *loop)
{
}

static void Coolio_Loop_free(struct Coolio_Loop *loop)
{
  if(!loop->ev_loop)
    return;

  ev_loop_destroy(loop->ev_loop);

  xfree(loop->eventbuf);
  xfree(loop);
}

/* Wrapper for populating a Coolio_Loop struct with a new event loop */
static VALUE Coolio_Loop_ev_loop_new(VALUE self, VALUE flags)
{
  struct Coolio_Loop *loop_data;
  Data_Get_Struct(self, struct Coolio_Loop, loop_data);

  if(loop_data->ev_loop)
    rb_raise(rb_eRuntimeError, "loop already initialized");

  loop_data->ev_loop = ev_loop_new(NUM2INT(flags));

  return Qnil;
}

/* libev callback for receiving events */
void Coolio_Loop_process_event(VALUE watcher, int revents)
{
  struct Coolio_Loop *loop_data;
  struct Coolio_Watcher *watcher_data;

  /* The Global VM lock isn't held right now, but hopefully
   * we can still do this safely */
  Data_Get_Struct(watcher, struct Coolio_Watcher, watcher_data);
  Data_Get_Struct(watcher_data->loop, struct Coolio_Loop, loop_data);

  /*  Well, what better place to explain how this all works than
   *  where the most wonky and convoluted stuff is going on!
   *
   *  Our call path up to here looks a little something like:
   *
   *  -> release GVL -> event syscall -> libev callback
   *  (GVL = Global VM Lock)             ^^^ You are here
   *
   *  We released the GVL in the Coolio_Loop_run_once() function
   *  so other Ruby threads can run while we make a blocking 
   *  system call (one of epoll, kqueue, port, poll, or select,
   *  depending on the platform).
   *
   *  More specifically, this is a libev callback abstraction
   *  called from a real libev callback in every watcher,
   *  hence this function not being static.  The real libev
   *  callbacks are event-specific and handled in a watcher.
   *
   *  For syscalls like epoll and kqueue, the kernel tells libev
   *  a pointer (to a structure with a pointer) to the watcher 
   *  object.  No data structure lookups are required at all
   *  (beyond structs), it's smooth O(1) sailing the entire way.  
   *  Then libev calls out to the watcher's callback, which
   *  calls this function.
   *
   *  Now, you may be curious: if the watcher already knew what
   *  event fired, why the hell is it telling the loop?  Why
   *  doesn't it just rb_funcall() the appropriate callback?
   *
   *  Well, the problem is the Global VM Lock isn't held right
   *  now, so we can't rb_funcall() anything.  In order to get
   *  it back we have to:
   *
   *  stash event and return -> acquire GVL -> dispatch to Ruby
   *
   *  Which is kinda ugly and confusing, but still gives us 
   *  an O(1) event loop whose heart is in the kernel itself. w00t!
   *
   *  So, stash the event in the loop's data struct.  When we return
   *  the ev_loop() call being made in the Coolio_Loop_run_once_blocking()
   *  function below will also return, at which point the GVL is
   *  reacquired and we can call out to Ruby */

  /* Grow the event buffer if it's too small */
  if(loop_data->events_received >= loop_data->eventbuf_size) {
    loop_data->eventbuf_size *= 2;
    loop_data->eventbuf = (struct Coolio_Event *)xrealloc(
        loop_data->eventbuf, 
        sizeof(struct Coolio_Event) * loop_data->eventbuf_size
        );
  }

  loop_data->eventbuf[loop_data->events_received].watcher = watcher;
  loop_data->eventbuf[loop_data->events_received].revents = revents;

  loop_data->events_received++;
}

/* Called whenever a timeout fires on the event loop */
static void Coolio_Loop_timeout_callback(struct ev_loop *ev_loop, struct ev_timer *timer, int revents)
{
  /* We don't actually need to do anything here, the mere firing of the
     timer is sufficient to interrupt the selector. However, libev still wants a callback */
}

/**
 *  call-seq:
 *    Coolio::Loop.run_once -> nil
 * 
 * Run the Coolio::Loop once, blocking until events are received.
 */
static VALUE Coolio_Loop_run_once(int argc, VALUE *argv, VALUE self)
{
  VALUE timeout;
  VALUE nevents;
  struct Coolio_Loop *loop_data;

  rb_scan_args(argc, argv, "01", &timeout);

  if (timeout != Qnil && NUM2DBL(timeout) < 0) {
    rb_raise(rb_eArgError, "time interval must be positive");
  }

  Data_Get_Struct(self, struct Coolio_Loop, loop_data);

  assert(loop_data->ev_loop && !loop_data->events_received);

  /* Implement the optional timeout (if any) as a ev_timer */
  /* Using the technique written at
     http://pod.tst.eu/http://cvs.schmorp.de/libev/ev.pod#code_ev_timer_code_relative_and_opti,
     the timer is not stopped/started everytime when timeout is specified, instead,
     the timer is stopped when timeout is not specified. */
  if (timeout != Qnil) {
    /* It seems libev is not a fan of timers being zero, so fudge a little */
    loop_data->timer.repeat = NUM2DBL(timeout) + 0.0001;
    ev_timer_again(loop_data->ev_loop, &loop_data->timer);
  } else {
    ev_timer_stop(loop_data->ev_loop, &loop_data->timer);
  }

  /* libev is patched to release the GIL when it makes its system call */
  RUN_LOOP(loop_data, EVLOOP_ONESHOT);

  Coolio_Loop_dispatch_events(loop_data);
  nevents = INT2NUM(loop_data->events_received);
  loop_data->events_received = 0;

  return nevents;
}

/**
 *  call-seq:
 *    Coolio::Loop.run_nonblock -> nil
 * 
 * Run the Coolio::Loop once, but return immediately if there are no pending events.
 */
static VALUE Coolio_Loop_run_nonblock(VALUE self)
{
  struct Coolio_Loop *loop_data;
  VALUE nevents;
  
  Data_Get_Struct(self, struct Coolio_Loop, loop_data);

  assert(loop_data->ev_loop && !loop_data->events_received);

  RUN_LOOP(loop_data, EVLOOP_NONBLOCK);  
  Coolio_Loop_dispatch_events(loop_data);
  
  nevents = INT2NUM(loop_data->events_received);
  loop_data->events_received = 0;
  
  return nevents;
}

static void Coolio_Loop_dispatch_events(struct Coolio_Loop *loop_data)
{
  int i;
  struct Coolio_Watcher *watcher_data;

  for(i = 0; i < loop_data->events_received; i++) {
    /* A watcher with pending events may have been detached from the loop
     * during the dispatch process.  If so, the watcher clears the pending
     * events, so skip over them */
    if(loop_data->eventbuf[i].watcher == Qnil)
      continue;

    Data_Get_Struct(loop_data->eventbuf[i].watcher, struct Coolio_Watcher, watcher_data);
    watcher_data->dispatch_callback(loop_data->eventbuf[i].watcher, loop_data->eventbuf[i].revents);
  }
}
