/* Windows notification support for ev_stat. Included by ev.c.
 * Copyright (C) 2026. Distributed under the same terms as ev.c.
 *
 * Only the loop thread touches ev_stat or runs callbacks. The IOCP thread
 * rearms directory reads and publishes dirty flags under a critical section.
 * Directory events (including overflow) are hints to re-stat, not callbacks.
 */

struct ev_win32_dir
{
  struct ev_win32_dir *next;
  HANDLE handle;
  OVERLAPPED overlapped;
  wchar_t *path;
  unsigned int refs;
  int pending, retired, failed;
  DWORD buffer[4096]; /* DWORD aligned; below the network 64 KiB limit */
};

struct ev_win32_watch
{
  struct ev_win32_watch *next;
  struct ev_win32_dir *dir;
  struct ev_win32_dir *children;
  ev_stat *watcher;
  int dirty;
};

struct ev_win32_fs
{
  HANDLE port, thread;
  CRITICAL_SECTION lock;
  ev_async async;
  struct ev_win32_dir *dirs;
  struct ev_win32_watch *watches;
  int stopping;
#if EV_MULTIPLICITY
  struct ev_loop *owner;
#endif
};

static int
win32_dir_read (struct ev_win32_dir *dir)
{
  memset (&dir->overlapped, 0, sizeof (dir->overlapped));
  dir->pending = ReadDirectoryChangesW (dir->handle, dir->buffer, sizeof (dir->buffer), FALSE,
    FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME
    | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE
    | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_LAST_ACCESS
    | FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_SECURITY,
    0, &dir->overlapped, 0) != 0;
  return dir->pending;
}

/* Must hold ctx->lock. A retired directory is freed only after its completion
 * packet has been dequeued: closing/cancelling a HANDLE alone is not enough. */
static void
win32_dir_free (struct ev_win32_fs *ctx, struct ev_win32_dir *dir)
{
  struct ev_win32_dir **link = &ctx->dirs;
  while (*link != dir)
    link = &(*link)->next;
  *link = dir->next;
  free (dir->path);
  free (dir);
}

static void
win32_dir_retire (struct ev_win32_fs *ctx, struct ev_win32_dir *dir)
{
  dir->retired = 1;
  CancelIoEx (dir->handle, &dir->overlapped);
  CloseHandle (dir->handle);
  if (!dir->pending)
    win32_dir_free (ctx, dir);
}

static unsigned int __stdcall
win32_fs_thread (void *arg)
{
  struct ev_win32_fs *ctx = (struct ev_win32_fs *)arg;
  for (;;)
    {
      DWORD bytes;
      ULONG_PTR key;
      OVERLAPPED *overlapped;
      BOOL ok = GetQueuedCompletionStatus (ctx->port, &bytes, &key, &overlapped, INFINITE);
      DWORD error = ok ? ERROR_SUCCESS : GetLastError ();

      EnterCriticalSection (&ctx->lock);
      if (overlapped)
        {
          struct ev_win32_dir *dir = (struct ev_win32_dir *)key;
          dir->pending = 0;
          if (dir->retired)
            win32_dir_free (ctx, dir);
          else
            {
              struct ev_win32_watch *watch;
              /* Re-stat every subscriber, also for zero-byte overflow or a
               * failed handle. This avoids depending on lossy name records. */
              for (watch = ctx->watches; watch; watch = watch->next)
                if (watch->dir == dir || watch->children == dir)
                  watch->dirty = 1;

              if ((!ok && error != ERROR_NOTIFY_ENUM_DIR) || !win32_dir_read (dir))
                dir->failed = 1;

#if EV_MULTIPLICITY
              ev_async_send (ctx->owner, &ctx->async);
#else
              ev_async_send (&ctx->async);
#endif
            }
        }

      if (ctx->stopping && !ctx->dirs)
        {
          LeaveCriticalSection (&ctx->lock);
          return 0;
        }
      LeaveCriticalSection (&ctx->lock);
    }
}

static void
win32_fs_cb (EV_P_ ev_async *async, int revents)
{
  struct ev_win32_fs *ctx = (struct ev_win32_fs *)async->data;
  struct ev_win32_watch *watch, *next;
  EnterCriticalSection (&ctx->lock);
  /* A stat change can replace this registration. The lock prevents the
   * receiver from changing dirty flags, and other subscriptions stay valid. */
  for (watch = ctx->watches; watch; watch = next)
    {
      next = watch->next;
      if (watch->dirty)
        {
          watch->dirty = 0;
          stat_timer_cb (EV_A_ &watch->watcher->timer, EV_STAT);
        }
    }
  LeaveCriticalSection (&ctx->lock);
}

