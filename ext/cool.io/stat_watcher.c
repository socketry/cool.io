/*
 * Copyright (C) 2009-10 Tony Arcieri
 * You may redistribute this under the terms of the Ruby license.
 * See LICENSE for details
 */

#include "ruby.h"
#include "ev_wrap.h"

#include "cool.io.h"
#include "watcher.h"

static VALUE mCoolio = Qnil;
static VALUE cCoolio_Watcher = Qnil;
static VALUE cCoolio_StatWatcher = Qnil;
static VALUE cCoolio_StatInfo = Qnil;
static VALUE cCoolio_Loop = Qnil;

static VALUE Coolio_StatWatcher_initialize(int argc, VALUE *argv, VALUE self);
static VALUE Coolio_StatWatcher_attach(VALUE self, VALUE loop);
static VALUE Coolio_StatWatcher_detach(VALUE self);
static VALUE Coolio_StatWatcher_enable(VALUE self);
static VALUE Coolio_StatWatcher_disable(VALUE self);
static VALUE Coolio_StatWatcher_on_change(VALUE self, VALUE previous, VALUE current);
static VALUE Coolio_StatWatcher_path(VALUE self);

static VALUE Coolio_StatInfo_build(ev_statdata *statdata_struct);

static void Coolio_StatWatcher_libev_callback(struct ev_loop *ev_loop, struct ev_stat *stat, int revents);
static void Coolio_StatWatcher_dispatch_callback(VALUE self, int revents);

/*
 * Coolio::StatWatcher lets you create either one-shot or periodic stats which
 * run within Coolio's event loop.  It's useful for creating timeouts or
 * events which fire periodically.
 **/
void Init_coolio_stat_watcher()
{
  mCoolio = rb_define_module("Coolio");
  cCoolio_Watcher = rb_define_class_under(mCoolio, "Watcher", rb_cObject);
  cCoolio_StatWatcher = rb_define_class_under(mCoolio, "StatWatcher", cCoolio_Watcher);
  cCoolio_StatInfo = rb_struct_define("StatInfo",
      "mtime",
      "ctime",
      "atime",
      "dev",
      "ino",
      "mode",
      "nlink",
      "uid",
      "guid",
      "rdev",
      "size",
      "blksize",
      "blocks",
      NULL);
  cCoolio_Loop = rb_define_class_under(mCoolio, "Loop", rb_cObject);

  rb_define_method(cCoolio_StatWatcher, "initialize", Coolio_StatWatcher_initialize, -1);
  rb_define_method(cCoolio_StatWatcher, "attach", Coolio_StatWatcher_attach, 1);
  rb_define_method(cCoolio_StatWatcher, "detach", Coolio_StatWatcher_detach, 0);
  rb_define_method(cCoolio_StatWatcher, "enable", Coolio_StatWatcher_enable, 0);
  rb_define_method(cCoolio_StatWatcher, "disable", Coolio_StatWatcher_disable, 0);
  rb_define_method(cCoolio_StatWatcher, "on_change", Coolio_StatWatcher_on_change, 2);
  rb_define_method(cCoolio_StatWatcher, "path", Coolio_StatWatcher_path, 0);
}

/**
 *  call-seq:
 *    Coolio::StatWatcher.initialize(path, interval = 0) -> Coolio::StatWatcher
 *
 * Create a new Coolio::StatWatcher for the given path.  This will monitor the
 * given path for changes at the filesystem level.  The interval argument
 * specified how often in seconds the path should be polled for changes.
 * Setting interval to zero uses an "automatic" value (typically around 5
 * seconds) which optimizes performance.  Otherwise, values less than
 * 0.1 are not particularly meaningful.  Where available (at present, on Linux)
 * high performance file monitoring interfaces will be used instead of polling.
 */
static VALUE Coolio_StatWatcher_initialize(int argc, VALUE *argv, VALUE self)
{
	VALUE path, interval;
  struct Coolio_Watcher *watcher_data;

  rb_scan_args(argc, argv, "11", &path, &interval);
  if(interval != Qnil)
    interval = rb_convert_type(interval, T_FLOAT, "Float", "to_f");

  path = rb_String(path);
  rb_iv_set(self, "@path", path);

  Data_Get_Struct(self, struct Coolio_Watcher, watcher_data);

  watcher_data->dispatch_callback = Coolio_StatWatcher_dispatch_callback;
  ev_stat_init(
      &watcher_data->event_types.ev_stat,
      Coolio_StatWatcher_libev_callback,
      RSTRING_PTR(path),
      interval == Qnil ? 0 : NUM2DBL(interval)
  );
  watcher_data->event_types.ev_stat.data = (void *)self;

  return Qnil;
}

/**
 *  call-seq:
 *    Coolio::StatWatcher.attach(loop) -> Coolio::StatWatcher
 *
 * Attach the stat watcher to the given Coolio::Loop.  If the watcher is already
 * attached to a loop, detach it from the old one and attach it to the new one.
 */
