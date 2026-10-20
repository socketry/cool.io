/* Windows notification support for ev_stat. Included by ev.c.
 * Copyright (C) 2026. Distributed under the same terms as ev.c.
 *
 * Only the loop thread touches ev_stat or runs callbacks. The IOCP thread
 * rearms directory reads and publishes dirty flags under a critical section.
 * Directory events (including overflow) are hints to re-stat, not callbacks.
 */

struct ev_win32_subscription
{
  struct ev_win32_subscription *next;
  struct ev_win32_dir *dir;
  struct ev_win32_watch *watch;
};

struct ev_win32_dir
{
  struct ev_win32_dir *next;
  HANDLE handle;
  OVERLAPPED overlapped;
  wchar_t *path;
  unsigned int refs;
  int pending, retired, failed, dirty;
  struct ev_win32_subscription *subscribers;
  DWORD buffer[4096]; /* DWORD aligned; below the network 64 KiB limit */
};

struct ev_win32_watch
{
  struct ev_win32_watch *next;
  struct ev_win32_watch *dirty_next;
  struct ev_win32_subscription parent, children;
  wchar_t *path, *shortpath, *longpath;
  const wchar_t *name, *shortname, *longname;
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
  struct ev_win32_watch *dirty;
  int stopping;
#if EV_MULTIPLICITY
  struct ev_loop *owner;
#endif
};

static void win32_fs_add_locked (EV_P_ ev_stat *w);
static int win32_dir_matches (struct ev_win32_dir *dir, const wchar_t *parent);

static void
win32_watch_names (struct ev_win32_watch *watch)
{
  DWORD length = GetShortPathNameW (watch->path, 0, 0);
  free (watch->shortpath);
  watch->shortname = 0;
  watch->shortpath = length ? (wchar_t *)malloc (length * sizeof (wchar_t)) : 0;
  if (watch->shortpath)
    {
      DWORD written = GetShortPathNameW (watch->path, watch->shortpath, length);
      wchar_t *slash = written && written < length ? wcsrchr (watch->shortpath, L'\\') : 0;
      if (slash)
        watch->shortname = slash + 1;
    }
  length = GetLongPathNameW (watch->path, 0, 0);
  free (watch->longpath);
  watch->longname = 0;
  watch->longpath = length ? (wchar_t *)malloc (length * sizeof (wchar_t)) : 0;
  if (watch->longpath)
    {
      DWORD written = GetLongPathNameW (watch->path, watch->longpath, length);
      wchar_t *slash = written && written < length ? wcsrchr (watch->longpath, L'\\') : 0;
      if (slash)
        watch->longname = slash + 1;
    }
}

/* Called with ctx->lock held. Queue each watcher at most once per batch. */
static void
win32_watch_dirty (struct ev_win32_fs *ctx, struct ev_win32_watch *watch)
{
  if (!watch->dirty)
    {
      watch->dirty = 1;
      watch->dirty_next = ctx->dirty;
      ctx->dirty = watch;
    }
}

static int
win32_name_matches (const wchar_t *name, const FILE_NOTIFY_INFORMATION *event)
{
  int length = event->FileNameLength / sizeof (wchar_t);
  return name && wcslen (name) == (size_t)length
    && CompareStringOrdinal (name, length, event->FileName, length, TRUE) == CSTR_EQUAL;
}

/* Name records only select paths to re-stat. Missing or malformed records
 * (including buffer overflow) invalidate every subscriber to this directory. */
