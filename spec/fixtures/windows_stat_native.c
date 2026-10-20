/* Compile a private libev copy to exercise path handling without adding test
 * hooks to the cool.io extension. */
#include <ruby.h>
#include <ruby/encoding.h>

#define EV_STANDALONE 1
#define EV_USE_REALTIME 0
#define EV_USE_MONOTONIC 0
#define EV_WIN32_STAT_CODEPAGE CP_UTF8
#include "ev.c"

static VALUE
stat_path (VALUE self, VALUE path, VALUE parent)
{
  wchar_t *wide = win32_fs_path (StringValueCStr (path), RTEST (parent));
  VALUE result;
  if (!wide)
    return Qnil;
  result = rb_enc_str_new ((const char *)wide, wcslen (wide) * sizeof (wchar_t),
                          rb_enc_find ("UTF-16LE"));
  free (wide);
  return rb_str_export_to_enc (result, rb_utf8_encoding ());
}

RUBY_FUNC_EXPORTED void
Init_coolio_stat_native (void)
{
  VALUE module = rb_define_module ("CoolioStatNativeSpec");
  rb_define_singleton_method (module, "path", stat_path, 2);
}
