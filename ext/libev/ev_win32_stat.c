/* Windows notification support for ev_stat. Included by ev.c.
 * Copyright (C) 2026. Distributed under the same terms as ev.c.
 *
 * Only the loop thread touches ev_stat or runs callbacks. The IOCP thread
 * rearms directory reads and publishes dirty flags under a critical section.
 * Directory events (including overflow) are hints to re-stat, not callbacks.
 */

struct ev_win32_subscription
{
  struct ev_win32_subscription *next, **prev;
  struct ev_win32_dir *dir;
  struct ev_win32_watch *watch;
};

struct ev_win32_name
{
  struct ev_win32_name *next;
  struct ev_win32_watch *watch;
  wchar_t *key;
  size_t length;
  unsigned int hash;
};

struct ev_win32_dir
{
  struct ev_win32_dir *next, **prev, *hash_next;
  HANDLE handle;
  OVERLAPPED overlapped;
  wchar_t *path;
  unsigned int refs, hash, namecnt, namemask;
  ev_tstamp checked_at;
  int pending, retired, failed, dirty;
  struct ev_win32_subscription *files, *children;
  struct ev_win32_name **names;
  DWORD buffer[4096]; /* DWORD aligned; below the network 64 KiB limit */
};

struct ev_win32_watch
{
  struct ev_win32_watch *dirty_next, **dirty_prev;
  struct ev_win32_subscription parent, children;
  struct ev_win32_name keys[3];
  wchar_t *path, *shortpath, *longpath;
  const wchar_t *name, *shortname, *longname;
  ev_stat *watcher;
  int dirty, names_dirty, names_failed;
};

struct ev_win32_fs
{
  HANDLE port, thread;
  CRITICAL_SECTION lock;
  ev_async async;
  struct ev_win32_dir *dirs, **dirtab;
  unsigned int dircnt, dirmask;
  struct ev_win32_watch **watches;
  int watchcnt, watchmax;
  struct ev_win32_watch *dirty;
  int stopping;
#if EV_MULTIPLICITY
  struct ev_loop *owner;
#endif
};

static void win32_fs_add_locked (EV_P_ ev_stat *w, int polling);
static int win32_dir_matches (struct ev_win32_dir *dir, const wchar_t *parent);

static unsigned int
win32_hash (const wchar_t *key, size_t length)
{
  unsigned int hash = 2166136261u;
  size_t i;
  for (i = 0; i < length; ++i)
    hash = (hash ^ (key[i] >= L'a' && key[i] <= L'z' ? key[i] - (L'a' - L'A') : key[i])) * 16777619u;
  return hash;
}

static int
win32_fold (const wchar_t *name, size_t length, wchar_t *key)
{
  return length && LCMapStringEx (LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE, name, (int)length,
                                  key, (int)length, 0, 0, 0) == (int)length;
}

static void
win32_name_link (struct ev_win32_dir *dir, struct ev_win32_name *node)
{
  struct ev_win32_name **bucket = &dir->names[node->hash & dir->namemask];
  node->next = *bucket;
  *bucket = node;
  if (++dir->namecnt > dir->namemask + 1)
    {
      unsigned int mask = dir->namemask * 2 + 1, i;
      struct ev_win32_name **names = (struct ev_win32_name **)calloc (mask + 1, sizeof (*names));
      if (!names)
        return;
      for (i = 0; i <= dir->namemask; ++i)
        while ((node = dir->names[i]))
          {
            dir->names[i] = node->next;
            node->next = names[node->hash & mask];
            names[node->hash & mask] = node;
          }
      free (dir->names);
      dir->names = names;
      dir->namemask = mask;
    }
}

static void
win32_name_unlink (struct ev_win32_dir *dir, struct ev_win32_name *node)
{
  struct ev_win32_name **link = &dir->names[node->hash & dir->namemask];
  while (*link != node)
    link = &(*link)->next;
  *link = node->next;
  --dir->namecnt;
}

