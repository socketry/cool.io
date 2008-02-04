require 'mkmf'

flags = []

unless have_func('rb_thread_blocking_region') and have_macro('RB_UBF_DFL', 'ruby.h')
  abort "Rev requires Ruby 1.9.0 or greater"
end

if have_header('sys/select.h')
  flags << '-DEV_USE_SELECT'
end

if have_header('poll.h')
  flags << '-DEV_USE_POLL'
end

if have_header('sys/epoll.h')
  flags << '-DEV_USE_EPOLL'
end

if have_header('sys/event.h') and have_header('sys/queue.h')
  flags << '-DEV_USE_KQUEUE'
end

if have_header('port.h')
  flags << '-DEV_USE_PORT'
end

if have_header('sys/inotify.h')
  flags << '-DEV_USE_INOTIFY'
end

if have_header('linux/proc_fs.h')
  flags << '-DHAVE_LINUX_PROCFS_H'
end

if have_header('sys/param.h')
  flags << '-DHAVE_SYS_PARAM_H'
end

if have_header('sys/sysctl.h')
  flags << '-DHAVE_SYS_SYSCTL_H'
end

$CFLAGS << ' ' << flags.join(' ')

dir_config('rev_ext')
create_makefile('rev_ext')
