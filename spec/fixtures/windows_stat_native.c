/* Compile a private libev copy to exercise path handling and inject socket
 * failures without adding test hooks to the cool.io extension. */
#include <ruby.h>
#include <ruby/encoding.h>
#include <winsock2.h>
#include <windows.h>
#include <errno.h>

static int fail_socket, socket_calls, syserr_calls, changed, timed_out;
static HANDLE completion_entered, completion_release;
static volatile LONG delay_completion;

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
#define EV_STANDALONE 1
#define EV_USE_REALTIME 0
#define EV_USE_MONOTONIC 0
#define EV_WIN32_STAT_CODEPAGE CP_UTF8
#include "ev.c"
#undef socket
#undef GetQueuedCompletionStatus

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

RUBY_FUNC_EXPORTED void
Init_coolio_stat_native (void)
{
  VALUE module = rb_define_module ("CoolioStatNativeSpec");
  rb_define_singleton_method (module, "path", stat_path, 2);
  rb_define_singleton_method (module, "check_pipe", check_pipe, 1);
  rb_define_singleton_method (module, "check_shutdown", check_shutdown, 1);
}
