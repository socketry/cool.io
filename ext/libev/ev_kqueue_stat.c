/* kqueue (EVFILT_VNODE) notification support for ev_stat. Included by ev.c.
 * Copyright (C) 2026. Distributed under the same terms as ev.c.
 *
 * Each loop has a private kqueue; each registration has an O_EVTONLY fd.
 * Events carry the ev_stat pointer as udata and trigger a stat comparison.
 * The ordinary polling timer remains enabled for path changes that vnode
 * notifications miss. Registration state belongs to the event loop.
 */

#include <sys/stat.h>
#include <sys/event.h>

struct ev_kq_stat_slot
{
  ev_stat *watcher;
  int fd;     /* descriptor registered in ctx->fd; -1 while polling only */
  int is_dir; /* fd names a directory used as a creation/replacement hint */
};

struct ev_kq_stat
{
  int fd;     /* the private kqueue collecting vnode events */
  pid_t owner_pid;
  ev_io io;   /* loop watcher making ctx->fd's events observable */
  struct ev_kq_stat_slot **slots;
  int slotcnt, slotmax;
};

/* Process one bounded batch per callback so other watchers can run. */
#define EV_KQ_EVENT_BUFSIZE 32

static void kq_stat_add (EV_P_ ev_stat *w);

static unsigned int
kq_stat_file_flags (void)
{
  return NOTE_WRITE | NOTE_EXTEND | NOTE_ATTRIB | NOTE_LINK | NOTE_RENAME | NOTE_DELETE;
}

static unsigned int
kq_stat_dir_flags (void)
{
  /* NOTE_WRITE is the hint for directory entry changes; DELETE/RENAME
   * indicate that the hint directory itself may have moved or disappeared. */
  return NOTE_WRITE | NOTE_ATTRIB | NOTE_RENAME | NOTE_DELETE;
}

/* ctx->io watches the private kqueue for readability when events are pending. */
static int
kq_stat_register (struct ev_kq_stat *ctx, struct ev_kq_stat_slot *slot, int fd, int is_dir, unsigned int nflags)
{
  struct kevent kev;

  EV_SET (&kev, fd, EVFILT_VNODE, EV_ADD | EV_ENABLE | EV_CLEAR, nflags, 0, slot->watcher);
  if (kevent (ctx->fd, &kev, 1, 0, 0, 0) < 0)
    return 0;
  slot->fd = fd;
  slot->is_dir = is_dir;
  return 1;
}

/* Closing the descriptor removes its kernel registration. Events already
 * copied into a callback batch are unaffected. */
static void
kq_stat_close_slot (struct ev_kq_stat *ctx, struct ev_kq_stat_slot *slot)
{
  (void)ctx;
  if (slot->fd >= 0)
    close (slot->fd);
  slot->fd = -1;
  slot->is_dir = 0;
}

/* Reuse a watcher's slot across registration retries; w->wd is its index. */
static struct ev_kq_stat_slot *
kq_stat_slot_get (EV_P_ ev_stat *w)
{
  struct ev_kq_stat *ctx = kq_stat;
  struct ev_kq_stat_slot *slot;

  if (w->wd >= 0)
    return ctx->slots [w->wd];

  if (ctx->slotcnt == ctx->slotmax)
    {
      int max = ctx->slotmax ? ctx->slotmax * 2 : 16;
      struct ev_kq_stat_slot **slots
        = (struct ev_kq_stat_slot **)realloc (ctx->slots, max * sizeof (*slots));
      if (!slots)
        return 0;
      ctx->slots = slots;
      ctx->slotmax = max;
    }
  slot = (struct ev_kq_stat_slot *)calloc (1, sizeof (*slot));
  if (!slot)
    return 0;
  slot->watcher = w;
  slot->fd = -1;
  w->wd = ctx->slotcnt;
  ctx->slots [ctx->slotcnt++] = slot;
  return slot;
}

/* The index may be unset after an allocation failure or watcher stop. */
static struct ev_kq_stat_slot *
kq_stat_slot_valid (struct ev_kq_stat *ctx, ev_stat *w)
{
  return w->wd >= 0 && w->wd < ctx->slotcnt && ctx->slots [w->wd]->watcher == w
    ? ctx->slots [w->wd] : 0;
}

/* Register notifications without changing the polling timer. O_NOFOLLOW
 * avoids watching a symlink's target; parent directories can provide
 * creation hints when the path cannot be opened. Reopen the path on retries
 * to detect replacement, retaining the current registration if it still
 * refers to the same vnode. */