static int
win32_name_key (struct ev_win32_watch *watch, int index, const wchar_t *name)
{
  struct ev_win32_name *node = &watch->keys[index];
  int i;
  node->watch = watch;
  node->length = name ? wcslen (name) : 0;
  node->key = node->length ? (wchar_t *)malloc (node->length * sizeof (wchar_t)) : 0;
  if (!node->key)
    return !node->length;
  if (!win32_fold (name, node->length, node->key))
    { free (node->key); node->key = 0; return 0; }
  node->hash = win32_hash (node->key, node->length);
  for (i = 0; i < index; ++i)
    if (watch->keys[i].key && watch->keys[i].length == node->length
        && !memcmp (watch->keys[i].key, node->key, node->length * sizeof (wchar_t)))
      {
        free (node->key);
        node->key = 0;
        break;
      }
  return 1;
}

static void
win32_watch_names (struct ev_win32_watch *watch)
{
  struct ev_win32_dir *dir = watch->parent.dir;
  DWORD length;
  int i, keyed;
  for (i = 1; i < 3; ++i)
    if (watch->keys[i].key)
      {
        if (dir)
          win32_name_unlink (dir, &watch->keys[i]);
        free (watch->keys[i].key);
        watch->keys[i].key = 0;
      }
  length = GetShortPathNameW (watch->path, 0, 0);
  watch->names_dirty = 0;
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
  keyed = win32_name_key (watch, 1, watch->shortname);
  keyed &= win32_name_key (watch, 2, watch->longname);
  if (dir)
    for (i = 1; i < 3; ++i)
      if (watch->keys[i].key)
        win32_name_link (dir, &watch->keys[i]);
  watch->names_failed = !watch->shortname || !watch->longname || !keyed;
}

/* Called with ctx->lock held. Queue each watcher at most once per batch. */
static void
win32_watch_dirty (struct ev_win32_fs *ctx, struct ev_win32_watch *watch)
{
  if (!watch->dirty)
    {
      watch->dirty = 1;
      watch->dirty_next = ctx->dirty;
      watch->dirty_prev = &ctx->dirty;
      if (ctx->dirty)
        ctx->dirty->dirty_prev = &watch->dirty_next;
      ctx->dirty = watch;
    }
}

/* Name records only select paths to re-stat. Missing or malformed records
 * (including buffer overflow) invalidate every subscriber to this directory. */