static void
win32_dir_notify (struct ev_win32_fs *ctx, struct ev_win32_dir *dir, DWORD bytes)
{
  struct ev_win32_subscription *sub;
  DWORD offset = 0;
  dir->dirty = 1;
  while (bytes && offset < bytes)
    {
      FILE_NOTIFY_INFORMATION *event = (FILE_NOTIFY_INFORMATION *)((char *)dir->buffer + offset);
      DWORD remaining = bytes - offset;
      DWORD header = offsetof (FILE_NOTIFY_INFORMATION, FileName);
      if (remaining < header || event->FileNameLength % sizeof (wchar_t)
          || event->FileNameLength > remaining - header)
        break;
      for (sub = dir->subscribers; sub; sub = sub->next)
        if (sub == &sub->watch->children
            || win32_name_matches (sub->watch->name, event)
            || win32_name_matches (sub->watch->shortname, event)
            || win32_name_matches (sub->watch->longname, event))
          win32_watch_dirty (ctx, sub->watch);
      if (!event->NextEntryOffset)
        return;
      if (event->NextEntryOffset < header + event->FileNameLength
          || event->NextEntryOffset >= remaining || event->NextEntryOffset % sizeof (DWORD))
        break;
      offset += event->NextEntryOffset;
    }
  for (sub = dir->subscribers; sub; sub = sub->next)
    win32_watch_dirty (ctx, sub->watch);
}

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
              win32_dir_notify (ctx, dir, ok && bytes <= sizeof (dir->buffer) ? bytes : 0);

              if ((!ok && error != ERROR_NOTIFY_ENUM_DIR) || !win32_dir_read (dir))
                {
                  dir->failed = 1;
                  win32_dir_notify (ctx, dir, 0);
                }

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
  struct ev_win32_dir *dir;
  struct ev_win32_watch *watch;
  struct win32_stat_batch
    {
      ev_stat *watcher;
      int refresh;
    } *batch;
  size_t count = 0, i = 0;

  EnterCriticalSection (&ctx->lock);
  /* Check identity once per directory per batch, rather than once for every
   * subscriber. A HANDLE can still refer to a directory moved elsewhere. */
  for (dir = ctx->dirs; dir; dir = dir->next)
    if (dir->dirty && !dir->retired)
      {
        dir->dirty = 0;
        if (dir->failed || !win32_dir_matches (dir, dir->path))
          {
            dir->failed = 1;
            win32_dir_notify (ctx, dir, 0);
          }
      }
  for (watch = ctx->dirty; watch; watch = watch->dirty_next)
    ++count;
  batch = count ? (struct win32_stat_batch *)malloc (count * sizeof (*batch)) : 0;
  if (batch)
    while ((watch = ctx->dirty))
      {
        ctx->dirty = watch->dirty_next;
        watch->dirty = 0;
        batch[i].watcher = watch->watcher;
        batch[i++].refresh = watch->parent.dir->failed
          || (watch->children.dir && watch->children.dir->failed);
      }
  LeaveCriticalSection (&ctx->lock);

  /* stat_timer_cb only queues user callbacks, so these ev_stat pointers stay
   * valid for this batch. Notifications arriving meanwhile form the next one. */
  if (batch)
    for (i = 0; i < count; ++i)
      {
        if (batch[i].refresh)
          {
            EnterCriticalSection (&ctx->lock);
            win32_fs_add_locked (EV_A_ batch[i].watcher);
            LeaveCriticalSection (&ctx->lock);
          }
        stat_timer_cb (EV_A_ &batch[i].watcher->timer, EV_STAT);
      }
  free (batch);
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

/* Include the separator in drive, UNC share and extended-length roots. */
static size_t
win32_fs_root (const wchar_t *path)
{
  const wchar_t *start = path, *slash;
  if (!wcsncmp (start, L"\\\\?\\", 4))
    {
      start += 4;
      if (!_wcsnicmp (start, L"UNC\\", 4))
        start += 4;
      else
        {
          slash = wcschr (start, L'\\');
          return slash ? slash - path + 1 : wcslen (path);
        }
    }
  else if (start[0] == L'\\' && start[1] == L'\\')
    start += 2;
  else
    return start[0] && start[1] == L':' && start[2] == L'\\' ? 3 : 0;

  slash = wcschr (start, L'\\');
  if (slash)
    slash = wcschr (slash + 1, L'\\');
  return slash ? slash - path + 1 : wcslen (path);
}

/* Snapshot an absolute path at registration. The configured codepage matches
 * the stat wrapper (UTF-8 for Ruby); all HANDLE operations are wide. */
static wchar_t *
win32_fs_path (const char *path, int parent)
{
  wchar_t *input, *absolute, *slash;
  int length = MultiByteToWideChar (EV_WIN32_STAT_CODEPAGE, 0, path, -1, 0, 0);
  DWORD needed, written;
  size_t root;
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
  root = win32_fs_root (absolute);
  length = (int)wcslen (absolute);
  while ((size_t)length > root && (absolute[length - 1] == L'\\' || absolute[length - 1] == L'/'))
    absolute[--length] = 0;
  if (!parent || (size_t)length <= root)
    return absolute;
  slash = wcsrchr (absolute, L'\\');
  if (!slash)
    { free (absolute); return 0; }
  if ((size_t)(slash - absolute) < root)
    absolute[root] = 0;
  else
    *slash = 0;
  return absolute;
}