static int
win32_fs_init (EV_P)
{
  struct ev_win32_fs *ctx;
  if (win32_fs)
    return 1;
  if (origflags & EVFLAG_NOINOTIFY)
    return 0;

  ctx = (struct ev_win32_fs *)calloc (1, sizeof (*ctx));
  if (!ctx)
    return 0;
  ctx->port = CreateIoCompletionPort (INVALID_HANDLE_VALUE, 0, 0, 1);
  if (!ctx->port)
    { free (ctx); return 0; }
  InitializeCriticalSection (&ctx->lock);
#if EV_MULTIPLICITY
  ctx->owner = EV_A;
#endif
  ev_async_init (&ctx->async, win32_fs_cb);
  ctx->async.data = ctx;
  ev_async_start (EV_A_ &ctx->async);
  ev_unref (EV_A); /* internal watcher must not keep the loop running */
  ctx->thread = (HANDLE)_beginthreadex (0, 0, win32_fs_thread, ctx, 0, 0);
  if (!ctx->thread)
    {
      ev_ref (EV_A);
      ev_async_stop (EV_A_ &ctx->async);
      DeleteCriticalSection (&ctx->lock);
      CloseHandle (ctx->port);
      free (ctx);
      return 0;
    }
  win32_fs = ctx;
  return 1;
}

/* Snapshot an absolute path at registration. The configured codepage matches
 * the stat wrapper (UTF-8 for Ruby); all HANDLE operations are wide. */
static wchar_t *
win32_fs_path (const char *path, int parent)
{
  wchar_t *input, *absolute, *slash;
  int length = MultiByteToWideChar (EV_WIN32_STAT_CODEPAGE, 0, path, -1, 0, 0);
  DWORD needed, written;
  if (!length)
    return 0;
  input = (wchar_t *)malloc (length * sizeof (wchar_t));
  if (!input)
    return 0;
  MultiByteToWideChar (EV_WIN32_STAT_CODEPAGE, 0, path, -1, input, length);
  needed = GetFullPathNameW (input, 0, 0, 0);
  absolute = needed ? (wchar_t *)malloc (needed * sizeof (wchar_t)) : 0;
  written = absolute ? GetFullPathNameW (input, needed, absolute, 0) : 0;
  if (!written || written >= needed)
    { free (input); free (absolute); return 0; }
  free (input);
  length = (int)wcslen (absolute);
  while (length > 3 && (absolute[length - 1] == L'\\' || absolute[length - 1] == L'/'))
    absolute[--length] = 0;
  if (!parent)
    return absolute;
  slash = wcsrchr (absolute, L'\\');
  if (!slash)
    { free (absolute); return 0; }
  /* Keep the separator of a drive root (C:\\). */
  if (slash == absolute + 2 && absolute[1] == L':')
    slash[1] = 0;
  else
    *slash = 0;
  return absolute;
}

static void
win32_fs_remove (struct ev_win32_fs *ctx, struct ev_win32_watch **link)
{
  struct ev_win32_watch *watch = *link;
  struct ev_win32_dir *dir = watch->dir;
  struct ev_win32_dir *children = watch->children;
  *link = watch->next;
  free (watch);
  if (!--dir->refs)
    win32_dir_retire (ctx, dir);
  if (children && !--children->refs)
    win32_dir_retire (ctx, children);
}

/* A directory HANDLE follows renames. Periodically verify that it still names
 * the directory at the watched path, including a changed current directory. */
static int
win32_dir_matches (struct ev_win32_dir *dir, const wchar_t *parent)
{
  BY_HANDLE_FILE_INFORMATION previous, current;
  HANDLE handle;
  int same;
  if (_wcsicmp (dir->path, parent))
    return 0;
  handle = CreateFileW (parent, FILE_READ_ATTRIBUTES,
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING,
    FILE_FLAG_BACKUP_SEMANTICS, 0);
  if (handle == INVALID_HANDLE_VALUE)
    return 0;
  same = GetFileInformationByHandle (dir->handle, &previous)
    && GetFileInformationByHandle (handle, &current)
    && previous.dwVolumeSerialNumber == current.dwVolumeSerialNumber
    && previous.nFileIndexHigh == current.nFileIndexHigh
    && previous.nFileIndexLow == current.nFileIndexLow;
  CloseHandle (handle);
  return same;
}

/* Consumes path on success or failure. All subscriptions to a directory share
 * one non-recursive read; directory watchers also subscribe to their own path. */
