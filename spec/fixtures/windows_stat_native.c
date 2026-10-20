/* Compile a private libev copy to count Windows API calls and inject failures
 * without adding test hooks to the cool.io extension. */
#include <ruby.h>
#include <ruby/encoding.h>
#include <winsock2.h>
#include <windows.h>
#include <errno.h>

static int fail_socket, socket_calls, syserr_calls, changed, timed_out;
static HANDLE completion_entered, completion_release;
static volatile LONG delay_completion;
static int directory_opens, identity_calls, short_calls, long_calls, fail_alias;

static HANDLE WINAPI
test_create_file (LPCWSTR path, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security,
                  DWORD disposition, DWORD attributes, HANDLE template)
{
  ++directory_opens;
  return CreateFileW (path, access, share, security, disposition, attributes, template);
}

static BOOL WINAPI
test_file_information (HANDLE handle, LPBY_HANDLE_FILE_INFORMATION information)
{
  ++identity_calls;
  return GetFileInformationByHandle (handle, information);
}

static DWORD WINAPI
test_short_path (LPCWSTR path, LPWSTR buffer, DWORD size)
{
  ++short_calls;
  if (fail_alias)
    { SetLastError (ERROR_ACCESS_DENIED); return 0; }
  return GetShortPathNameW (path, buffer, size);
}

static DWORD WINAPI
test_long_path (LPCWSTR path, LPWSTR buffer, DWORD size)
{
  ++long_calls;
  if (fail_alias)
    { SetLastError (ERROR_ACCESS_DENIED); return 0; }
  return GetLongPathNameW (path, buffer, size);
}

static BOOL WINAPI
test_completion (HANDLE port, DWORD *bytes, ULONG_PTR *key,
                 OVERLAPPED **overlapped, DWORD timeout)
{
  BOOL result = GetQueuedCompletionStatus (port, bytes, key, overlapped, timeout);
  DWORD error = GetLastError ();
  if (*overlapped && InterlockedExchange (&delay_completion, 0))
    {
      SetEvent (completion_entered);
      WaitForSingleObject (completion_release, 5000);
    }
  SetLastError (error);
  return result;
}

static SOCKET
test_socket (int af, int type, int protocol)
{
  ++socket_calls;
  if (fail_socket)
    {
      errno = EACCES;
      WSASetLastError (WSAEACCES);
      return INVALID_SOCKET;
    }
  return socket (af, type, protocol);
}

#define socket test_socket
#define GetQueuedCompletionStatus test_completion
#define CreateFileW test_create_file
#define GetFileInformationByHandle test_file_information
#define GetShortPathNameW test_short_path
#define GetLongPathNameW test_long_path
#define EV_STANDALONE 1
#define EV_USE_REALTIME 0
#define EV_USE_MONOTONIC 0
#define EV_WIN32_STAT_CODEPAGE CP_UTF8
#include "ev.c"
#undef socket
#undef GetQueuedCompletionStatus
#undef CreateFileW
#undef GetFileInformationByHandle
#undef GetShortPathNameW
#undef GetLongPathNameW

static void
recover_socket (const char *message)
{
  ++syserr_calls;
  fail_socket = 0;
}

static void
changed_cb (EV_P_ ev_stat *watcher, int events)
{
  changed = watcher->attr.st_size == 5;
  ev_break (EV_A_ EVBREAK_ALL);
}

static void
timeout_cb (EV_P_ ev_timer *watcher, int events)
{
  timed_out = 1;
  ev_break (EV_A_ EVBREAK_ALL);
}

static void async_cb (EV_P_ ev_async *watcher, int events) {}

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

