/* A private libev copy permits failure injection without production test hooks. */
#include <ruby.h>
#include "ev_wrap.h"
#include <sys/event.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>

static int fail_kqueue, kqueue_calls, fail_calloc, changed, timed_out;
static int invalid_closes;
static int event_reads, flood_events;
static void *flood_watcher;
static const char *registration_change;

static int
test_kevent (int fd, const struct kevent *changes, int count,
             struct kevent *events, int size, const struct timespec *timeout)
{
  int result;
  if (!count && size)
    {
      ++event_reads;
      if (flood_events && event_reads <= 2)
        {
          int i;
          for (i = 0; i < size; ++i)
            EV_SET (&events [i], 0, EVFILT_VNODE, 0, NOTE_WRITE, 0, flood_watcher);
          return size;
        }
    }
  result = kevent (fd, changes, count, events, size, timeout);
  if (count && result == 0 && registration_change)
    {
      FILE *file = fopen (registration_change, "w");
      if (file)
        { fputs ("registration race", file); fclose (file); }
      registration_change = 0;
    }
  return result;
}

static int
test_close (int fd)
{
  if (fd < 0)
    ++invalid_closes;
  return close (fd);
}

static int
test_kqueue (void)
{
  ++kqueue_calls;
  if (fail_kqueue)
    {
      errno = EMFILE;
      return -1;
    }
  return kqueue ();
}

static void *
test_calloc (size_t count, size_t size)
{
  if (fail_calloc)
    {
      fail_calloc = 0;
      errno = ENOMEM;
      return 0;
    }
  return calloc (count, size);
}

/* sys/event.h uses EV_ERROR for a different flag than libev. */
#undef EV_ERROR
#define kevent(...) test_kevent (__VA_ARGS__)
#define kqueue test_kqueue
#define calloc test_calloc
#define close test_close
#include "libev.c"
#undef kevent
#undef kqueue
#undef calloc
#undef close

static void
changed_cb (EV_P_ ev_stat *w, int revents)
{
  changed = 1;
  ev_break (EV_A_ EVBREAK_ALL);
}

static void
timeout_cb (EV_P_ ev_timer *w, int revents)
{
  timed_out = 1;
  ev_break (EV_A_ EVBREAK_ALL);
}

static int
write_file (const char *path, const char *contents)
{
  FILE *file = fopen (path, "w");
  if (!file)
    return 0;
  fputs (contents, file);
  return fclose (file) == 0;
}

static int
await_change (struct ev_loop *loop)
{
  ev_timer deadline;
  changed = timed_out = 0;
  ev_timer_init (&deadline, timeout_cb, 1., 0.);
  ev_timer_start (loop, &deadline);
  ev_run (loop, 0);
  ev_timer_stop (loop, &deadline);
  return changed && !timed_out;
}

#if EV_USE_KQUEUE_STAT
static int
check_reused_fd (struct ev_loop *loop, ev_stat *watcher, int recreate)
{
  int stale_fd = loop->kq_stat->fd;
  int status;
  pid_t pid = fork ();
  if (pid < 0)
    return 0;
  if (!pid)
    {
      struct stat st;
      int fd = open ("/dev/null", O_RDONLY);
      if (fd < 0 || dup2 (fd, stale_fd) < 0)
        _exit (1);
      if (fd != stale_fd)
        close (fd);

      /* The child reuses the descriptor number of its parent's kqueue. */
      if (recreate)
        kq_stat_fork (loop);
      ev_stat_stop (loop, watcher);
      ev_loop_destroy (loop);
      _exit (fstat (stale_fd, &st) == 0 && S_ISCHR (st.st_mode) ? 0 : 1);
    }
  while (waitpid (pid, &status, 0) < 0)
    if (errno != EINTR)
      return 0;
  return WIFEXITED (status) && WEXITSTATUS (status) == 0;
}
#endif