static void
win32_dir_notify (struct ev_win32_fs *ctx, struct ev_win32_dir *dir, DWORD bytes)
{
  struct ev_win32_subscription *sub;
  struct ev_win32_name *node;
  wchar_t key[MAX_PATH];
  DWORD offset = 0;
  dir->dirty = 1;
  for (sub = dir->children; sub; sub = sub->next)
    win32_watch_dirty (ctx, sub->watch);
  while (bytes && offset < bytes)
    {
      FILE_NOTIFY_INFORMATION *event = (FILE_NOTIFY_INFORMATION *)((char *)dir->buffer + offset);
      DWORD remaining = bytes - offset;
      DWORD header = offsetof (FILE_NOTIFY_INFORMATION, FileName);
      size_t length;
      unsigned int hash;
      if (remaining < header || event->FileNameLength % sizeof (wchar_t)
          || event->FileNameLength > remaining - header)
        break;
      length = event->FileNameLength / sizeof (wchar_t);
      if (length && dir->namecnt)
        {
          if (length > MAX_PATH || !win32_fold (event->FileName, length, key))
            break;
          hash = win32_hash (key, length);
          for (node = dir->names[hash & dir->namemask]; node; node = node->next)
            if (node->hash == hash && node->length == length
                && !memcmp (node->key, key, length * sizeof (wchar_t)))
              {
                /* Replacement can change aliases even with equal stat data. */
                if (event->Action != FILE_ACTION_MODIFIED)
                  {
                    node->watch->names_dirty = 1;
                    if (node->watch->children.dir)
                      node->watch->children.dir->dirty = 1;
                  }
                win32_watch_dirty (ctx, node->watch);
              }
        }
      if (!event->NextEntryOffset)
        return;
      if (event->NextEntryOffset < header + event->FileNameLength
          || event->NextEntryOffset >= remaining || event->NextEntryOffset % sizeof (DWORD))
        break;
      offset += event->NextEntryOffset;
    }
  for (sub = dir->files; sub; sub = sub->next)
    {
      sub->watch->names_dirty = 1; /* Overflow may have hidden a replacement. */
      if (sub->watch->children.dir)
        sub->watch->children.dir->dirty = 1;
      win32_watch_dirty (ctx, sub->watch);
    }
  for (sub = dir->children; sub; sub = sub->next)
    sub->watch->names_dirty = 1;
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
  struct ev_win32_dir **link = &ctx->dirtab[dir->hash & ctx->dirmask];
  while (*link != dir)
    link = &(*link)->hash_next;
  *link = dir->hash_next;
  --ctx->dircnt;
  *dir->prev = dir->next;
  if (dir->next)
    dir->next->prev = dir->prev;
  free (dir->names);
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
          CloseHandle (ctx->thread);
          CloseHandle (ctx->port);
          DeleteCriticalSection (&ctx->lock);
          free (ctx->dirtab);
          free (ctx);
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
  int refresh;

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
        if (ctx->dirty)
          ctx->dirty->dirty_prev = &ctx->dirty;
        watch->dirty = 0;
        refresh = watch->parent.dir->failed
          || (watch->children.dir && watch->children.dir->failed)
          || (watch->names_dirty && !watch->children.dir && watch->watcher->attr.st_nlink
              && (watch->watcher->attr.st_mode & S_IFMT) == S_IFDIR);
        if (watch->names_dirty && !refresh)
          win32_watch_names (watch);
        batch[i].watcher = watch->watcher;
        batch[i++].refresh = refresh;
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
            win32_fs_add_locked (EV_A_ batch[i].watcher, 0);
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
  if (win32_fs_disabled)
    return 0;

  ctx = (struct ev_win32_fs *)calloc (1, sizeof (*ctx));
  if (!ctx)
    return 0;
  ctx->port = CreateIoCompletionPort (INVALID_HANDLE_VALUE, 0, 0, 1);
  if (!ctx->port)
    { free (ctx); return 0; }
  /* Prepare the same pipe ev_async_start will use, with a recoverable failure
   * path for optional notifications. Existing async/signal callers still retry. */
  if (!evpipe_init_try (EV_A))
    { CloseHandle (ctx->port); free (ctx); return 0; }
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
  struct ev_win32_subscription **head;
  int i;
  sub->dir = dir;
  sub->watch = watch;
  if (!dir)
    return;
  head = sub == &watch->children ? &dir->children : &dir->files;
  sub->next = *head;
  sub->prev = head;
  if (*head)
    (*head)->prev = &sub->next;
  *head = sub;
  if (sub == &watch->parent)
    for (i = 0; i < 3; ++i)
      if (watch->keys[i].key)
        win32_name_link (dir, &watch->keys[i]);
}

static void
win32_subscription_del (struct ev_win32_fs *ctx, struct ev_win32_subscription *sub)
{
  struct ev_win32_dir *dir = sub->dir;
  int i;
  if (!dir)
    return;
  if (sub == &sub->watch->parent)
    for (i = 0; i < 3; ++i)
      if (sub->watch->keys[i].key)
        win32_name_unlink (dir, &sub->watch->keys[i]);
  *sub->prev = sub->next;
  if (sub->next)
    sub->next->prev = sub->prev;
  if (!--dir->refs)
    win32_dir_retire (ctx, dir);
}

static struct ev_win32_watch *
win32_fs_watch (struct ev_win32_fs *ctx, ev_stat *w)
{
  if (w->wd >= 0 && w->wd < ctx->watchcnt && ctx->watches[w->wd]->watcher == w)
    return ctx->watches[w->wd];
  return 0;
}

