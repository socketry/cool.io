/*
 * Copyright (C) 2007 Tony Arcieri
 * You may redistribute this under the terms of the Ruby license.
 * See LICENSE for details
 */

#include <assert.h>
#include "ruby.h"
#include "rubysig.h"

#define EV_STANDALONE 1
#include "../libev/ev.h"

#include "rev.h"

static VALUE mRev = Qnil;
static VALUE cRev_Loop = Qnil;

static VALUE Rev_Loop_allocate(VALUE klass);
static void Rev_Loop_mark(struct Rev_Loop *loop);
static void Rev_Loop_free(struct Rev_Loop *loop);

static VALUE Rev_Loop_initialize(VALUE self);
static VALUE Rev_Loop_ev_loop_new(VALUE self, VALUE flags);
static VALUE Rev_Loop_run_once(VALUE self);
static VALUE Rev_Loop_run_nonblock(VALUE self);

static void Rev_Loop_ev_loop_oneshot(struct Rev_Loop *loop_data);
static void Rev_Loop_dispatch_events(struct Rev_Loop *loop_data);

#define DEFAULT_EVENTBUF_SIZE 32
#define RUN_LOOP(loop_data, options) \
  loop_data->running = 1; \
  ev_loop(loop_data->ev_loop, options); \
  loop_data->running = 0;

/* 
 * Rev::Loop represents an event loop.  Event watchers can be attached and
 * unattached.  When an event loop is run, all currently attached watchers
 * are monitored for events, and their respective callbacks are signaled
 * whenever events occur.
 */
void Init_rev_loop()
{
  mRev = rb_define_module("Rev");
  cRev_Loop = rb_define_class_under(mRev, "Loop", rb_cObject);
  rb_define_alloc_func(cRev_Loop, Rev_Loop_allocate);
 
  rb_define_method(cRev_Loop, "initialize", Rev_Loop_initialize, 0);
  rb_define_private_method(cRev_Loop, "ev_loop_new", Rev_Loop_ev_loop_new, 1);
  rb_define_method(cRev_Loop, "run_once", Rev_Loop_run_once, 0);
  rb_define_method(cRev_Loop, "run_nonblock", Rev_Loop_run_nonblock, 0);
}

static VALUE Rev_Loop_allocate(VALUE klass)
{
  struct Rev_Loop *loop = (struct Rev_Loop *)xmalloc(sizeof(struct Rev_Loop));

  loop->ev_loop = 0;
  loop->running = 0;
  loop->events_received = 0;
  loop->eventbuf_size = DEFAULT_EVENTBUF_SIZE;
  loop->eventbuf = (struct Rev_Event *)xmalloc(sizeof(struct Rev_Event) * DEFAULT_EVENTBUF_SIZE);

  return Data_Wrap_Struct(klass, Rev_Loop_mark, Rev_Loop_free, loop);
}

static void Rev_Loop_mark(struct Rev_Loop *loop)
{
}

static void Rev_Loop_free(struct Rev_Loop *loop)
{
  if(!loop->ev_loop)
    return;

  ev_loop_destroy(loop->ev_loop);

  xfree(loop->eventbuf);
  xfree(loop);
}

static VALUE Rev_Loop_initialize(VALUE self)
{
  Rev_Loop_ev_loop_new(self, INT2NUM(0));
}

/* Wrapper for populating a Rev_Loop struct with a new event loop */
static VALUE Rev_Loop_ev_loop_new(VALUE self, VALUE flags)
{
  struct Rev_Loop *loop_data;
  Data_Get_Struct(self, struct Rev_Loop, loop_data);

  if(loop_data->ev_loop)
    rb_raise(rb_eRuntimeError, "loop already initialized");

  loop_data->ev_loop = ev_loop_new(NUM2INT(flags));

  return Qnil;
}

/* libev callback for receiving events */
void Rev_Loop_process_event(VALUE watcher, int revents)
{
  struct Rev_Loop *loop_data;
  struct Rev_Watcher *watcher_data;

  /* The Global VM lock isn't held right now, but hopefully
   * we can still do this safely */
  Data_Get_Struct(watcher, struct Rev_Watcher, watcher_data);
  Data_Get_Struct(watcher_data->loop, struct Rev_Loop, loop_data);

  /*  Well, what better place to explain how this all works than
   *  where the most wonky and convoluted stuff is going on!
   *
   *  Our call path up to here looks a little something like:
   *
   *  -> release GVL -> event syscall -> libev callback
   *  (GVL = Global VM Lock)             ^^^ You are here
   *
   *  We released the GVL in the Rev_Loop_run_once() function
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
   *  the ev_loop() call being made in the Rev_Loop_run_once_blocking()
   *  function below will also return, at which point the GVL is
   *  reacquired and we can call out to Ruby */

  /* Grow the event buffer if it's too small */
  if(loop_data->events_received >= loop_data->eventbuf_size) {
    loop_data->eventbuf_size *= 2;
    loop_data->eventbuf = (struct Rev_Event *)xrealloc(
        loop_data->eventbuf, 
        sizeof(struct Rev_Event) * loop_data->eventbuf_size
        );
  }

  loop_data->eventbuf[loop_data->events_received].watcher = watcher;
  loop_data->eventbuf[loop_data->events_received].revents = revents;

  loop_data->events_received++;
}