static VALUE
check_pipe (VALUE self, VALUE path)
{
  struct ev_loop *loop;
  ev_stat watcher;
  ev_timer timeout;
  ev_async async;
  WSADATA data;
  FILE *file;
  const char *error = 0;
  const char *filename = StringValueCStr (path);
  if (WSAStartup (MAKEWORD (2, 2), &data))
    rb_raise (rb_eRuntimeError, "WSAStartup failed");
  file = fopen (filename, "w");
  if (!file)
    { WSACleanup (); rb_sys_fail (filename); }
  fputs ("a", file);
  fclose (file);
  loop = ev_loop_new (EVFLAG_NOENV | EVBACKEND_SELECT);
  if (!loop)
    { WSACleanup (); rb_raise (rb_eRuntimeError, "ev_loop_new failed"); }
  ev_set_syserr_cb (recover_socket);
  fail_socket = 1;
  ev_stat_init (&watcher, changed_cb, filename, 0.1);
  ev_stat_start (loop, &watcher);
  if (loop->win32_fs || ev_is_active (&loop->pipe_w) || syserr_calls
      || !ev_is_active (&watcher.timer))
    { error = "pipe failure did not fall back to polling"; goto cleanup; }

  /* With the default syserr handler, a failed retry must not abort either. */
  ev_set_syserr_cb (0);
  file = fopen (filename, "w");
  if (!file)
    { error = "could not update watched file"; goto cleanup; }
  fputs ("final", file);
  fclose (file);
  ev_timer_init (&timeout, timeout_cb, 5, 0);
  ev_timer_start (loop, &timeout);
  ev_run (loop, 0);
  ev_timer_stop (loop, &timeout);
  if (!changed || timed_out || loop->win32_fs)
    { error = "polling did not deliver the change"; goto cleanup; }

  fail_socket = 0;
  socket_calls = 0;
  ev_run (loop, EVRUN_ONCE); /* The next polling tick retries notification setup. */
  /* One listener plus one client socket; ev_async_start must reuse this pair. */
  if (!loop->win32_fs || !ev_is_active (&loop->pipe_w) || socket_calls != 2)
    error = "notification recovery did not reuse the prepared pipe";
cleanup:
  ev_stat_stop (loop, &watcher);
  ev_loop_destroy (loop);
  if (error)
    { WSACleanup (); rb_raise (rb_eRuntimeError, "%s", error); }

  loop = ev_loop_new (EVFLAG_NOENV | EVBACKEND_SELECT);
  if (!loop)
    { WSACleanup (); rb_raise (rb_eRuntimeError, "ev_loop_new failed"); }
  fail_socket = 1;
  socket_calls = syserr_calls = 0;
  ev_set_syserr_cb (recover_socket);
  ev_async_init (&async, async_cb);
  ev_async_start (loop, &async);
  if (syserr_calls != 1 || socket_calls != 3 || !ev_is_active (&loop->pipe_w))
    error = "existing async retry semantics changed";
  ev_async_stop (loop, &async);
  ev_loop_destroy (loop);
  WSACleanup ();
  if (error)
    rb_raise (rb_eRuntimeError, "%s", error);
  return Qtrue;
}

static VALUE
check_shutdown (VALUE self, VALUE path)
{
  struct ev_loop *loop;
  ev_stat watcher;
  WSADATA data;
  FILE *file;
  HANDLE thread = 0;
  DWORD started, elapsed, finished;
  const char *filename = StringValueCStr (path);
  const char *error = 0;
  if (WSAStartup (MAKEWORD (2, 2), &data))
    rb_raise (rb_eRuntimeError, "WSAStartup failed");
  completion_entered = CreateEvent (0, TRUE, FALSE, 0);
  completion_release = CreateEvent (0, TRUE, FALSE, 0);
  loop = ev_loop_new (EVFLAG_NOENV | EVBACKEND_SELECT);
  if (!loop || !completion_entered || !completion_release)
    rb_raise (rb_eRuntimeError, "shutdown test setup failed");
  file = fopen (filename, "w");
  if (!file)
    rb_sys_fail (filename);
  fputs ("a", file);
  fclose (file);
  ev_stat_init (&watcher, changed_cb, filename, 60);
  ev_stat_start (loop, &watcher);
  if (!loop->win32_fs || !DuplicateHandle (GetCurrentProcess (), loop->win32_fs->thread,
      GetCurrentProcess (), &thread, 0, FALSE, DUPLICATE_SAME_ACCESS))
    { error = "notification worker did not start"; goto cleanup; }
  InterlockedExchange (&delay_completion, 1);
  file = fopen (filename, "w");
  if (!file)
    { error = "could not update shutdown test file"; goto cleanup; }
  fputs ("final", file);
  fclose (file);
  if (WaitForSingleObject (completion_entered, 5000) != WAIT_OBJECT_0)
    { error = "worker did not receive a completion"; goto cleanup; }
cleanup:
  ev_stat_stop (loop, &watcher);
  started = GetTickCount ();
  ev_loop_destroy (loop);
  elapsed = GetTickCount () - started;
  SetEvent (completion_release);
  finished = thread ? WaitForSingleObject (thread, 5000) : WAIT_OBJECT_0;
  if (thread)
    CloseHandle (thread);
  CloseHandle (completion_entered);
  CloseHandle (completion_release);
  WSACleanup ();
  if (error)
    rb_raise (rb_eRuntimeError, "%s", error);
  if (elapsed >= 1000 || finished != WAIT_OBJECT_0)
    rb_raise (rb_eRuntimeError, "shutdown blocked or worker failed to exit");
  return Qtrue;
}