static void
win32_watch_free (struct ev_win32_fs *ctx, struct ev_win32_watch *watch)
{
  int i;
  if (watch->dirty)
    {
      *watch->dirty_prev = watch->dirty_next;
      if (watch->dirty_next)
        watch->dirty_next->dirty_prev = watch->dirty_prev;
    }
  win32_subscription_del (ctx, &watch->parent);
  win32_subscription_del (ctx, &watch->children);
  for (i = 0; i < 3; ++i)
    free (watch->keys[i].key);
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

static void
win32_dirtab_grow (struct ev_win32_fs *ctx)
{
  unsigned int mask = ctx->dirmask * 2 + 1, i;
  struct ev_win32_dir *dir, **table = (struct ev_win32_dir **)calloc (mask + 1, sizeof (*table));
  if (!table)
    return;
  for (i = 0; i <= ctx->dirmask; ++i)
    while ((dir = ctx->dirtab[i]))
      {
        ctx->dirtab[i] = dir->hash_next;
        dir->hash_next = table[dir->hash & mask];
        table[dir->hash & mask] = dir;
      }
  free (ctx->dirtab);
  ctx->dirtab = table;
  ctx->dirmask = mask;
}

/* Consumes path on success or failure. All subscriptions to a directory share
 * one non-recursive read; directory watchers also subscribe to their own path. */
static struct ev_win32_dir *
win32_dir_acquire (struct ev_win32_fs *ctx, wchar_t *path)
{
  struct ev_win32_dir *dir;
  unsigned int hash;
  if (!path)
    return 0;
  if (!ctx->dirtab)
    {
      ctx->dirtab = (struct ev_win32_dir **)calloc (16, sizeof (*ctx->dirtab));
      if (!ctx->dirtab)
        goto fail;
      ctx->dirmask = 15;
    }
  hash = win32_hash (path, wcslen (path));
  for (dir = ctx->dirtab[hash & ctx->dirmask]; dir; dir = dir->hash_next)
    if (dir->hash == hash && !dir->retired && !dir->failed && !_wcsicmp (dir->path, path))
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
      if (dir)
        dir->names = (struct ev_win32_name **)calloc (16, sizeof (*dir->names));
      if (!dir || !dir->names)
        { free (dir); goto fail; }
      dir->path = path;
      dir->hash = hash;
      dir->namemask = 15;
      dir->handle = CreateFileW (path, FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, 0);
      if (dir->handle == INVALID_HANDLE_VALUE)
        { free (dir->names); free (dir); goto fail; }
      if (!CreateIoCompletionPort (dir->handle, ctx->port, (ULONG_PTR)dir, 0)
          || !win32_dir_read (dir))
        { CloseHandle (dir->handle); free (dir->names); free (dir); goto fail; }
      dir->next = ctx->dirs;
      dir->prev = &ctx->dirs;
      if (ctx->dirs)
        ctx->dirs->prev = &dir->next;
      ctx->dirs = dir;
      dir->hash_next = ctx->dirtab[hash & ctx->dirmask];
      ctx->dirtab[hash & ctx->dirmask] = dir;
      if (++ctx->dircnt > ctx->dirmask + 1)
        win32_dirtab_grow (ctx);
    }

  ++dir->refs;
  return dir;

fail:
  free (path);
  return 0;
}

