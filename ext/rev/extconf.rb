require 'mkmf'

libs = []

$defs << "-DRUBY_VERSION_CODE=#{RUBY_VERSION.gsub(/\D/, '')}"

if have_func('rb_thread_blocking_region')
  $defs << '-DHAVE_RB_THREAD_BLOCKING_REGION'
end

if have_func('rb_str_set_len')
  $defs << '-DHAVE_RB_STR_SET_LEN'
end

if have_library('rt', 'clock_gettime')
  libs << "-lrt"
end

if have_header('sys/select.h')
  $defs << '-DEV_USE_SELECT'
end

if have_header('poll.h')
  $defs << '-DEV_USE_POLL'
end

if have_header('sys/epoll.h')
  $defs << '-DEV_USE_EPOLL'
end

if have_header('sys/event.h') and have_header('sys/queue.h')
  $defs << '-DEV_USE_KQUEUE'
end

if have_header('port.h')
  $defs << '-DEV_USE_PORT'
end

if have_header('openssl/ssl.h')
  $defs << '-DHAVE_OPENSSL_SSL_H'
  libs << '-lssl -lcrypto'
end

if have_header('sys/resource.h')
  $defs << '-DHAVE_SYS_RESOURCE_H'
end

# ncpu detection specifics
case RUBY_PLATFORM
when /linux/
  $defs << '-DHAVE_LINUX_PROCFS'
else
  if have_func('sysctlbyname', ['sys/param.h', 'sys/sysctl.h'])
    $defs << '-DHAVE_SYSCTLBYNAME'
  end
end

$LIBS << ' ' << libs.join(' ')

dir_config('rev_ext')
create_makefile('rev_ext')

if have_header('openssl/ssl.h') and RUBY_PLATFORM =~ /mingw|win32/
  print "Note--SSL not yet supported on windows--continuing without SSL support"
  makefile_contents = File.read 'Makefile'
  makefile_contents.gsub!('-DHAVE_OPENSSL_SSL_H', '')
  makefile_contents.gsub! 'LIBS = $(LIBRUBYARG_SHARED)', 'LIBS = -lws2_32 $(LIBRUBYARG_SHARED)' # for some reason has to come first or ioctlsocket will be mapped to an [inverted] ruby specific version

  File.open('Makefile', 'w') { |f| f.write makefile_contents }
end
