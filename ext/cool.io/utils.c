/*
 * Copyright (C) 2007 Tony Arcieri
 * You may redistribute this under the terms of the Ruby license.
 * See LICENSE for details
 */

#include "ruby.h"

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#ifdef HAVE_SYS_SYSCTL_H
#include <sys/param.h>
#include <sys/sysctl.h>
#endif

static VALUE mCoolio = Qnil;
static VALUE cCoolio_Utils = Qnil;

static VALUE Coolio_Utils_ncpus(VALUE self);
static VALUE Coolio_Utils_maxfds(VALUE self);
static VALUE Coolio_Utils_setmaxfds(VALUE self, VALUE max);

/*
 * Assorted utility routines
 */
void Init_coolio_utils()
{
  mCoolio = rb_define_module("Coolio");
  cCoolio_Utils = rb_define_module_under(mCoolio, "Utils");

  rb_define_singleton_method(cCoolio_Utils, "ncpus", Coolio_Utils_ncpus, 0);
  rb_define_singleton_method(cCoolio_Utils, "maxfds", Coolio_Utils_maxfds, 0);
  rb_define_singleton_method(cCoolio_Utils, "maxfds=", Coolio_Utils_setmaxfds, 1);
}

/**
 *  call-seq:
 *    Coolio::Utils.ncpus -> Integer
 * 
 * Return the number of CPUs in the present system
 */
static VALUE Coolio_Utils_ncpus(VALUE self)
{
  int ncpus = 0;

#ifdef HAVE_LINUX_PROCFS
#define HAVE_COOLIO_UTILS_NCPUS
  char buf[512];
  FILE *cpuinfo;
  
  if(!(cpuinfo = fopen("/proc/cpuinfo", "r")))
    rb_sys_fail("fopen");

  while(fgets(buf, 512, cpuinfo)) {
    if(!strncmp(buf, "processor", 9))
      ncpus++;
  }
#endif

#ifdef HAVE_SYSCTLBYNAME
#define HAVE_COOLIO_UTILS_NCPUS
  size_t size = sizeof(int);

  if(sysctlbyname("hw.ncpu", &ncpus, &size, NULL, 0)) 
    return INT2NUM(1);
#endif

#ifndef HAVE_COOLIO_UTILS_NCPUS
  rb_raise(rb_eRuntimeError, "operation not supported");
#endif

  return INT2NUM(ncpus);
}

/**
 *  call-seq:
 *    Coolio::Utils.maxfds -> Integer
 * 
 * Return the maximum number of files descriptors available to the process
 */
static VALUE Coolio_Utils_maxfds(VALUE self)
{
#ifdef HAVE_SYS_RESOURCE_H
  struct rlimit rlim;

  if(getrlimit(RLIMIT_NOFILE, &rlim) < 0)
    rb_sys_fail("getrlimit");

  return INT2NUM(rlim.rlim_cur);
#endif

#ifndef HAVE_SYS_RESOURCE_H
  rb_raise(rb_eRuntimeError, "operation not supported");
#endif
}

/**
 *  call-seq:
 *    Coolio::Utils.maxfds=(count) -> Integer
 * 
 * Set the number of file descriptors available to the process.  May require
 * superuser privileges.
 */
static VALUE Coolio_Utils_setmaxfds(VALUE self, VALUE max)
{
#ifdef HAVE_SYS_RESOURCE_H
  struct rlimit rlim;

  rlim.rlim_cur = NUM2INT(max);

  if(setrlimit(RLIMIT_NOFILE, &rlim) < 0)
    rb_sys_fail("setrlimit");

  return max;
#endif

#ifndef HAVE_SYS_RESOURCE_H
  rb_raise(rb_eRuntimeError, "operation not supported");
#endif
}