static void
win32_fs_add_locked (EV_P_ ev_stat *w, int polling)
{
  struct ev_win32_fs *ctx;
  struct ev_win32_watch *old, *watch;
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
  old = win32_fs_watch (ctx, w);
  if (old)
    {
      int same_parent;
      int same_children = !children && !old->children.dir;
      /* Timer callbacks in one loop turn share the same timestamp. Validate
       * shared handles once per turn; new registrations still check afresh. */
      if (polling)
        {
          struct ev_win32_dir *dirs[2];
          int i;
          dirs[0] = old->parent.dir;
          dirs[1] = old->children.dir;
          for (i = 0; i < 2; ++i)
            if (dirs[i] && !dirs[i]->failed && dirs[i]->checked_at != ev_rt_now)
              {
                dirs[i]->checked_at = ev_rt_now;
                if (!win32_dir_matches (dirs[i], dirs[i]->path))
                  dirs[i]->failed = 1;
              }
        }
      same_parent = !old->parent.dir->failed
        && (polling ? !_wcsicmp (old->parent.dir->path, parent)
                    : win32_dir_matches (old->parent.dir, parent));
      if (children && old->children.dir)
        same_children = !old->children.dir->failed
          && (polling ? !_wcsicmp (old->children.dir->path, children)
                      : win32_dir_matches (old->children.dir, children));
      if (same_parent && same_children)
        {
          /* Appends leave aliases unchanged. Retry failures during polling,
           * and refresh after name events or an explicit re-registration. */
          if (!polling || old->names_dirty || old->names_failed)
            win32_watch_names (old);
          free (parent);
          free (children);
          return;
        }
      if (!same_parent && !_wcsicmp (old->parent.dir->path, parent))
        old->parent.dir->failed = 1;
      if (!same_children && children && old->children.dir
          && !_wcsicmp (old->children.dir->path, children))
        old->children.dir->failed = 1;
    }
  else if (ctx->watchcnt == ctx->watchmax)
    {
      int max = ctx->watchmax ? ctx->watchmax * 2 : 16;
      struct ev_win32_watch **watches
        = (struct ev_win32_watch **)realloc (ctx->watches, max * sizeof (*watches));
      if (!watches)
        { free (parent); free (children); return; }
      ctx->watches = watches;
      ctx->watchmax = max;
    }

  path = win32_fs_path (w->path, 0);
  watch = path ? (struct ev_win32_watch *)calloc (1, sizeof (*watch)) : 0;
  if (watch && !win32_name_key (watch, 0, wcsrchr (path, L'\\') + 1))
    { free (watch); watch = 0; }
  if (!watch)
    { free (parent); free (children); free (path); return; }
  dir = win32_dir_acquire (ctx, parent);
  if (!dir)
    { free (watch->keys[0].key); free (watch); free (children); free (path); return; }
  watch->path = path;
  watch->name = wcsrchr (path, L'\\') + 1;
  win32_watch_names (watch);
  win32_subscription_add (&watch->parent, watch, dir);
  win32_subscription_add (&watch->children, watch, win32_dir_acquire (ctx, children));
  watch->watcher = w;
  /* Acquire the replacement first, retaining unchanged shared directory reads. */
  if (old)
    {
      ctx->watches[w->wd] = watch;
      win32_watch_free (ctx, old);
    }
  else
    {
      w->wd = ctx->watchcnt;
      ctx->watches[ctx->watchcnt++] = watch;
    }
}

static void
win32_fs_add (EV_P_ ev_stat *w, int polling)
{
  struct ev_win32_fs *ctx;
  if (!win32_fs_init (EV_A))
    return;
  ctx = win32_fs;
  EnterCriticalSection (&ctx->lock);
  win32_fs_add_locked (EV_A_ w, polling);
  LeaveCriticalSection (&ctx->lock);
}

static void
win32_fs_del (EV_P_ ev_stat *w)
{
  struct ev_win32_fs *ctx = win32_fs;
  struct ev_win32_watch *watch;
  if (!ctx)
    return;
  EnterCriticalSection (&ctx->lock);
  watch = win32_fs_watch (ctx, w);
  if (watch)
    {
      struct ev_win32_watch *last = ctx->watches[--ctx->watchcnt];
      ctx->watches[w->wd] = last;
      last->watcher->wd = w->wd;
      w->wd = -1;
      win32_watch_free (ctx, watch);
    }
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
  while (ctx->watchcnt)
    win32_watch_free (ctx, ctx->watches[--ctx->watchcnt]);
  free (ctx->watches);
  ctx->watches = 0;
  for (dir = ctx->dirs; dir; dir = next)
    {
      next = dir->next;
      if (!dir->retired)
        win32_dir_retire (ctx, dir);
    }
  ev_ref (EV_A);
  ev_async_stop (EV_A_ &ctx->async);
  win32_fs = 0;
  /* The worker owns ctx after this handoff. Pending cancellation completions
   * still reference directory buffers, so retain them until they arrive, but
   * do not hold up loop destruction (which can run during Ruby GC). All
   * subscriptions are gone and the worker can no longer signal this loop.
   * A driver that never completes cancellation retains these resources. */
  PostQueuedCompletionStatus (ctx->port, 0, 0, 0);
  LeaveCriticalSection (&ctx->lock);
}
