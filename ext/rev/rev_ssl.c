/*
 * Copyright (C) 2008 Tony Arcieri
 * Includes portions from the 'OpenSSL for Ruby' project
 * Copyright (C) 2000-2002  GOTOU Yuuzou <gotoyuzo@notwork.org>
 * Copyright (C) 2001-2002  Michal Rokos <m.rokos@sh.cvut.cz>
 * Copyright (C) 2001-2007  Technorama Ltd. <oss-ruby@technorama.net>
 * You may redistribute this under the terms of the Ruby license.
 * See LICENSE for details
 */

#include "ruby.h"
#include "rubyio.h"

#include <openssl/ssl.h>

/* Module and object handles */
static VALUE mOSSL = Qnil;
static VALUE eOSSLError = Qnil;

static VALUE mSSL = Qnil;
static VALUE cSSLSocket = Qnil;
static VALUE eSSLError = Qnil;

static VALUE mRev = Qnil;
static VALUE mRev_SSL = Qnil;
static VALUE cRev_SSL_IO = Qnil;

static VALUE eRev_SSL_IO_ReadAgain = Qnil;
static VALUE eRev_SSL_IO_WriteAgain = Qnil;

/* Method implementations */
static VALUE Rev_SSL_IO_connect_nonblock(VALUE self);
static VALUE Rev_SSL_IO_accept_nonblock(VALUE self);
static VALUE Rev_SSL_IO_ssl_setup(VALUE self);
static VALUE Rev_SSL_IO_start_ssl(VALUE self, int (*func)(), const char *funcname);

void Init_rev_ssl()
{
  rb_require("openssl");
  
  mOSSL = rb_define_module("OpenSSL");
  eOSSLError = rb_define_class_under(mOSSL, "OpenSSLError", rb_eStandardError);
  
  mSSL = rb_define_module_under(mOSSL, "SSL");
  cSSLSocket = rb_define_class_under(mSSL, "SSLSocket", rb_cObject);
  eSSLError = rb_define_class_under(mSSL, "SSLError", eOSSLError);

  mRev = rb_define_module("Rev");
  mRev_SSL = rb_define_module_under(mRev, "SSL");
  cRev_SSL_IO = rb_define_class_under(mRev_SSL, "IO", cSSLSocket);
  
  eRev_SSL_IO_ReadAgain = rb_define_class_under(cRev_SSL_IO, "ReadAgain", rb_eStandardError);
  eRev_SSL_IO_WriteAgain = rb_define_class_under(cRev_SSL_IO, "WriteAgain", rb_eStandardError);
  
  rb_define_method(cRev_SSL_IO, "connect_nonblock", Rev_SSL_IO_connect_nonblock, 0);
  rb_define_method(cRev_SSL_IO, "accept_nonblock", Rev_SSL_IO_accept_nonblock, 0);
}

static VALUE
Rev_SSL_IO_ssl_setup(VALUE self)
{
  /*
   * DANGER WILL ROBINSON!  CRAZY HACKS AHEAD!
   *
   * Before we connect or accept we need to call the ossl_ssl_setup() function
   * in ossl_ssl.c.  For whatever reason this isn't called in 
   * SSLSocket#initialize but is instead called directly from #connect and
   * #accept.
   *
   * To make things even more awesome, it's a static function, so we can't
   * call it directly.  However, we can call it indirectly...
   *
   * There's one other method within ossl_ssl.c which calls ossl_ssl_setup(),
   * and that's #session=.  I'm not sure why this calls it, but its author
   * left this comment to help us figure out:
   *
   * "why is ossl_ssl_setup delayed?"
   *
   * Why indeed, guy... why indeed.  Well, his function calls ossl_ssl_setup(), 
   * then typechecks its arguments, which means if we pass a bogus one it will
   * happily setup SSL for us, then raise an exception.  So we can catch
   * that exception and be on our merry way.
   *
   * I don't even know what this method is supposed to do.  It appears related
   * to OpenSSL::SSL::Session, which is linked into the OpenSSL library but
   * never initialized, probably because it's buggy.  Nevertheless, the 
   * #session= method is still available to use for this hack.  Awesome!
   */
  rb_funcall(self, rb_intern("session="), 1, Qnil);
}

/*
 * call-seq:
 *    ssl.connect => self
 */
static VALUE
Rev_SSL_IO_connect_nonblock(VALUE self)
{
  rb_rescue(Rev_SSL_IO_ssl_setup, self, 0, 0);
  return Rev_SSL_IO_start_ssl(self, SSL_connect, "SSL_connect");
}

/*
 * call-seq:
 *    ssl.accept => self
 */
static VALUE
Rev_SSL_IO_accept_nonblock(VALUE self)
{
  rb_rescue(Rev_SSL_IO_ssl_setup, self, 0, 0);
  return Rev_SSL_IO_start_ssl(self, SSL_accept, "SSL_accept");
}

static VALUE
Rev_SSL_IO_start_ssl(VALUE self, int (*func)(), const char *funcname)
{
  SSL *ssl;
  int ret, ret2;

  Data_Get_Struct(self, SSL, ssl);
  if(!ssl)
    rb_raise(rb_eRuntimeError, "SSL never initialized");

  if((ret = func(ssl)) <= 0) {
    switch((ret2 = SSL_get_error(ssl, ret))) {
    case SSL_ERROR_WANT_WRITE:
      rb_raise(eRev_SSL_IO_WriteAgain, "write again");
    case SSL_ERROR_WANT_READ:
      rb_raise(eRev_SSL_IO_ReadAgain, "read again");
    case SSL_ERROR_SYSCALL:
      if (errno) rb_sys_fail(funcname);
      rb_raise(eSSLError, "%s SYSCALL returned=%d errno=%d state=%s", 
        funcname, ret2, errno, SSL_state_string_long(ssl)
      );
    default:
      rb_raise(eSSLError, "%s returned=%d errno=%d state=%s", 
        funcname, ret2, errno, SSL_state_string_long(ssl)
      );
    }
  }

  return self;
}