static void
kq_stat_add (EV_P_ ev_stat *w)
{
  struct ev_kq_stat *ctx = kq_stat;
  struct ev_kq_stat_slot *slot;
  int fd = -1, is_dir = 0;
  unsigned int flags = 0;

  slot = kq_stat_slot_get (EV_A_ w);
  if (!slot)
    return;

  fd = open (w->path, O_RDONLY | O_EVTONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC);
  if (fd >= 0)
    {
      struct stat st;
      if (fstat (fd, &st) == 0 && (S_ISREG (st.st_mode) || S_ISDIR (st.st_mode)))
        flags = kq_stat_file_flags ();
      else
        close (fd), fd = -1;
    }
  else if ((errno == ENOENT || errno == ENOTDIR || errno == ELOOP || errno == EACCES)
           && strlen (w->path) < 4096)
    {
      /* Parent hints only: they select paths to re-stat, so following a
       * symlinked directory here cannot change what the stat reports. */
      char path [4096];
      strcpy (path, w->path);
      do
        {
          char *pend = strrchr (path, '/');
          if (!pend || pend == path)
            break;
          *pend = 0;
          fd = open (path, O_RDONLY | O_EVTONLY | O_NONBLOCK | O_CLOEXEC);
        }
      while (fd < 0 && (errno == ENOENT || errno == ENOTDIR || errno == ELOOP || errno == EACCES));

      if (fd >= 0)
        {
          struct stat st;
          if (fstat (fd, &st) == 0 && S_ISDIR (st.st_mode))
            is_dir = 1, flags = kq_stat_dir_flags ();
          else
            close (fd), fd = -1;
        }
    }

  if (fd < 0)
    {
      /* Keep polling when no descriptor can be opened. */
      kq_stat_close_slot (ctx, slot);
      return;
    }

  if (slot->fd >= 0 && slot->is_dir == is_dir)
    {
      struct stat o, n;
      if (fstat (slot->fd, &o) == 0 && fstat (fd, &n) == 0
          && o.st_dev == n.st_dev && o.st_ino == n.st_ino)
        {
          close (fd); /* same target as before: keep the existing kevent */
          return;
        }
    }

  kq_stat_close_slot (ctx, slot);
  if (!kq_stat_register (ctx, slot, fd, is_dir, flags))
    close (fd);
}

static void
kq_stat_cb (EV_P_ ev_io *w_, int revents)
{
  struct ev_kq_stat *ctx = kq_stat;
  struct kevent evs [EV_KQ_EVENT_BUFSIZE];
  struct timespec ts;
  int n, i;

  (void)w_;
  if (revents & EV_ERROR)
    {
      /* fd_kill stops the unreferenced I/O watcher before feeding EV_ERROR. */
      ev_ref (EV_A);
      ev_io_stop (EV_A_ &ctx->io);
      if (ctx->fd >= 0 && ctx->owner_pid == getpid () && fd_valid (ctx->fd))
        close (ctx->fd);
      ctx->fd = -1;
      for (i = 0; i < ctx->slotcnt; ++i)
        kq_stat_close_slot (ctx, ctx->slots [i]);
      return;
    }

  ts.tv_sec = 0;
  ts.tv_nsec = 0;
  n = kevent (ctx->fd, 0, 0, evs, EV_KQ_EVENT_BUFSIZE, &ts);
  for (i = 0; i < n; ++i)
    {
      ev_stat *w = (ev_stat *)evs [i].udata;
      struct ev_kq_stat_slot *slot = kq_stat_slot_valid (ctx, w);

      if (!slot)
        continue;

      /* Follow the path when the monitored vnode is removed or renamed. */
      if (evs [i].fflags & (NOTE_DELETE | NOTE_RENAME))
        kq_stat_add (EV_A_ w);
      /* A parent hint may mean that a deeper path component now exists. */
      else if (slot->is_dir)
        kq_stat_add (EV_A_ w);

      stat_timer_cb (EV_A_ &w->timer, 0);
    }
}

/* Retry kqueue creation before walking slots, so persistent failure does
 * not rescan every watcher on each polling tick. */
static int
kq_stat_recreate (EV_P)
{
  struct ev_kq_stat *ctx = kq_stat;
  int i;

  ctx->fd = kqueue ();
  if (ctx->fd < 0)
    return 0;

  ctx->owner_pid = getpid ();
  fd_intern (ctx->fd);
  ev_io_set (&ctx->io, ctx->fd, EV_READ);
  ev_io_start (EV_A_ &ctx->io);
  ev_unref (EV_A);

  for (i = 0; i < ctx->slotcnt; ++i)
    kq_stat_add (EV_A_ ctx->slots [i]->watcher);
  return 1;
}