static VALUE
check (VALUE self, VALUE mode, VALUE path)
{
  const char *scenario = StringValueCStr (mode);
  const char *filename = StringValueCStr (path);
  const char *error = 0;
  struct ev_loop *loop;
  ev_stat watcher;
  int disabled = !strcmp (scenario, "disabled");
  int init_failure = !strcmp (scenario, "init_failure");
  int fork_failure = !strcmp (scenario, "fork_failure");
  int slot_failure = !strcmp (scenario, "slot_failure");

  fail_kqueue = kqueue_calls = fail_calloc = invalid_closes = 0;
  event_reads = flood_events = 0;
  registration_change = 0;
  if (!write_file (filename, "a"))
    rb_sys_fail (filename);
  /* A poll backend isolates the stat kqueue from the loop's I/O backend. */
  loop = ev_loop_new (EVFLAG_NOENV | EVBACKEND_POLL | (disabled ? EVFLAG_NOINOTIFY : 0));
  if (!loop)
    rb_raise (rb_eRuntimeError, "ev_loop_new failed");

#if EV_USE_KQUEUE_STAT
  if (slot_failure)
    {
      if (!kq_stat_init (loop))
        { error = "could not initialize stat kqueue"; goto destroy; }
      fail_calloc = 1;
    }
#endif
  fail_kqueue = init_failure;
  ev_stat_init (&watcher, changed_cb, filename, 0.1);
  ev_stat_start (loop, &watcher);

#if EV_USE_KQUEUE_STAT
  if (!strcmp (scenario, "bounded_batch"))
    {
      flood_watcher = &watcher;
      flood_events = 1;
      kq_stat_cb (loop, &loop->kq_stat->io, EV_READ);
      flood_events = 0;
      if (event_reads != 1)
        error = "notification callback drained more than one batch";
      goto stop;
    }
  if (!strcmp (scenario, "io_error") || !strcmp (scenario, "io_error_valid_fd"))
    {
      int refs = loop->activecnt;
      int fd = loop->kq_stat->fd;
      fd_reify (loop);
      if (!strcmp (scenario, "io_error"))
        close (fd);
      fd_kill (loop, fd);
      ev_invoke_pending (loop);
      if (fcntl (fd, F_GETFD) != -1 || errno != EBADF
          || loop->activecnt != refs || loop->kq_stat->fd != -1
          || ev_is_active (&loop->kq_stat->io) || loop->kq_stat->slots [watcher.wd]->fd != -1)
        { error = "I/O failure did not restore polling references"; goto stop; }
    }
  if (!strcmp (scenario, "registration_race"))
    {
      kq_stat_close_slot (loop->kq_stat, loop->kq_stat->slots [watcher.wd]);
      registration_change = filename;
      changed = 0;
      stat_timer_cb (loop, &watcher.timer, EV_TIMER);
      ev_invoke_pending (loop);
      if (!changed || registration_change)
        error = "retry missed a change during registration";
      goto stop;
    }
  if (!strcmp (scenario, "gc_cleanup"))
    {
      size_t size = (size_t)getpagesize ();
      ev_stat *collected = (ev_stat *)mmap (0, size, PROT_READ | PROT_WRITE,
                                          MAP_PRIVATE | MAP_ANON, -1, 0);
      if (collected == MAP_FAILED)
        { error = "could not allocate watcher storage"; goto stop; }
      ev_stat_stop (loop, &watcher);
      ev_stat_init (collected, changed_cb, filename, 0.1);
      ev_stat_start (loop, collected);
      /* Ruby GC may free watcher storage before freeing its loop. Make any
       * access to that storage fail deterministically during loop destruction. */
      if (mprotect (collected, size, PROT_NONE) < 0)
        {
          ev_stat_stop (loop, collected);
          munmap (collected, size);
          error = "could not protect watcher storage";
          goto stop;
        }
      ev_loop_destroy (loop);
      munmap (collected, size);
      return Qtrue;
    }
  if (!strcmp (scenario, "fork_fd_reuse") || !strcmp (scenario, "destroy_fd_reuse"))
    {
      if (!check_reused_fd (loop, &watcher, !strcmp (scenario, "fork_fd_reuse")))
        error = "child closed a descriptor that reused the parent's kqueue number";
      goto stop;
    }
  if (disabled && (kqueue_calls || loop->kq_stat || !loop->kq_stat_disabled))
    { error = "NOINOTIFY did not disable stat kqueue"; goto stop; }
  if (init_failure && (kqueue_calls != 1 || loop->kq_stat || !ev_is_active (&watcher.timer)))
    { error = "injected EMFILE did not leave a polling watcher"; goto stop; }
  if (slot_failure && (fail_calloc || watcher.wd >= 0 || loop->kq_stat->slotcnt))
    { error = "injected allocation failure did not leave an unregistered watcher"; goto stop; }
  if (fork_failure)
    {
      int refs = loop->activecnt;
      fail_kqueue = 1;
      /* Inject failures directly; the watcher spec covers a real fork. */
      kq_stat_fork (loop);
      kq_stat_fork (loop); /* repeated failure must not leak loop references */
      kq_stat_poll (loop, &watcher, EV_TIMER);
      if (loop->kq_stat->fd >= 0 || ev_is_active (&loop->kq_stat->io) || loop->activecnt != refs)
        { error = "failed fork recreation left an active kqueue or leaked references"; goto stop; }
    }
#endif

  if (!write_file (filename, "final") || !await_change (loop))
    { error = "polling did not deliver the change"; goto stop; }

#if EV_USE_KQUEUE_STAT
  if (disabled)
    {
      if (kqueue_calls || loop->kq_stat)
        { error = "polling ignored NOINOTIFY"; goto stop; }
    }
  else
    {
      fail_kqueue = 0;
      /* Retry deterministically; await_change above exercises real timer ticks. */
      kq_stat_poll (loop, &watcher, EV_TIMER);
      if (!loop->kq_stat || loop->kq_stat->fd < 0 || !ev_is_active (&loop->kq_stat->io) || watcher.wd < 0)
        { error = "notifications did not recover after the injected failure"; goto stop; }
      /* A long interval proves the recovered registration delivers native events. */
      watcher.timer.repeat = 60.;
      ev_timer_again (loop, &watcher.timer);
      if (!write_file (filename, "longer") || !await_change (loop))
        { error = "recovered native registration did not deliver the change"; goto stop; }
    }
#else
  if (kqueue_calls)
    { error = "disabled build attempted kqueue initialization"; goto stop; }
#endif

stop:
  fail_kqueue = fail_calloc = 0;
  ev_stat_stop (loop, &watcher);
#if EV_USE_KQUEUE_STAT
  if (!error && loop->kq_stat)
    {
      int calls = kqueue_calls;
      /* Stale calls must not recreate a registration for a stopped watcher. */
      kq_stat_poll (loop, &watcher, EV_TIMER);
      watcher.prev.st_ino = 0;
      kq_stat_refresh (loop, &watcher);
      if (loop->kq_stat->slotcnt || watcher.wd != -1 || kqueue_calls != calls)
        error = "stopped watcher was re-registered";
    }
destroy:
#endif
  ev_loop_destroy (loop);
  if (!error && invalid_closes)
    error = "attempted to close an invalid descriptor";
  if (error)
    rb_raise (rb_eRuntimeError, "%s", error);
  return Qtrue;
}

RUBY_FUNC_EXPORTED void
Init_coolio_kqueue_native (void)
{
  VALUE module = rb_define_module ("CoolioKqueueNativeSpec");
  rb_define_singleton_method (module, "check", check, 2);
}