/**
 *  call-seq:
 *    Rev::Loop.run_once -> nil
 * 
 * Run the Rev::Loop once, blocking until events are received.
 */
static VALUE Rev_Loop_run_once(VALUE self)
{
  struct Rev_Loop *loop_data;
  VALUE nevents;
  
  Data_Get_Struct(self, struct Rev_Loop, loop_data);

  assert(loop_data->ev_loop && !loop_data->events_received);
  
  Rev_Loop_ev_loop_oneshot(loop_data);  
  Rev_Loop_dispatch_events(loop_data);
  
  nevents = INT2NUM(loop_data->events_received);
  loop_data->events_received = 0;
  
  return nevents;
}

/* Ruby 1.9 supports blocking system calls through rb_thread_blocking_region() */
#ifdef HAVE_RB_THREAD_BLOCKING_REGION
#define HAVE_EV_LOOP_ONESHOT
static VALUE Rev_Loop_ev_loop_oneshot_blocking(void *ptr) 
{
  /* The libev loop has now escaped through the Global VM Lock unscathed! */
  struct Rev_Loop *loop_data = (struct Rev_Loop *)ptr;

  RUN_LOOP(loop_data, EVLOOP_ONESHOT);
  
  return Qnil;
}

static void Rev_Loop_ev_loop_oneshot(struct Rev_Loop *loop_data)
{
  /* Use Ruby 1.9's rb_thread_blocking_region call to make a blocking system call */
  rb_thread_blocking_region(Rev_Loop_ev_loop_oneshot_blocking, loop_data, RB_UBF_DFL, 0);
}
#endif

/* Ruby 1.8 requires us to periodically run the event loop then defer back to
 * the green threads scheduler */
#ifndef HAVE_EV_LOOP_ONESHOT
#define BLOCKING_INTERVAL 0.01 /* Block for 10ms at a time */

/* Stub for scheduler's ev_timer callback */
static void timer_callback(struct ev_loop *ev_loop, struct ev_timer *timer, int revents)
{
}

/* Run the event loop, calling rb_thread_schedule every 10ms */
static void Rev_Loop_ev_loop_oneshot(struct Rev_Loop *loop_data)
{
  struct ev_timer timer;
  struct timeval tv;

  /* Set up an ev_timer to unblock the loop every 10ms */
  ev_timer_init(&timer, timer_callback, BLOCKING_INTERVAL, BLOCKING_INTERVAL);
  ev_timer_start(loop_data->ev_loop, &timer);

  /* Loop until we receive events */
  while(!loop_data->events_received) {
    TRAP_BEG;
    RUN_LOOP(loop_data, EVLOOP_ONESHOT);
    TRAP_END;

    rb_thread_schedule();
  }

  ev_timer_stop(loop_data->ev_loop, &timer);
}
#endif

/**
 *  call-seq:
 *    Rev::Loop.run_nonblock -> nil
 * 
 * Run the Rev::Loop once, but return immediately if there are no pending events.
 */
static VALUE Rev_Loop_run_nonblock(VALUE self)
{
  struct Rev_Loop *loop_data;
  VALUE nevents;
  
  Data_Get_Struct(self, struct Rev_Loop, loop_data);

  assert(loop_data->ev_loop && !loop_data->events_received);

  RUN_LOOP(loop_data, EVLOOP_NONBLOCK);  
  Rev_Loop_dispatch_events(loop_data);
  
  nevents = INT2NUM(loop_data->events_received);
  loop_data->events_received = 0;
  
  return nevents;
}

static void Rev_Loop_dispatch_events(struct Rev_Loop *loop_data)
{
  int i;
  struct Rev_Watcher *watcher_data;

  for(i = 0; i < loop_data->events_received; i++) {
    /* A watcher with pending events may have been detached from the loop
     * during the dispatch process.  If so, the watcher clears the pending
     * events, so skip over them */
    if(loop_data->eventbuf[i].watcher == Qnil)
      continue;

    Data_Get_Struct(loop_data->eventbuf[i].watcher, struct Rev_Watcher, watcher_data);
    watcher_data->dispatch_callback(loop_data->eventbuf[i].watcher, loop_data->eventbuf[i].revents);
  }
}