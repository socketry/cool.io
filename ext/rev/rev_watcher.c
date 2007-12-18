#include "ruby.h"

#define EV_STANDALONE 1
#include "../libev/ev.h"

#include "rev.h"

/* Module and object handles */
extern VALUE Rev;
VALUE Rev_Watcher = Qnil;

/* Data allocators and deallocators */
static VALUE Rev_Watcher_allocate(VALUE klass);
static void Rev_Watcher_mark(struct Rev_Watcher *watcher);
static void Rev_Watcher_free(struct Rev_Watcher *watcher);

/* Method implementations */
static VALUE Rev_Watcher_initialize(VALUE self);
static VALUE Rev_Watcher_attach(VALUE self, VALUE loop);
static VALUE Rev_Watcher_detach(VALUE self);
static VALUE Rev_Watcher_evloop(VALUE self);

void Init_rev_watcher()
{
  Rev_Watcher = rb_define_class_under(Rev, "Watcher", rb_cObject);
  rb_define_alloc_func(Rev_Watcher, Rev_Watcher_allocate);

  rb_define_method(Rev_Watcher, "initialize", Rev_Watcher_initialize, 0);
  rb_define_method(Rev_Watcher, "attach", Rev_Watcher_attach, 1);
  rb_define_method(Rev_Watcher, "detach", Rev_Watcher_detach, 0);
  rb_define_method(Rev_Watcher, "evloop", Rev_Watcher_evloop, 0);
}

static VALUE Rev_Watcher_allocate(VALUE klass)
{
  struct Rev_Watcher *watcher_data = (struct Rev_Watcher *)xmalloc(sizeof(struct Rev_Watcher));

  watcher_data->loop = Qnil;

  return Data_Wrap_Struct(klass, Rev_Watcher_mark, Rev_Watcher_free, watcher_data);
}

static void Rev_Watcher_mark(struct Rev_Watcher *watcher_data)
{
  if(watcher_data->loop != Qnil)
    rb_gc_mark(watcher_data->loop);
}

static void Rev_Watcher_free(struct Rev_Watcher *watcher_data)
{
  xfree(watcher_data);
}

static VALUE Rev_Watcher_initialize(VALUE self)
{
  rb_raise(rb_eRuntimeError, "watcher base class should not be initialized directly");
}

/**
 *  call-seq:
 *    Rev::Watcher.attach(loop) -> Rev::Watcher
 * 
 * Attach the watcher to the given Rev::Loop.  If the watcher is already attached
 * to a loop, detach it from the old one and attach it to the new one.
 */
static VALUE Rev_Watcher_attach(VALUE self, VALUE loop)
{
  VALUE loop_watchers = rb_ivar_get(loop, rb_intern("@watchers"));

  if(loop_watchers == Qnil) {
    loop_watchers = rb_ary_new();
    rb_ivar_set(loop, rb_intern("@watchers"), loop_watchers);
  }

  rb_ary_push(loop_watchers, self);

  return self;
}

/**
 *  call-seq:
 *    Rev::Watcher.detach -> Rev::Watcher
 * 
 * Detach the watcher from its current Rev::Loop.
 */
static VALUE Rev_Watcher_detach(VALUE self)
{
  struct Rev_Watcher *watcher_data;
  VALUE loop_watchers;

  Data_Get_Struct(self, struct Rev_Watcher, watcher_data);

  if(watcher_data->loop == Qnil)
    rb_raise(rb_eRuntimeError, "not attached to a loop");

  loop_watchers = rb_ivar_get(watcher_data->loop, rb_intern("@watchers"));
  rb_ary_delete(loop_watchers, self);

  watcher_data->loop = Qnil;

  return self;
}

/**
 *  call-seq:
 *    Rev::Watcher.evloop -> Rev::Loop
 * 
 * Return the loop to which we're currently attached
 */
static VALUE Rev_Watcher_evloop(VALUE self)
{
  struct Rev_Watcher *watcher_data;

  Data_Get_Struct(self, struct Rev_Watcher, watcher_data);
  return watcher_data->loop;
}