static void
win32_subscription_add (struct ev_win32_subscription *sub, struct ev_win32_watch *watch,
                        struct ev_win32_dir *dir)
{
  sub->dir = dir;
  sub->watch = watch;
  if (dir)
    {
      sub->next = dir->subscribers;
      dir->subscribers = sub;
    }
}

static void
win32_subscription_del (struct ev_win32_fs *ctx, struct ev_win32_subscription *sub)
{
  struct ev_win32_dir *dir = sub->dir;
  struct ev_win32_subscription **link;
  if (!dir)
    return;
  for (link = &dir->subscribers; *link != sub; link = &(*link)->next)
    ;
  *link = sub->next;
  if (!--dir->refs)
    win32_dir_retire (ctx, dir);
}

static void
win32_fs_remove (struct ev_win32_fs *ctx, struct ev_win32_watch **link)
{
  struct ev_win32_watch *watch = *link;
  *link = watch->next;
  if (watch->dirty)
    {
      struct ev_win32_watch **dirty;
      for (dirty = &ctx->dirty; *dirty != watch; dirty = &(*dirty)->dirty_next)
        ;
      *dirty = watch->dirty_next;
    }
  win32_subscription_del (ctx, &watch->parent);
  win32_subscription_del (ctx, &watch->children);
  free (watch->path);
  free (watch->shortpath);
  free (watch->longpath);
  free (watch);
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
      {
        if (win32_dir_matches (dir, path))
          break;
        /* An open HANDLE follows a renamed directory, even after another
         * directory has been created at the same path. */
        dir->failed = 1;
      }

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
win32_fs_add_locked (EV_P_ ev_stat *w)
{
  struct ev_win32_fs *ctx;
  struct ev_win32_watch **link, *watch;
  struct ev_win32_dir *dir;
  wchar_t *parent, *children, *path;
  if (!win32_fs_init (EV_A))
    return;
  ctx = win32_fs;
  parent = win32_fs_path (w->path, 1);
  children = (w->attr.st_nlink && (w->attr.st_mode & S_IFMT) == S_IFDIR)
    ? win32_fs_path (w->path, 0) : 0;
  if (!parent)
    { free (children); return; }
  for (link = &ctx->watches; *link; link = &(*link)->next)
    if ((*link)->watcher == w)
      {
        struct ev_win32_watch *old = *link;
        int same_parent = !old->parent.dir->failed && win32_dir_matches (old->parent.dir, parent);
        int same_children = !children && !old->children.dir;
        if (children && old->children.dir)
          same_children = !old->children.dir->failed && win32_dir_matches (old->children.dir, children);
        if (same_parent && same_children)
          {
            win32_watch_names (old);
            free (parent);
            free (children);
            return;
          }
        if (!same_parent && !_wcsicmp (old->parent.dir->path, parent))
          old->parent.dir->failed = 1;
        if (!same_children && children && old->children.dir)
          old->children.dir->failed = 1;
        break;
      }

  path = win32_fs_path (w->path, 0);
  watch = path ? (struct ev_win32_watch *)calloc (1, sizeof (*watch)) : 0;
  if (!watch)
    { free (parent); free (children); free (path); return; }
  dir = win32_dir_acquire (ctx, parent);
  if (!dir)
    { free (watch); free (children); free (path); return; }
  watch->path = path;
  watch->name = wcsrchr (path, L'\\') + 1;
  win32_watch_names (watch);
  win32_subscription_add (&watch->parent, watch, dir);
  win32_subscription_add (&watch->children, watch, win32_dir_acquire (ctx, children));
  /* Acquire the replacement first, retaining unchanged shared directory reads. */
  if (*link)
    win32_fs_remove (ctx, link);
  watch->watcher = w;
  watch->next = ctx->watches;
  ctx->watches = watch;
}

static void
win32_fs_add (EV_P_ ev_stat *w)
{
  struct ev_win32_fs *ctx;
  if (!win32_fs_init (EV_A))
    return;
  ctx = win32_fs;
  EnterCriticalSection (&ctx->lock);
  win32_fs_add_locked (EV_A_ w);
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