static VALUE Coolio_StatWatcher_attach(VALUE self, VALUE loop)
{
  ev_tstamp interval, timeout;
  struct Coolio_Loop *loop_data;
  struct Coolio_Watcher *watcher_data;

  if(!rb_obj_is_kind_of(loop, cCoolio_Loop))
    rb_raise(rb_eArgError, "expected loop to be an instance of Coolio::Loop, not %s", RSTRING_PTR(rb_inspect(loop)));

  Data_Get_Struct(loop, struct Coolio_Loop, loop_data);
  Data_Get_Struct(self, struct Coolio_Watcher, watcher_data);

  if(watcher_data->loop != Qnil)
    Coolio_StatWatcher_detach(self);

  watcher_data->loop = loop;

  ev_stat_start(loop_data->ev_loop, &watcher_data->event_types.ev_stat);
	rb_call_super(1, &loop);

  return self;
}

/**
 *  call-seq:
 *    Coolio::StatWatcher.detach -> Coolio::StatWatcher
 *
 * Detach the stat watcher from its current Coolio::Loop.
 */
static VALUE Coolio_StatWatcher_detach(VALUE self)
{
  Watcher_Detach(stat, self);

  return self;
}

/**
 *  call-seq:
 *    Coolio::StatWatcher.enable -> Coolio::StatWatcher
 *
 * Re-enable a stat watcher which has been temporarily disabled.  See the
 * disable method for a more thorough explanation.
 */
static VALUE Coolio_StatWatcher_enable(VALUE self)
{
  Watcher_Enable(stat, self);

  return self;
}

/**
 *  call-seq:
 *    Coolio::StatWatcher.disable -> Coolio::StatWatcher
 *
 * Temporarily disable a stat watcher which is attached to a loop.
 * This is useful if you wish to toggle event monitoring on and off.
 */
static VALUE Coolio_StatWatcher_disable(VALUE self)
{
  Watcher_Disable(stat, self);

  return self;
}

/**
 *  call-seq:
 *    Coolio::StatWatcher#on_change -> nil
 *
 * Called whenever the status of the given path changes
 */
static VALUE Coolio_StatWatcher_on_change(VALUE self, VALUE previous, VALUE current)
{
  return Qnil;
}

/**
 *  call-seq:
 *    Coolio::StatWatcher#path -> String
 *
 * Retrieve the path associated with this StatWatcher
 */
static VALUE Coolio_StatWatcher_path(VALUE self)
{
  return rb_iv_get(self, "@path");
}

/* libev callback */
static void Coolio_StatWatcher_libev_callback(struct ev_loop *ev_loop, struct ev_stat *stat, int revents)
{
  Coolio_Loop_process_event((VALUE)stat->data, revents);
}

/* Coolio::Loop dispatch callback */
static void Coolio_StatWatcher_dispatch_callback(VALUE self, int revents)
{
  struct Coolio_Watcher *watcher_data;
  Data_Get_Struct(self, struct Coolio_Watcher, watcher_data);

  VALUE previous_statdata = Coolio_StatInfo_build(&watcher_data->event_types.ev_stat.prev);
  VALUE current_statdata = Coolio_StatInfo_build(&watcher_data->event_types.ev_stat.attr);
  rb_funcall(self, rb_intern("on_change"), 2, previous_statdata, current_statdata);
}

/**
 * Convience method to build StatInfo structs given an ev_statdata
 * */
static VALUE Coolio_StatInfo_build(ev_statdata *statdata_struct)
{
  VALUE at_method = rb_intern("at");
  VALUE cTime     = rb_const_get(rb_cObject, rb_intern("Time"));

  VALUE mtime     = Qnil;
  VALUE ctime     = Qnil;
  VALUE atime     = Qnil;
  VALUE dev       = Qnil;
  VALUE ino       = Qnil;
  VALUE mode      = Qnil;
  VALUE nlink     = Qnil;
  VALUE uid       = Qnil;
  VALUE gid       = Qnil;
  VALUE rdev      = Qnil;
  VALUE size      = Qnil;
  VALUE blksize   = Qnil;
  VALUE blocks    = Qnil;

  mtime   = rb_funcall(cTime, at_method, 1, INT2NUM(statdata_struct->st_mtime));
  ctime   = rb_funcall(cTime, at_method, 1, INT2NUM(statdata_struct->st_ctime));
  atime   = rb_funcall(cTime, at_method, 1, INT2NUM(statdata_struct->st_atime));
  dev     = INT2NUM(statdata_struct->st_dev);
  ino     = INT2NUM(statdata_struct->st_ino);
  mode    = INT2NUM(statdata_struct->st_mode);
  nlink   = INT2NUM(statdata_struct->st_nlink);
  uid     = INT2NUM(statdata_struct->st_uid);
  gid     = INT2NUM(statdata_struct->st_gid);
  rdev    = INT2NUM(statdata_struct->st_rdev);
  size    = INT2NUM(statdata_struct->st_size);
#ifdef HAVE_ST_BLKSIZE
  blksize = INT2NUM(statdata_struct->st_blksize);
  blocks  = INT2NUM(statdata_struct->st_blocks);
#endif

  return rb_struct_new(cCoolio_StatInfo,
      mtime,
      ctime,
      atime,
      dev,
      ino,
      mode,
      nlink,
      uid,
      gid,
      rdev,
      size,
      blksize,
      blocks,
      NULL);
}