static VALUE
notification_enabled (VALUE self, VALUE path, VALUE flags)
{
  struct ev_loop *loop;
  ev_stat watcher;
  WSADATA data;
  FILE *file;
  int enabled;
  const char *filename = StringValueCStr (path);
  if (WSAStartup (MAKEWORD (2, 2), &data))
    rb_raise (rb_eRuntimeError, "WSAStartup failed");
  file = fopen (filename, "w");
  if (!file)
    { WSACleanup (); rb_sys_fail (filename); }
  fputs ("a", file);
  fclose (file);
  loop = ev_loop_new (NUM2UINT (flags) | EVBACKEND_SELECT);
  if (!loop)
    { WSACleanup (); rb_raise (rb_eRuntimeError, "ev_loop_new failed"); }
  ev_stat_init (&watcher, changed_cb, filename, 60);
  ev_stat_start (loop, &watcher);
  enabled = loop->win32_fs != 0;
  ev_stat_stop (loop, &watcher);
  ev_loop_destroy (loop);
  WSACleanup ();
  return enabled ? Qtrue : Qfalse;
}

static VALUE
path_work (VALUE self, VALUE directory, VALUE mode)
{
  enum { count = 32 };
  struct ev_loop *loop;
  struct ev_win32_fs *ctx;
  ev_stat watchers[count];
  char paths[count][1024];
  WSADATA data;
  FILE *file;
  VALUE result;
  int i, valid = 1, notification = NUM2INT (mode) == 1;
  const char *parent = StringValueCStr (directory);
  if (WSAStartup (MAKEWORD (2, 2), &data))
    rb_raise (rb_eRuntimeError, "WSAStartup failed");
  /* Create everything before registration so setup cannot invalidate aliases. */
  for (i = 0; i < count; ++i)
    {
      snprintf (paths[i], sizeof (paths[i]), "%s/work%d.txt", parent, i);
      file = fopen (paths[i], "w");
      if (!file)
        rb_sys_fail (paths[i]);
      fputs ("a", file);
      fclose (file);
    }
  loop = ev_loop_new (EVFLAG_NOENV | EVBACKEND_SELECT);
  if (!loop)
    rb_raise (rb_eRuntimeError, "ev_loop_new failed");
  for (i = 0; i < count; ++i)
    {
      ev_stat_init (&watchers[i], changed_cb, paths[i], 60);
      ev_stat_start (loop, &watchers[i]);
    }
  ctx = loop->win32_fs;
  if (!ctx)
    rb_raise (rb_eRuntimeError, "notification worker did not start");
  for (i = 0; i < count; ++i)
    {
      file = fopen (paths[i], "a");
      if (!file)
        rb_sys_fail (paths[i]);
      fputs ("x", file);
      fclose (file);
    }
  if (NUM2INT (mode) == 2)
    {
      struct ev_win32_dir event_dir;
      FILE_NOTIFY_INFORMATION *event = (FILE_NOTIFY_INFORMATION *)event_dir.buffer;
      const wchar_t name[] = L"unrelated.tmp";
      memset (&event_dir, 0, sizeof (event_dir));
      event_dir.subscribers = ctx->dirs->subscribers;
      event->Action = FILE_ACTION_ADDED;
      event->FileNameLength = sizeof (name) - sizeof (wchar_t);
      memcpy (event->FileName, name, event->FileNameLength);
      EnterCriticalSection (&ctx->lock);
      win32_dir_notify (ctx, &event_dir,
        offsetof (FILE_NOTIFY_INFORMATION, FileName) + event->FileNameLength);
      LeaveCriticalSection (&ctx->lock);
    }
  if (notification)
    {
      /* Queue the whole changed set without depending on IOCP batch timing. */
      EnterCriticalSection (&ctx->lock);
      for (i = 0; i < ctx->watchcnt; ++i)
        win32_watch_dirty (ctx, ctx->watches[i]);
      ctx->dirs->dirty = 1;
      LeaveCriticalSection (&ctx->lock);
    }
  directory_opens = identity_calls = short_calls = long_calls = 0;
  if (notification)
    win32_fs_cb (loop, &ctx->async, EV_ASYNC);
  else
    for (i = 0; i < count; ++i)
      stat_timer_cb (loop, &watchers[i].timer, EV_TIMER);
  for (i = 0; i < count; ++i)
    valid = valid && watchers[i].attr.st_size == 2;
  result = rb_ary_new_from_args (5, INT2NUM (directory_opens), INT2NUM (identity_calls),
    INT2NUM (short_calls), INT2NUM (long_calls), valid ? Qtrue : Qfalse);
  for (i = 0; i < count; ++i)
    ev_stat_stop (loop, &watchers[i]);
  ev_loop_destroy (loop);
  WSACleanup ();
  return result;
}