static struct ev_win32_dir *
win32_dir_acquire (struct ev_win32_fs *ctx, wchar_t *path)
{
  struct ev_win32_dir *dir;
  if (!path)
    return 0;
  for (dir = ctx->dirs; dir; dir = dir->next)
    if (!dir->retired && !dir->failed && !_wcsicmp (dir->path, path))
      break;

  if (dir)
    free (path);
  else
    {
      dir = (struct ev_win32_dir *)calloc (1, sizeof (*dir));
      if (!dir)
        goto fail;
      dir->path = path;
      dir->handle = CreateFileW (path, FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, 0);
      if (dir->handle == INVALID_HANDLE_VALUE)
        { free (dir); goto fail; }
      if (!CreateIoCompletionPort (dir->handle, ctx->port, (ULONG_PTR)dir, 0)
          || !win32_dir_read (dir))
        { CloseHandle (dir->handle); free (dir); goto fail; }
      dir->next = ctx->dirs;
      ctx->dirs = dir;
    }

  ++dir->refs;
  return dir;

fail:
  free (path);
  return 0;
}

static void
win32_fs_add (EV_P_ ev_stat *w)
{
  struct ev_win32_fs *ctx;
  struct ev_win32_watch **link, *watch;
  struct ev_win32_dir *dir;
  wchar_t *parent, *children;
  if (!win32_fs_init (EV_A))
    return;
  ctx = win32_fs;
  EnterCriticalSection (&ctx->lock);
  parent = win32_fs_path (w->path, 1);
  children = (w->attr.st_nlink && (w->attr.st_mode & S_IFMT) == S_IFDIR)
    ? win32_fs_path (w->path, 0) : 0;
  if (!parent)
    { free (children); LeaveCriticalSection (&ctx->lock); return; }
  for (link = &ctx->watches; *link; link = &(*link)->next)
    if ((*link)->watcher == w)
      {
        struct ev_win32_watch *old = *link;
        if (!old->dir->failed && win32_dir_matches (old->dir, parent)
            && ((!children && !old->children)
                || (children && old->children && !old->children->failed
                    && win32_dir_matches (old->children, children))))
          {
            free (parent);
            free (children);
            LeaveCriticalSection (&ctx->lock);
            return;
          }
        if (!_wcsicmp (old->dir->path, parent) && !win32_dir_matches (old->dir, parent))
          old->dir->failed = 1;
        if (children && old->children && !win32_dir_matches (old->children, children))
          old->children->failed = 1;
        win32_fs_remove (ctx, link);
        break;
      }

  watch = (struct ev_win32_watch *)calloc (1, sizeof (*watch));
  if (!watch)
    { free (parent); free (children); LeaveCriticalSection (&ctx->lock); return; }
  dir = win32_dir_acquire (ctx, parent);
  if (!dir)
    { free (watch); free (children); LeaveCriticalSection (&ctx->lock); return; }
  watch->dir = dir;
  watch->children = win32_dir_acquire (ctx, children);
  watch->watcher = w;
  watch->next = ctx->watches;
  ctx->watches = watch;
  LeaveCriticalSection (&ctx->lock);
}

static void
win32_fs_del (EV_P_ ev_stat *w)
{
  struct ev_win32_fs *ctx = win32_fs;
  struct ev_win32_watch **link;
  if (!ctx)
    return;
  EnterCriticalSection (&ctx->lock);
  for (link = &ctx->watches; *link; link = &(*link)->next)
    if ((*link)->watcher == w)
      { win32_fs_remove (ctx, link); break; }
  LeaveCriticalSection (&ctx->lock);
}

static void
win32_fs_destroy (EV_P)
{
  struct ev_win32_fs *ctx = win32_fs;
  struct ev_win32_dir *dir, *next;
  if (!ctx)
    return;
  EnterCriticalSection (&ctx->lock);
  ctx->stopping = 1;
  while (ctx->watches)
    win32_fs_remove (ctx, &ctx->watches);
  for (dir = ctx->dirs; dir; dir = next)
    {
      next = dir->next;
      if (!dir->retired)
        win32_dir_retire (ctx, dir);
    }
  LeaveCriticalSection (&ctx->lock);
  PostQueuedCompletionStatus (ctx->port, 0, 0, 0);
  WaitForSingleObject (ctx->thread, INFINITE);
  CloseHandle (ctx->thread);
  CloseHandle (ctx->port);
  ev_ref (EV_A);
  ev_async_stop (EV_A_ &ctx->async);
  DeleteCriticalSection (&ctx->lock);
  free (ctx);
  win32_fs = 0;
}
