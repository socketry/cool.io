/*
 * Copyright (C) 2008 Tony Arcieri
 * Includes portions from the 'OpenSSL for Ruby' project
 * Copyright (C) 2000-2002  GOTOU Yuuzou <gotoyuzo@notwork.org>
 * Copyright (C) 2001-2002  Michal Rokos <m.rokos@sh.cvut.cz>
 * Copyright (C) 2001-2007  Technorama Ltd. <oss-ruby@technorama.net>
 * You may redistribute this under the terms of the Ruby license.
 * See LICENSE for details
 */

#ifdef HAVE_OPENSSL_SSL_H

#include "ruby.h"
#include "rubyio.h"

#include <openssl/ssl.h>

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

static VALUE Rev_SSL_IO_connect_nonblock(VALUE self);
static VALUE Rev_SSL_IO_accept_nonblock(VALUE self);
static VALUE Rev_SSL_IO_ssl_setup(VALUE self);
static VALUE Rev_SSL_IO_ssl_setup_check(VALUE dummy, VALUE error_info);
static VALUE Rev_SSL_IO_start_ssl(VALUE self, int (*func)(), const char *funcname);

static VALUE Rev_SSL_IO_read_nonblock(int argc, VALUE *argv, VALUE self);
static VALUE Rev_SSL_IO_write_nonblock(VALUE self, VALUE str);

/*
 * Time to monkey patch some C code!
 */

/* Ruby 1.8 leaves us no recourse but to commonly couple to the OpenSSL native
   extension through externs.  Ugh */
#if RUBY_VERSION_CODE < 190
 
/* Externs from Ruby's OpenSSL native extension , in ossl_ssl.c*/
extern int ossl_ssl_ex_vcb_idx;
extern int ossl_ssl_ex_store_p;
extern int ossl_ssl_ex_ptr_idx;
extern int ossl_ssl_ex_client_cert_cb_idx;
extern int ossl_ssl_ex_tmp_dh_callback_idx;

/* #defines shamelessly copied and pasted from ossl_ssl.c */
#define ossl_ssl_get_io(o)                rb_iv_get((o),"@io")
#define ossl_ssl_get_ctx(o)               rb_iv_get((o),"@context")
#define ossl_sslctx_get_verify_cb(o)      rb_iv_get((o),"@verify_callback")
#define ossl_sslctx_get_client_cert_cb(o) rb_iv_get((o),"@client_cert_cb")
#define ossl_sslctx_get_tmp_dh_cb(o)      rb_iv_get((o),"@tmp_dh_callback")

#ifdef _WIN32
#  define TO_SOCKET(s) _get_osfhandle(s)
#else
#  define TO_SOCKET(s) s
#endif

#endif

#ifndef HAVE_RB_STR_SET_LEN
static void rb_str_set_len(VALUE str, long len)
{
  RSTRING(str)->len = len;
  RSTRING(str)->ptr[len] = '\0';
}
#endif

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
  
  rb_define_method(cRev_SSL_IO, "read_nonblock", Rev_SSL_IO_read_nonblock, -1);
  rb_define_method(cRev_SSL_IO, "write_nonblock", Rev_SSL_IO_write_nonblock, 1);
}

#if RUBY_VERSION_CODE < 190 
/* SSL initialization for Ruby 1.8 */
static VALUE
Rev_SSL_IO_ssl_setup(VALUE self)
{
  VALUE io, v_ctx, cb;
  SSL_CTX *ctx;
  SSL *ssl;
  OpenFile *fptr;

  Data_Get_Struct(self, SSL, ssl);
  if(!ssl) {
    v_ctx = ossl_ssl_get_ctx(self);
    Data_Get_Struct(v_ctx, SSL_CTX, ctx);

    ssl = SSL_new(ctx);
    if (!ssl) {
      ossl_raise(eSSLError, "SSL_new:");
    }
    DATA_PTR(self) = ssl;

    io = ossl_ssl_get_io(self);
    GetOpenFile(io, fptr);
    rb_io_check_readable(fptr);
    rb_io_check_writable(fptr);
    SSL_set_fd(ssl, TO_SOCKET(fileno(fptr->f)));
    SSL_set_ex_data(ssl, ossl_ssl_ex_ptr_idx, (void*)self);
    cb = ossl_sslctx_get_verify_cb(v_ctx);
    SSL_set_ex_data(ssl, ossl_ssl_ex_vcb_idx, (void*)cb);
    cb = ossl_sslctx_get_client_cert_cb(v_ctx);
    SSL_set_ex_data(ssl, ossl_ssl_ex_client_cert_cb_idx, (void*)cb);
    cb = ossl_sslctx_get_tmp_dh_cb(v_ctx);
    SSL_set_ex_data(ssl, ossl_ssl_ex_tmp_dh_callback_idx, (void*)cb);
  }

  return Qtrue;
}  
#endif

