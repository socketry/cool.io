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
static int directory_opens, identity_calls, name_calls, fail_alias;

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

static const wchar_t *long_prefix;

static HANDLE WINAPI
test_find_file (LPCWSTR path, FINDEX_INFO_LEVELS level, LPVOID data, FINDEX_SEARCH_OPS search,
                LPVOID filter, DWORD flags)
{
  HANDLE handle;
  wchar_t *name;
  size_t extra;
  ++name_calls;
  if (fail_alias)
    {
      SetLastError (ERROR_ACCESS_DENIED);
      return INVALID_HANDLE_VALUE;
    }
  handle = FindFirstFileExW (path, level, data, search, filter, flags);
  if (handle != INVALID_HANDLE_VALUE && long_prefix)
    {
      name = ((WIN32_FIND_DATAW *)data)->cFileName;
      extra = wcslen (long_prefix);
      if (wcslen (name) + extra >= MAX_PATH)
        {
          FindClose (handle);
          SetLastError (ERROR_INSUFFICIENT_BUFFER);
          return INVALID_HANDLE_VALUE;
        }
      memmove (name + extra, name, (wcslen (name) + 1) * sizeof (wchar_t));
      memcpy (name, long_prefix, extra * sizeof (wchar_t));
    }
  return handle;
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
#define FindFirstFileExW test_find_file
#define EV_STANDALONE 1
#define EV_USE_REALTIME 0
#define EV_USE_MONOTONIC 0
#define EV_WIN32_STAT_CODEPAGE CP_UTF8
#include "ev.c"
#undef socket
#undef GetQueuedCompletionStatus
#undef CreateFileW
#undef GetFileInformationByHandle
#undef FindFirstFileExW

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
  ev_tstamp deadline;
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
  deadline = ev_time () + 5;
  while (!loop->win32_fs && ev_time () < deadline)
    {
      ev_run (loop, EVRUN_ONCE); /* The next polling tick retries notification setup. */
    }
  /* One listener plus one client socket; ev_async_start must reuse this pair. */
  if (!loop->win32_fs)
    {
      error = "polling did not retry notification setup";
    }
  else if (!ev_is_active (&loop->pipe_w) || socket_calls != 2)
    {
      error = "notification recovery did not reuse the prepared pipe";
    }
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
      event->Action = FILE_ACTION_ADDED;
      event->FileNameLength = sizeof (name) - sizeof (wchar_t);
      memcpy (event->FileName, name, event->FileNameLength);
      EnterCriticalSection (&ctx->lock);
      memcpy (&event_dir, ctx->dirs, offsetof (struct ev_win32_dir, buffer));
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
  directory_opens = identity_calls = name_calls = 0;
  if (notification)
    win32_fs_cb (loop, &ctx->async, EV_ASYNC);
  else
    for (i = 0; i < count; ++i)
      stat_timer_cb (loop, &watchers[i].timer, EV_TIMER);
  for (i = 0; i < count; ++i)
    valid = valid && watchers[i].attr.st_size == 2;
  result = rb_ary_new_from_args (4, INT2NUM (directory_opens), INT2NUM (identity_calls),
    INT2NUM (name_calls), valid ? Qtrue : Qfalse);
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
  directory_opens = identity_calls = name_calls = 0;
  poll_active (loop, watchers, count);
  result = rb_ary_new_from_args (3, INT2NUM (directory_opens), INT2NUM (identity_calls),
    INT2NUM (name_calls));
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

static void
clear_dirty (struct ev_win32_fs *ctx)
{
  struct ev_win32_watch *watch;
  while ((watch = ctx->dirty))
    {
      ctx->dirty = watch->dirty_next;
      if (ctx->dirty)
        {
          ctx->dirty->dirty_prev = &ctx->dirty;
        }
      watch->dirty = 0;
    }
}

static void
notify_records (struct ev_win32_fs *ctx, struct ev_win32_dir *dir,
                const wchar_t *const *names, int count, DWORD action)
{
  struct ev_win32_dir event_dir;
  char *record = (char *)event_dir.buffer;
  int i;
  memset (&event_dir, 0, sizeof (event_dir));
  memcpy (&event_dir, dir, offsetof (struct ev_win32_dir, buffer));
  for (i = 0; i < count; ++i)
    {
      FILE_NOTIFY_INFORMATION *event = (FILE_NOTIFY_INFORMATION *)record;
      DWORD length = (DWORD)(wcslen (names[i]) * sizeof (wchar_t));
      DWORD size = (DWORD)((offsetof (FILE_NOTIFY_INFORMATION, FileName) + length + 3) & ~3u);
      event->Action = action;
      event->FileNameLength = length;
      memcpy (event->FileName, names[i], length);
      event->NextEntryOffset = i + 1 < count ? size : 0;
      record += size;
    }
  win32_dir_notify (ctx, &event_dir, (DWORD)(record - (char *)event_dir.buffer));
}

static void
notify_names (struct ev_win32_fs *ctx, struct ev_win32_dir *dir,
              const wchar_t *const *names, int count)
{
  notify_records (ctx, dir, names, count, FILE_ACTION_MODIFIED);
}

static VALUE
dirty_flags (const int *dirty, int count)
{
  VALUE flags = rb_ary_new ();
  int i;
  for (i = 0; i < count; ++i)
    {
      rb_ary_push (flags, dirty[i] ? Qtrue : Qfalse);
    }
  return flags;
}

static void
make_dir (const char *parent, const char *relative)
{
  char path[1024];
  snprintf (path, sizeof (path), "%s/%s", parent, relative);
  if (!CreateDirectoryA (path, 0) && GetLastError () != ERROR_ALREADY_EXISTS)
    {
      WSACleanup ();
      rb_raise (rb_eRuntimeError, "CreateDirectoryA failed: %s", path);
    }
}

static void
make_file (const char *parent, const char *relative)
{
  char path[1024];
  FILE *file;
  snprintf (path, sizeof (path), "%s/%s", parent, relative);
  file = fopen (path, "w");
  if (!file)
    {
      WSACleanup ();
      rb_sys_fail (path);
    }
  fputs ("a", file);
  fclose (file);
}

static int
notified (struct ev_win32_fs *ctx, struct ev_win32_dir *dir, const wchar_t *name)
{
  int dirty;
  EnterCriticalSection (&ctx->lock);
  clear_dirty (ctx);
  notify_names (ctx, dir, &name, 1);
  dirty = ctx->dirty != 0;
  clear_dirty (ctx);
  LeaveCriticalSection (&ctx->lock);
  return dirty;
}

static VALUE
name_index (VALUE self, VALUE directory)
{
  enum { count = 7 };
  static const char *const files[] = { "a/x0.txt", "a/x1.txt", "a/x2.txt", "a/x3.txt", "b/y.txt" };
  static const char *const watched[count] = {
    "a/x0.txt", "A/X1.Txt", "a/x2.txt", "a/x3.txt", "b/y.txt", "a", "a/\xc3\x89.txt"
  };
  static const wchar_t *const records[] = {
    L"", L"x1.txt", L"x2.txt", L"x3.txt", L"\u00e9.txt", L"unrelated.tmp"
  };
  struct ev_loop *loop;
  struct ev_win32_fs *ctx;
  struct ev_win32_dir *dir;
  struct ev_win32_watch *watch;
  ev_stat watchers[count];
  char paths[count][1024];
  WSADATA data;
  VALUE result;
  int i, shared, named[count], overflowed[count];
  const char *parent = StringValueCStr (directory);
  if (WSAStartup (MAKEWORD (2, 2), &data))
    {
      rb_raise (rb_eRuntimeError, "WSAStartup failed");
    }
  make_dir (parent, "a");
  make_dir (parent, "b");
  for (i = 0; i < (int)(sizeof (files) / sizeof (*files)); ++i)
    {
      make_file (parent, files[i]);
    }
  loop = ev_loop_new (EVFLAG_NOENV | EVBACKEND_SELECT);
  if (!loop)
    {
      WSACleanup ();
      rb_raise (rb_eRuntimeError, "ev_loop_new failed");
    }
  fail_alias = 1;
  for (i = 0; i < count; ++i)
    {
      snprintf (paths[i], sizeof (paths[i]), "%s/%s", parent, watched[i]);
      ev_stat_init (&watchers[i], changed_cb, paths[i], 60);
      ev_stat_start (loop, &watchers[i]);
    }
  fail_alias = 0;
  ctx = loop->win32_fs;
  if (!ctx)
    {
      ev_loop_destroy (loop);
      WSACleanup ();
      rb_raise (rb_eRuntimeError, "notification worker did not start");
    }
  dir = win32_fs_watch (ctx, &watchers[0])->parent.dir;
  shared = win32_fs_watch (ctx, &watchers[1])->parent.dir == dir
    && win32_fs_watch (ctx, &watchers[2])->parent.dir == dir
    && win32_fs_watch (ctx, &watchers[3])->parent.dir == dir
    && win32_fs_watch (ctx, &watchers[4])->parent.dir != dir
    && win32_fs_watch (ctx, &watchers[5])->children.dir == dir
    && win32_fs_watch (ctx, &watchers[6])->parent.dir == dir;
  ev_stat_stop (loop, &watchers[2]);
  EnterCriticalSection (&ctx->lock);
  clear_dirty (ctx);
  notify_names (ctx, dir, records, sizeof (records) / sizeof (*records));
  for (i = 0; i < count; ++i)
    {
      watch = win32_fs_watch (ctx, &watchers[i]);
      named[i] = watch && watch->dirty;
    }
  clear_dirty (ctx);
  notify_names (ctx, dir, records, 0);
  for (i = 0; i < count; ++i)
    {
      watch = win32_fs_watch (ctx, &watchers[i]);
      overflowed[i] = watch && watch->dirty;
    }
  clear_dirty (ctx);
  LeaveCriticalSection (&ctx->lock);
  result = rb_ary_new_from_args (3, shared ? Qtrue : Qfalse,
    dirty_flags (named, count), dirty_flags (overflowed, count));
  for (i = 0; i < count; ++i)
    {
      ev_stat_stop (loop, &watchers[i]);
    }
  ev_loop_destroy (loop);
  WSACleanup ();
  return result;
}

static VALUE
alias_index (VALUE self, VALUE directory)
{
  struct ev_loop *loop;
  struct ev_win32_fs *ctx;
  struct ev_win32_dir *dir;
  struct ev_win32_watch *watch;
  ev_stat watched, keeper;
  char watched_path[1024], keeper_path[1024];
  WSADATA data;
  int before, stale, renamed, plain, stopped;
  const char *parent = StringValueCStr (directory);
  if (WSAStartup (MAKEWORD (2, 2), &data))
    {
      rb_raise (rb_eRuntimeError, "WSAStartup failed");
    }
  make_dir (parent, "c");
  make_file (parent, "c/z.txt");
  make_file (parent, "c/keep.txt");
  snprintf (watched_path, sizeof (watched_path), "%s/c/z.txt", parent);
  snprintf (keeper_path, sizeof (keeper_path), "%s/c/keep.txt", parent);
  loop = ev_loop_new (EVFLAG_NOENV | EVBACKEND_SELECT);
  if (!loop)
    {
      WSACleanup ();
      rb_raise (rb_eRuntimeError, "ev_loop_new failed");
    }
  long_prefix = L"long-";
  ev_stat_init (&watched, changed_cb, watched_path, 60);
  ev_stat_start (loop, &watched);
  ev_stat_init (&keeper, changed_cb, keeper_path, 60);
  ev_stat_start (loop, &keeper);
  ctx = loop->win32_fs;
  if (!ctx)
    {
      long_prefix = 0;
      ev_loop_destroy (loop);
      WSACleanup ();
      rb_raise (rb_eRuntimeError, "notification worker did not start");
    }
  watch = win32_fs_watch (ctx, &watched);
  dir = watch->parent.dir;
  before = notified (ctx, dir, L"long-z.txt");
  long_prefix = L"other-";
  EnterCriticalSection (&ctx->lock);
  win32_watch_names (watch);
  LeaveCriticalSection (&ctx->lock);
  long_prefix = 0;
  stale = notified (ctx, dir, L"long-z.txt");
  renamed = notified (ctx, dir, L"other-z.txt");
  plain = notified (ctx, dir, L"z.txt");
  ev_stat_stop (loop, &watched);
  stopped = !notified (ctx, dir, L"other-z.txt") && !notified (ctx, dir, L"z.txt");
  ev_stat_stop (loop, &keeper);
  ev_loop_destroy (loop);
  WSACleanup ();
  return rb_ary_new_from_args (5, before ? Qtrue : Qfalse, stale ? Qtrue : Qfalse,
    renamed ? Qtrue : Qfalse, plain ? Qtrue : Qfalse, stopped ? Qtrue : Qfalse);
}

static VALUE
index_growth (VALUE self, VALUE directory)
{
  enum { names = 40, dirs = 20 };
  struct ev_loop *loop;
  struct ev_win32_fs *ctx;
  struct ev_win32_dir *dir;
  struct ev_win32_watch *watch;
  ev_stat files[names], first[dirs], second[dirs];
  char file_paths[names][1024], first_paths[dirs][1024], second_paths[dirs][1024], name[64];
  wchar_t record_names[names][16];
  const wchar_t *records[names];
  WSADATA data;
  int i, j, queued = 1, routed = 1, shared = 1, grown, count = 0;
  const char *parent = StringValueCStr (directory);
  if (WSAStartup (MAKEWORD (2, 2), &data))
    {
      rb_raise (rb_eRuntimeError, "WSAStartup failed");
    }
  make_dir (parent, "g");
  for (i = 0; i < names; ++i)
    {
      snprintf (name, sizeof (name), "g/n%d.txt", i);
      make_file (parent, name);
      snprintf (file_paths[i], sizeof (file_paths[i]), "%s/%s", parent, name);
      snprintf (name, sizeof (name), "n%d.txt", i);
      for (j = 0; name[j]; ++j)
        {
          record_names[i][j] = (wchar_t)name[j];
        }
      record_names[i][j] = 0;
      records[i] = record_names[i];
    }
  for (i = 0; i < dirs; ++i)
    {
      snprintf (name, sizeof (name), "h%d", i);
      make_dir (parent, name);
      snprintf (name, sizeof (name), "h%d/f.txt", i);
      make_file (parent, name);
      snprintf (first_paths[i], sizeof (first_paths[i]), "%s/h%d/f.txt", parent, i);
      snprintf (second_paths[i], sizeof (second_paths[i]), "%s/H%d/F.TXT", parent, i);
    }
  loop = ev_loop_new (EVFLAG_NOENV | EVBACKEND_SELECT);
  if (!loop)
    {
      WSACleanup ();
      rb_raise (rb_eRuntimeError, "ev_loop_new failed");
    }
  fail_alias = 1;
  for (i = 0; i < names; ++i)
    {
      ev_stat_init (&files[i], changed_cb, file_paths[i], 60);
      ev_stat_start (loop, &files[i]);
    }
  for (i = 0; i < dirs; ++i)
    {
      ev_stat_init (&first[i], changed_cb, first_paths[i], 60);
      ev_stat_start (loop, &first[i]);
      ev_stat_init (&second[i], changed_cb, second_paths[i], 60);
      ev_stat_start (loop, &second[i]);
    }
  fail_alias = 0;
  ctx = loop->win32_fs;
  if (!ctx)
    {
      ev_loop_destroy (loop);
      WSACleanup ();
      rb_raise (rb_eRuntimeError, "notification worker did not start");
    }
  for (i = 0; i < dirs; ++i)
    {
      shared = shared && win32_fs_watch (ctx, &first[i])->parent.dir
        == win32_fs_watch (ctx, &second[i])->parent.dir;
    }
  dir = win32_fs_watch (ctx, &files[1])->parent.dir;
  grown = dir->namemask > 15 && ctx->dirmask > 15;
  EnterCriticalSection (&ctx->lock);
  clear_dirty (ctx);
  notify_names (ctx, dir, records, names);
  LeaveCriticalSection (&ctx->lock);
  for (i = 0; i < names; i += 3)
    {
      ev_stat_stop (loop, &files[i]);
    }
  EnterCriticalSection (&ctx->lock);
  for (watch = ctx->dirty; watch; watch = watch->dirty_next)
    {
      queued = queued && watch->dirty && *watch->dirty_prev == watch;
      if (watch->parent.dir == dir)
        {
          ++count;
        }
    }
  queued = queued && count == names - (names + 2) / 3;
  clear_dirty (ctx);
  notify_names (ctx, dir, records, names);
  for (i = 0; i < names; ++i)
    {
      watch = win32_fs_watch (ctx, &files[i]);
      routed = routed && (i % 3 ? watch && watch->dirty : !watch);
    }
  clear_dirty (ctx);
  LeaveCriticalSection (&ctx->lock);
  for (i = 0; i < names; ++i)
    {
      ev_stat_stop (loop, &files[i]);
    }
  for (i = 0; i < dirs; ++i)
    {
      ev_stat_stop (loop, &first[i]);
      ev_stat_stop (loop, &second[i]);
    }
  ev_loop_destroy (loop);
  WSACleanup ();
  return rb_ary_new_from_args (4, grown ? Qtrue : Qfalse, shared ? Qtrue : Qfalse,
    queued ? Qtrue : Qfalse, routed ? Qtrue : Qfalse);
}

static wchar_t *
absolute_path (const char *path)
{
  wchar_t *absolute = win32_fs_path (path, 0);
  if (!absolute)
    {
      rb_raise (rb_eRuntimeError, "win32_fs_path failed: %s", path);
    }
  return absolute;
}

static int
lookup_skipped (wchar_t *path)
{
  struct ev_win32_watch watch;
  int skipped, i;
  memset (&watch, 0, sizeof (watch));
  watch.path = path;
  watch.name = wcsrchr (path, L'\\') + 1;
  name_calls = 0;
  win32_watch_names (&watch);
  skipped = !name_calls && !watch.keys[1].key && !watch.keys[2].key;
  for (i = 1; i < 3; ++i)
    {
      free (watch.keys[i].key);
    }
  return skipped;
}

static VALUE
name_guards (VALUE self, VALUE directory)
{
  static const wchar_t *const roots[] = {
    L"C:\\", L"\\\\?\\C:", L"\\\\localhost\\share", L"\\\\localhost\\share\\",
    L"\\\\?\\UNC\\localhost\\share"
  };
  wchar_t root[64], *path;
  char pattern[1024];
  VALUE result = rb_hash_new ();
  int i, skipped = 1;
  const char *parent = StringValueCStr (directory);
  for (i = 0; i < (int)(sizeof (roots) / sizeof (*roots)); ++i)
    {
      wcscpy (root, roots[i]);
      skipped = skipped && lookup_skipped (root);
    }
  rb_hash_aset (result, ID2SYM (rb_intern ("root")), skipped ? Qtrue : Qfalse);
  make_dir (parent, "wild");
  make_file (parent, "wild/x0.txt");
  snprintf (pattern, sizeof (pattern), "%s/wild/x*.txt", parent);
  path = absolute_path (pattern);
  skipped = lookup_skipped (path);
  free (path);
  rb_hash_aset (result, ID2SYM (rb_intern ("wildcard")), skipped ? Qtrue : Qfalse);
  return result;
}

static int
routed_to (struct ev_win32_fs *ctx, ev_stat *watcher, const wchar_t *name)
{
  struct ev_win32_watch *watch = win32_fs_watch (ctx, watcher);
  int dirty;
  EnterCriticalSection (&ctx->lock);
  clear_dirty (ctx);
  notify_names (ctx, watch->parent.dir, &name, 1);
  dirty = watch->dirty;
  clear_dirty (ctx);
  LeaveCriticalSection (&ctx->lock);
  return dirty;
}

static VALUE
short_alias (VALUE self, VALUE directory)
{
  struct ev_loop *loop;
  struct ev_win32_fs *ctx;
  ev_stat long_watcher, short_watcher;
  char long_path[1024], short_path[1024];
  wchar_t *path, *buffer, *short_name;
  WSADATA data;
  DWORD length, written;
  VALUE result;
  int prefix;
  const char *parent = StringValueCStr (directory);
  if (WSAStartup (MAKEWORD (2, 2), &data))
    {
      rb_raise (rb_eRuntimeError, "WSAStartup failed");
    }
  make_dir (parent, "short");
  make_file (parent, "short/long-file-name.txt");
  snprintf (long_path, sizeof (long_path), "%s/short/long-file-name.txt", parent);
  path = absolute_path (long_path);
  length = GetShortPathNameW (path, 0, 0);
  buffer = length ? (wchar_t *)malloc (length * sizeof (wchar_t)) : 0;
  written = buffer ? GetShortPathNameW (path, buffer, length) : 0;
  free (path);
  if (!written || written >= length)
    {
      free (buffer);
      WSACleanup ();
      rb_raise (rb_eRuntimeError, "GetShortPathNameW failed");
    }
  short_name = wcsrchr (buffer, L'\\') + 1;
  if (!_wcsicmp (short_name, L"long-file-name.txt"))
    {
      free (buffer);
      WSACleanup ();
      return Qnil;
    }
  prefix = snprintf (short_path, sizeof (short_path), "%s/short/", parent);
  if (!WideCharToMultiByte (CP_UTF8, 0, short_name, -1, short_path + prefix,
                            (int)sizeof (short_path) - prefix, 0, 0))
    {
      free (buffer);
      WSACleanup ();
      rb_raise (rb_eRuntimeError, "WideCharToMultiByte failed");
    }
  loop = ev_loop_new (EVFLAG_NOENV | EVBACKEND_SELECT);
  if (!loop)
    {
      free (buffer);
      WSACleanup ();
      rb_raise (rb_eRuntimeError, "ev_loop_new failed");
    }
  ev_stat_init (&long_watcher, changed_cb, long_path, 60);
  ev_stat_start (loop, &long_watcher);
  ev_stat_init (&short_watcher, changed_cb, short_path, 60);
  ev_stat_start (loop, &short_watcher);
  ctx = loop->win32_fs;
  if (!ctx)
    {
      free (buffer);
      ev_loop_destroy (loop);
      WSACleanup ();
      rb_raise (rb_eRuntimeError, "notification worker did not start");
    }
  result = rb_hash_new ();
  rb_hash_aset (result, ID2SYM (rb_intern ("short")),
    routed_to (ctx, &long_watcher, short_name) ? Qtrue : Qfalse);
  rb_hash_aset (result, ID2SYM (rb_intern ("long")),
    routed_to (ctx, &short_watcher, L"long-file-name.txt") ? Qtrue : Qfalse);
  ev_stat_stop (loop, &long_watcher);
  ev_stat_stop (loop, &short_watcher);
  ev_loop_destroy (loop);
  free (buffer);
  WSACleanup ();
  return result;
}

static VALUE
children_cwd (VALUE self, VALUE first, VALUE second, VALUE polling)
{
  struct ev_loop *loop;
  struct ev_win32_fs *ctx;
  struct ev_win32_dir *shared_dir, *children_dir;
  ev_stat dir_watcher, file_watcher;
  WSADATA data;
  wchar_t original[MAX_PATH], *first_dir, *second_dir, *second_rel;
  VALUE file = rb_sprintf ("%s/rel/f.txt", StringValueCStr (first));
  VALUE moved_rel = rb_sprintf ("%s/rel", StringValueCStr (second));
  VALUE result;
  DWORD length;
  int shared, failed, moved, followed, cwd_changed;
  length = GetCurrentDirectoryW (MAX_PATH, original);
  if (!length || length >= MAX_PATH)
    {
      rb_raise (rb_eRuntimeError, "GetCurrentDirectoryW failed");
    }
  first_dir = absolute_path (StringValueCStr (first));
  second_dir = absolute_path (StringValueCStr (second));
  second_rel = absolute_path (StringValueCStr (moved_rel));
  if (!SetCurrentDirectoryW (first_dir) || WSAStartup (MAKEWORD (2, 2), &data))
    {
      SetCurrentDirectoryW (original);
      free (first_dir);
      free (second_dir);
      free (second_rel);
      rb_raise (rb_eRuntimeError, "could not prepare the current directory and Winsock");
    }
  loop = ev_loop_new (EVFLAG_NOENV | EVBACKEND_SELECT);
  if (!loop)
    {
      SetCurrentDirectoryW (original);
      WSACleanup ();
      free (first_dir);
      free (second_dir);
      free (second_rel);
      rb_raise (rb_eRuntimeError, "ev_loop_new failed");
    }
  ev_stat_init (&dir_watcher, changed_cb, "rel", 60);
  ev_stat_start (loop, &dir_watcher);
  ev_stat_init (&file_watcher, changed_cb, StringValueCStr (file), 60);
  ev_stat_start (loop, &file_watcher);
  ctx = loop->win32_fs;
  if (!ctx || !win32_fs_watch (ctx, &dir_watcher) || !win32_fs_watch (ctx, &file_watcher))
    {
      ev_stat_stop (loop, &dir_watcher);
      ev_stat_stop (loop, &file_watcher);
      ev_loop_destroy (loop);
      SetCurrentDirectoryW (original);
      WSACleanup ();
      free (first_dir);
      free (second_dir);
      free (second_rel);
      rb_raise (rb_eRuntimeError, "the watchers were not registered");
    }
  EnterCriticalSection (&ctx->lock);
  shared_dir = win32_fs_watch (ctx, &file_watcher)->parent.dir;
  shared = win32_fs_watch (ctx, &dir_watcher)->children.dir == shared_dir;
  LeaveCriticalSection (&ctx->lock);
  cwd_changed = SetCurrentDirectoryW (second_dir);
  if (NUM2INT (polling))
    {
      stat_timer_cb (loop, &dir_watcher.timer, EV_TIMER);
    }
  else
    {
      win32_fs_add (loop, &dir_watcher, 0);
    }
  EnterCriticalSection (&ctx->lock);
  failed = shared_dir->failed;
  children_dir = win32_fs_watch (ctx, &dir_watcher)->children.dir;
  followed = children_dir && !_wcsicmp (children_dir->path, second_rel);
  LeaveCriticalSection (&ctx->lock);
  stat_timer_cb (loop, &file_watcher.timer, EV_TIMER);
  EnterCriticalSection (&ctx->lock);
  moved = win32_fs_watch (ctx, &file_watcher)->parent.dir != shared_dir;
  LeaveCriticalSection (&ctx->lock);
  SetCurrentDirectoryW (original);
  ev_stat_stop (loop, &dir_watcher);
  ev_stat_stop (loop, &file_watcher);
  ev_loop_destroy (loop);
  WSACleanup ();
  free (first_dir);
  free (second_dir);
  free (second_rel);
  RB_GC_GUARD (file);
  RB_GC_GUARD (moved_rel);
  if (!cwd_changed)
    {
      rb_raise (rb_eRuntimeError, "could not change the current directory");
    }
  result = rb_hash_new ();
  rb_hash_aset (result, ID2SYM (rb_intern ("shared")), shared ? Qtrue : Qfalse);
  rb_hash_aset (result, ID2SYM (rb_intern ("failed")), failed ? Qtrue : Qfalse);
  rb_hash_aset (result, ID2SYM (rb_intern ("moved")), moved ? Qtrue : Qfalse);
  rb_hash_aset (result, ID2SYM (rb_intern ("followed")), followed ? Qtrue : Qfalse);
  return result;
}

static int
follows (struct ev_win32_fs *ctx, struct ev_win32_dir *dir, const wchar_t *path)
{
  int same;
  EnterCriticalSection (&ctx->lock);
  same = dir && win32_dir_matches (dir, path);
  LeaveCriticalSection (&ctx->lock);
  return same;
}

static VALUE
replaced_dir (VALUE self, VALUE directory, VALUE mode)
{
  static const wchar_t *const watched_name[] = { L"P" };
  struct ev_loop *loop;
  struct ev_win32_fs *ctx;
  struct ev_win32_watch *watch;
  ev_stat dir_watcher, file_watcher;
  WSADATA data;
  int kind = NUM2INT (mode);
  int with_file = kind != 2;
  const char *parent = StringValueCStr (directory);
  VALUE root = rb_sprintf ("replaced-%d", kind);
  VALUE watched_rel = rb_sprintf ("replaced-%d/P", kind);
  VALUE file_rel = rb_sprintf ("replaced-%d/P/f.txt", kind);
  VALUE watched = rb_sprintf ("%s/%s", parent, StringValueCStr (watched_rel));
  VALUE moved = rb_sprintf ("%s/replaced-%d/P.old", parent, kind);
  VALUE file = rb_sprintf ("%s/%s", parent, StringValueCStr (file_rel));
  VALUE result;
  wchar_t *watched_dir = absolute_path (StringValueCStr (watched));
  wchar_t *moved_dir = absolute_path (StringValueCStr (moved));
  int replaced, directory_followed, file_followed = 0;
  if (WSAStartup (MAKEWORD (2, 2), &data))
    {
      free (watched_dir);
      free (moved_dir);
      rb_raise (rb_eRuntimeError, "WSAStartup failed");
    }
  make_dir (parent, StringValueCStr (root));
  make_dir (parent, StringValueCStr (watched_rel));
  make_file (parent, StringValueCStr (file_rel));
  loop = ev_loop_new (EVFLAG_NOENV | EVBACKEND_SELECT);
  if (!loop)
    {
      free (watched_dir);
      free (moved_dir);
      WSACleanup ();
      rb_raise (rb_eRuntimeError, "ev_loop_new failed");
    }
  ev_stat_init (&dir_watcher, changed_cb, StringValueCStr (watched), 60);
  ev_stat_start (loop, &dir_watcher);
  ev_stat_init (&file_watcher, changed_cb, StringValueCStr (file), 60);
  if (with_file)
    {
      ev_stat_start (loop, &file_watcher);
    }
  ctx = loop->win32_fs;
  if (!ctx || !win32_fs_watch (ctx, &dir_watcher) || !win32_fs_watch (ctx, &dir_watcher)->children.dir
      || (with_file && !win32_fs_watch (ctx, &file_watcher)))
    {
      ev_stat_stop (loop, &dir_watcher);
      ev_stat_stop (loop, &file_watcher);
      ev_loop_destroy (loop);
      free (watched_dir);
      free (moved_dir);
      WSACleanup ();
      rb_raise (rb_eRuntimeError, "the watchers were not registered");
    }
  replaced = MoveFileW (watched_dir, moved_dir);
  if (replaced && kind == 2)
    {
      win32_fs_add (loop, &dir_watcher, 0);
    }
  replaced = replaced && CreateDirectoryW (watched_dir, 0);
  EnterCriticalSection (&ctx->lock);
  clear_dirty (ctx);
  watch = win32_fs_watch (ctx, &dir_watcher);
  if (kind == 1)
    {
      notify_records (ctx, watch->parent.dir, watched_name, 1, FILE_ACTION_RENAMED_OLD_NAME);
    }
  notify_records (ctx, watch->parent.dir, watched_name, 1, FILE_ACTION_ADDED);
  LeaveCriticalSection (&ctx->lock);
  win32_fs_cb (loop, &ctx->async, EV_ASYNC);
  directory_followed = follows (ctx, win32_fs_watch (ctx, &dir_watcher)->children.dir, watched_dir);
  if (with_file)
    {
      file_followed = follows (ctx, win32_fs_watch (ctx, &file_watcher)->parent.dir, watched_dir);
    }
  ev_stat_stop (loop, &dir_watcher);
  ev_stat_stop (loop, &file_watcher);
  ev_loop_destroy (loop);
  free (watched_dir);
  free (moved_dir);
  WSACleanup ();
  RB_GC_GUARD (root);
  RB_GC_GUARD (watched_rel);
  RB_GC_GUARD (file_rel);
  RB_GC_GUARD (watched);
  RB_GC_GUARD (moved);
  RB_GC_GUARD (file);
  if (!replaced)
    {
      rb_raise (rb_eRuntimeError, "could not replace the watched directory");
    }
  result = rb_hash_new ();
  rb_hash_aset (result, ID2SYM (rb_intern ("directory")), directory_followed ? Qtrue : Qfalse);
  rb_hash_aset (result, ID2SYM (rb_intern ("file")),
    with_file ? (file_followed ? Qtrue : Qfalse) : Qnil);
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
  initial = watch->names_failed;
  fail_alias = 0;
  stat_timer_cb (loop, &watcher.timer, EV_TIMER);
  initial = initial && !watch->names_failed;

  /* A replacement name record must refresh aliases even with equal stat data.
   * Use a separate buffer: the real directory buffer belongs to pending I/O. */
  memset (&event_dir, 0, sizeof (event_dir));
  length = wcslen (watch->name) * sizeof (wchar_t);
  event->Action = FILE_ACTION_RENAMED_NEW_NAME;
  event->FileNameLength = (DWORD)length;
  memcpy (event->FileName, watch->name, length);
  EnterCriticalSection (&ctx->lock);
  memcpy (&event_dir, watch->parent.dir, offsetof (struct ev_win32_dir, buffer));
  win32_dir_notify (ctx, &event_dir, (DWORD)(offsetof (FILE_NOTIFY_INFORMATION, FileName) + length));
  LeaveCriticalSection (&ctx->lock);
  fail_alias = 1;
  win32_fs_cb (loop, &ctx->async, EV_ASYNC);
  invalidated = watch->names_failed;
  fail_alias = 0;
  stat_timer_cb (loop, &watcher.timer, EV_TIMER);
  retried = !watch->names_failed;
  name_calls = 0;
  stat_timer_cb (loop, &watcher.timer, EV_TIMER);
  cached = !name_calls;
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
  rb_define_singleton_method (module, "name_index", name_index, 1);
  rb_define_singleton_method (module, "alias_index", alias_index, 1);
  rb_define_singleton_method (module, "name_guards", name_guards, 1);
  rb_define_singleton_method (module, "short_alias", short_alias, 1);
  rb_define_singleton_method (module, "index_growth", index_growth, 1);
  rb_define_singleton_method (module, "children_cwd", children_cwd, 3);
  rb_define_singleton_method (module, "replaced_dir", replaced_dir, 2);
  rb_define_singleton_method (module, "alias_recovery", alias_recovery, 1);
}