static int
watch_index_consistent (struct ev_win32_fs *ctx, ev_stat *watchers, int count)
{
  int i, active = 0;
  for (i = 0; i < count; ++i)
    {
      struct ev_win32_watch *watch = win32_fs_watch (ctx, &watchers[i]);
      if (ev_is_active (&watchers[i]))
        {
          if (!watch || watch->parent.dir->failed)
            {
              return 0;
            }
          ++active;
        }
      else if (watch)
        {
          return 0;
        }
    }
  return ctx->watchcnt == active;
}

static void
poll_active (struct ev_loop *loop, ev_stat *watchers, int count)
{
  int i;
  for (i = 0; i < count; ++i)
    {
      if (ev_is_active (&watchers[i]))
        {
          stat_timer_cb (loop, &watchers[i].timer, EV_TIMER);
        }
    }
}

static VALUE
watch_index (VALUE self, VALUE directory)
{
  enum { count = 4 };
  struct ev_loop *loop;
  struct ev_win32_fs *ctx;
  ev_stat watchers[count];
  char paths[count][1024];
  WSADATA data;
  FILE *file;
  VALUE result;
  int i, stopped, replaced;
  const char *parent = StringValueCStr (directory);
  if (WSAStartup (MAKEWORD (2, 2), &data))
    {
      rb_raise (rb_eRuntimeError, "WSAStartup failed");
    }
  for (i = 0; i < count; ++i)
    {
      snprintf (paths[i], sizeof (paths[i]), "%s/index%d.txt", parent, i);
      file = fopen (paths[i], "w");
      if (!file)
        {
          WSACleanup ();
          rb_sys_fail (paths[i]);
        }
      fputs ("a", file);
      fclose (file);
    }
  loop = ev_loop_new (EVFLAG_NOENV | EVBACKEND_SELECT);
  if (!loop)
    {
      WSACleanup ();
      rb_raise (rb_eRuntimeError, "ev_loop_new failed");
    }
  for (i = 0; i < count; ++i)
    {
      ev_stat_init (&watchers[i], changed_cb, paths[i], 60);
      ev_stat_start (loop, &watchers[i]);
    }
  ctx = loop->win32_fs;
  if (!ctx)
    {
      ev_loop_destroy (loop);
      WSACleanup ();
      rb_raise (rb_eRuntimeError, "notification worker did not start");
    }
  ev_stat_stop (loop, &watchers[1]);
  directory_opens = identity_calls = short_calls = long_calls = 0;
  poll_active (loop, watchers, count);
  result = rb_ary_new_from_args (4, INT2NUM (directory_opens), INT2NUM (identity_calls),
    INT2NUM (short_calls), INT2NUM (long_calls));
  stopped = watch_index_consistent (ctx, watchers, count);
  ev_stat_stop (loop, ctx->watches[ctx->watchcnt - 1]->watcher);
  ev_stat_start (loop, &watchers[1]);
  ctx->watches[0]->parent.dir->failed = 1;
  poll_active (loop, watchers, count);
  replaced = watch_index_consistent (ctx, watchers, count);
  rb_ary_push (result, stopped ? Qtrue : Qfalse);
  rb_ary_push (result, replaced ? Qtrue : Qfalse);
  for (i = 0; i < count; ++i)
    {
      ev_stat_stop (loop, &watchers[i]);
    }
  ev_loop_destroy (loop);
  WSACleanup ();
  return result;
}