#if RUBY_VERSION_CODE >= 190
/* Slightly less insane SSL setup for Ruby 1.9 */
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
#endif

/* Ensure the error raised by calling #session= with a dummy argument is 
 * the one we were expecting */
static VALUE 
Rev_SSL_IO_ssl_setup_check(VALUE dummy, VALUE err)
{
  return Qnil;
}

/*
 * call-seq:
 *    ssl.connect => self
 */
static VALUE
Rev_SSL_IO_connect_nonblock(VALUE self)
{
#if RUBY_VERSION_CODE >= 190
  rb_rescue(Rev_SSL_IO_ssl_setup, self, Rev_SSL_IO_ssl_setup_check, Qnil);
#else
  Rev_SSL_IO_ssl_setup(self);
#endif
  
  return Rev_SSL_IO_start_ssl(self, SSL_connect, "SSL_connect");
}

/*
 * call-seq:
 *    ssl.accept => self
 */
static VALUE
Rev_SSL_IO_accept_nonblock(VALUE self)
{
#if RUBY_VERSION_CODE >= 190
  rb_rescue(Rev_SSL_IO_ssl_setup, self, 0, 0);
#else
  Rev_SSL_IO_ssl_setup(self);
#endif

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

/*
 * call-seq:
 *    ssl.read_nonblock(length) => string
 *    ssl.read_nonblock(length, buffer) => buffer
 *
 * === Parameters
 * * +length+ is a positive integer.
 * * +buffer+ is a string used to store the result.
 */
static VALUE
Rev_SSL_IO_read_nonblock(int argc, VALUE *argv, VALUE self)
{
  SSL *ssl;
  int ilen, nread = 0;
  VALUE len, str;

  rb_scan_args(argc, argv, "11", &len, &str);
  ilen = NUM2INT(len);
  
  if(NIL_P(str)) 
    str = rb_str_new(0, ilen);
  else {
    StringValue(str);
    rb_str_modify(str);
    rb_str_resize(str, ilen);
  }
  
  if(ilen == 0) return str;

  Data_Get_Struct(self, SSL, ssl);

  if (ssl) {
    nread = SSL_read(ssl, RSTRING_PTR(str), RSTRING_LEN(str));
    switch(SSL_get_error(ssl, nread)){
    case SSL_ERROR_NONE:
      goto end;
    case SSL_ERROR_ZERO_RETURN:
      rb_eof_error();
    case SSL_ERROR_WANT_WRITE:
      rb_raise(eRev_SSL_IO_WriteAgain, "write again");
    case SSL_ERROR_WANT_READ:
      rb_raise(eRev_SSL_IO_ReadAgain, "read again");
    case SSL_ERROR_SYSCALL:
      if(ERR_peek_error() == 0 && nread == 0) rb_eof_error();
      rb_sys_fail(0);
    default:
      rb_raise(eSSLError, "SSL_read:");
    }
  } else
    rb_raise(rb_eRuntimeError, "SSL session is not started yet.");

end:
  rb_str_set_len(str, nread);
  OBJ_TAINT(str);

  return str;
}

/*
 * call-seq:
 *    ssl.write_nonblock(string) => integer
 */
static VALUE
Rev_SSL_IO_write_nonblock(VALUE self, VALUE str)
{
  SSL *ssl;
  int nwrite = 0;

  StringValue(str);
  Data_Get_Struct(self, SSL, ssl);

  if (ssl) {
    nwrite = SSL_write(ssl, RSTRING_PTR(str), RSTRING_LEN(str));
    switch(SSL_get_error(ssl, nwrite)){
    case SSL_ERROR_NONE:
      goto end;
    case SSL_ERROR_WANT_WRITE:
      rb_raise(eRev_SSL_IO_WriteAgain, "write again");
    case SSL_ERROR_WANT_READ:
      rb_raise(eRev_SSL_IO_ReadAgain, "read again");
    case SSL_ERROR_SYSCALL:
      if (errno) rb_sys_fail(0);
    default:
      rb_raise(eSSLError, "SSL_write:");
    }
  } else
    rb_raise(rb_eRuntimeError, "SSL session is not started yet.");

end:
  return INT2NUM(nwrite);
}

#endif