static int
kq_stat_init (EV_P)
{
  struct ev_kq_stat *ctx;

  if (kq_stat_disabled)
    return 0;
  if (kq_stat)
    return kq_stat->fd >= 0 || kq_stat_recreate (EV_A);

  ctx = (struct ev_kq_stat *)calloc (1, sizeof (*ctx));
  if (!ctx)
    return 0;
  ctx->fd = kqueue ();
  if (ctx->fd < 0)
    {
      free (ctx);
      return 0;
    }
  ctx->owner_pid = getpid ();
  fd_intern (ctx->fd);
  ev_io_init (&ctx->io, kq_stat_cb, ctx->fd, EV_READ);
  ev_set_priority (&ctx->io, EV_MAXPRI);
  ctx->io.data = ctx;
  ev_io_start (EV_A_ &ctx->io);
  ev_unref (EV_A); /* internal watcher must not keep the loop running */
  kq_stat = ctx;
  return 1;
}

/* The ordinary stat timer remains the backstop when notification setup fails. */
static void
kq_stat_start (EV_P_ ev_stat *w)
{
  if (kq_stat_init (EV_A))
    {
      kq_stat_add (EV_A_ w);
      stat_timer_cb (EV_A_ &w->timer, 0); /* close the stat/registration race */
    }
}

static void
kq_stat_poll (EV_P_ ev_stat *w, int revents)
{
  if ((revents & EV_TIMER) && ev_is_active (w) && kq_stat_init (EV_A))
    {
      struct ev_kq_stat_slot *slot = kq_stat_slot_valid (kq_stat, w);
      /* An active watcher can lack a slot after an allocation or initial
       * kqueue failure. Retry it along with failed and parent registrations. */
      if (!slot || slot->fd < 0 || slot->is_dir)
        kq_stat_add (EV_A_ w);
    }
}

static void
kq_stat_refresh (EV_P_ ev_stat *w)
{
  if (ev_is_active (w) && kq_stat && kq_stat->fd >= 0
      && (!w->prev.st_nlink || !w->attr.st_nlink
          || (w->prev.st_mode & S_IFMT) != (w->attr.st_mode & S_IFMT)
          || w->prev.st_dev != w->attr.st_dev || w->prev.st_ino != w->attr.st_ino))
    {
      kq_stat_add (EV_A_ w);
      ev_stat_stat (EV_A_ w); /* close the stat/registration race */
    }
}

static void
kq_stat_del (EV_P_ ev_stat *w)
{
  struct ev_kq_stat *ctx = kq_stat;
  struct ev_kq_stat_slot *slot, *last;

  if (!ctx || w->wd < 0)
    return;
  slot = ctx->slots [w->wd];
  last = ctx->slots [--ctx->slotcnt];
  kq_stat_close_slot (ctx, slot);
  free (slot);
  ctx->slots [w->wd] = last;
  if (last != slot)
    last->watcher->wd = w->wd;
  w->wd = -1;
}

/* Recreate the private kqueue after fork and re-register existing watchers. */
static void
kq_stat_fork (EV_P)
{
  struct ev_kq_stat *ctx = kq_stat;
  int i;

  if (!ctx)
    return;

  if (ev_is_active (&ctx->io))
    {
      ev_ref (EV_A);
      ev_io_stop (EV_A_ &ctx->io);
    }
  /* The child may have reused the parent's non-inherited kqueue fd number. */
  if (ctx->fd >= 0 && ctx->owner_pid == getpid ())
    close (ctx->fd);
  ctx->fd = -1;

  for (i = 0; i < ctx->slotcnt; ++i)
    {
      struct ev_kq_stat_slot *slot = ctx->slots [i];
      /* drop the inherited descriptors first: the new kqueue holds no
       * registrations, and kq_stat_add would otherwise keep them */
      kq_stat_close_slot (ctx, slot);
    }
  kq_stat_recreate (EV_A);
}

static void
kq_stat_destroy (EV_P)
{
  struct ev_kq_stat *ctx = kq_stat;

  if (!ctx)
    return;

  /* Ruby GC may have freed the watchers already; release only loop-owned data. */
  while (ctx->slotcnt)
    {
      struct ev_kq_stat_slot *slot = ctx->slots [ctx->slotcnt - 1];
      kq_stat_close_slot (ctx, slot);
      free (slot);
      --ctx->slotcnt;
    }
  free (ctx->slots);
  if (ev_is_active (&ctx->io))
    {
      ev_ref (EV_A);
      ev_io_stop (EV_A_ &ctx->io);
    }
  if (ctx->fd >= 0 && ctx->owner_pid == getpid ())
    close (ctx->fd);
  kq_stat = 0;
  free (ctx);
}
