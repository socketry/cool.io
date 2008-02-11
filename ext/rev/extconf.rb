require 'mkmf'

cflags = []
ldflags = []

unless have_func('rb_thread_blocking_region') and have_macro('RB_UBF_DFL', 'ruby.h')
  abort "Rev requires Ruby 1.9.0 or greater"
end

if have_header('sys/select.h')
  cflags << '-DEV_USE_SELECT'
end

if have_header('poll.h')
  cflags << '-DEV_USE_POLL'
end

if have_header('sys/epoll.h')
  cflags << '-DEV_USE_EPOLL'
end

if have_header('sys/event.h') and have_header('sys/queue.h')
  cflags << '-DEV_USE_KQUEUE'
end

if have_header('port.h')
  cflags << '-DEV_USE_PORT'
end

if have_header('openssl/ssl.h')
  cflags << '-DHAVE_OPENSSL_SSL_H'
  ldflags << '-lssl -lcrypto'
end

# ncpu detection specifics
case RUBY_PLATFORM
when /linux/
  cflags << '-DHAVE_LINUX_PROCFS'
else
  if have_func('sysctlbyname', ['sys/param.h', 'sys/sysctl.h'])
    cflags << '-DHAVE_SYSCTLBYNAME'
  end
end

$CFLAGS << ' ' << cflags.join(' ')
$LDFLAGS << ' ' << ldflags.join(' ')

dir_config('rev_ext')
create_makefile('rev_ext')