static VALUE
alias_recovery (VALUE self, VALUE path)
{
  struct ev_loop *loop;
  struct ev_win32_fs *ctx;
  struct ev_win32_watch *watch;
  struct ev_win32_dir event_dir;
  FILE_NOTIFY_INFORMATION *event = (FILE_NOTIFY_INFORMATION *)event_dir.buffer;
  ev_stat watcher;
  WSADATA data;
  FILE *file;
  size_t length;
  int initial, invalidated, retried, cached;
  const char *filename = StringValueCStr (path);
  if (WSAStartup (MAKEWORD (2, 2), &data))
    rb_raise (rb_eRuntimeError, "WSAStartup failed");
  file = fopen (filename, "w");
  if (!file)
    rb_sys_fail (filename);
  fputs ("a", file);
  fclose (file);
  loop = ev_loop_new (EVFLAG_NOENV | EVBACKEND_SELECT);
  if (!loop)
    rb_raise (rb_eRuntimeError, "ev_loop_new failed");
  ev_stat_init (&watcher, changed_cb, filename, 60);
  fail_alias = 1;
  ev_stat_start (loop, &watcher);
  ctx = loop->win32_fs;
  if (!ctx)
    rb_raise (rb_eRuntimeError, "notification worker did not start");
  watch = ctx->watches[0];
  initial = !watch->shortname && !watch->longname;
  fail_alias = 0;
  stat_timer_cb (loop, &watcher.timer, EV_TIMER);
  initial = initial && watch->shortname && watch->longname;

  /* A replacement name record must refresh aliases even with equal stat data.
   * Use a separate buffer: the real directory buffer belongs to pending I/O. */
  memset (&event_dir, 0, sizeof (event_dir));
  event_dir.subscribers = watch->parent.dir->subscribers;
  length = wcslen (watch->name) * sizeof (wchar_t);
  event->Action = FILE_ACTION_RENAMED_NEW_NAME;
  event->FileNameLength = (DWORD)length;
  memcpy (event->FileName, watch->name, length);
  EnterCriticalSection (&ctx->lock);
  win32_dir_notify (ctx, &event_dir, (DWORD)(offsetof (FILE_NOTIFY_INFORMATION, FileName) + length));
  LeaveCriticalSection (&ctx->lock);
  fail_alias = 1;
  win32_fs_cb (loop, &ctx->async, EV_ASYNC);
  invalidated = !watch->shortname && !watch->longname;
  fail_alias = 0;
  stat_timer_cb (loop, &watcher.timer, EV_TIMER);
  retried = watch->shortname && watch->longname;
  short_calls = long_calls = 0;
  stat_timer_cb (loop, &watcher.timer, EV_TIMER);
  cached = !short_calls && !long_calls;
  ev_stat_stop (loop, &watcher);
  ev_loop_destroy (loop);
  WSACleanup ();
  return rb_ary_new_from_args (4, initial ? Qtrue : Qfalse, invalidated ? Qtrue : Qfalse,
    retried ? Qtrue : Qfalse, cached ? Qtrue : Qfalse);
}

RUBY_FUNC_EXPORTED void
Init_coolio_stat_native (void)
{
  VALUE module = rb_define_module ("CoolioStatNativeSpec");
  rb_define_singleton_method (module, "path", stat_path, 2);
  rb_define_singleton_method (module, "check_pipe", check_pipe, 1);
  rb_define_singleton_method (module, "check_shutdown", check_shutdown, 1);
  rb_define_singleton_method (module, "notification_enabled", notification_enabled, 2);
  rb_define_singleton_method (module, "path_work", path_work, 2);
  rb_define_singleton_method (module, "watch_index", watch_index, 1);
  rb_define_singleton_method (module, "alias_recovery", alias_recovery, 1);
}
