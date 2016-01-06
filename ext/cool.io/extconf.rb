require 'mkmf'

libs = []

$defs << "-DRUBY_VERSION_CODE=#{RUBY_VERSION.gsub(/\D/, '')}"

have_func('rb_thread_blocking_region')
have_func('rb_thread_call_without_gvl')
have_func('rb_thread_alone')
have_func('rb_str_set_len')
have_library('rt', 'clock_gettime')

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

have_header('sys/resource.h')

# ncpu detection specifics
case RUBY_PLATFORM
when /linux/
  $defs << '-DHAVE_LINUX_PROCFS'
else
  if have_func('sysctlbyname', ['sys/param.h', 'sys/sysctl.h'])
    $defs << '-DHAVE_SYSCTLBYNAME'
  end
end

if RUBY_PLATFORM =~ /solaris/
  # libev/ev.c requires NSIG which is undefined if _XOPEN_SOURCE is defined
  $defs << '-D__EXTENSIONS__'
end

$LIBS << ' ' << libs.join(' ')

dir_config('cool.io_ext')
create_makefile('cool.io_ext')

# win32 needs to link in "just the right order" for some reason or  ioctlsocket will be mapped to an [inverted] ruby specific version.  See libev mailing list for (not so helpful discussion--true cause I'm not sure, but this overcomes the symptom)
if RUBY_PLATFORM =~ /mingw|mswin/
  makefile_contents = File.read 'Makefile'

  # "Init_cool could not be found" when loading cool.io.so.
  # I'm not sure why this is needed. But this line causes "1114 A dynamic link library (DLL) initialization routine failed." So I commented out this line.
  #makefile_contents.gsub! 'DLDFLAGS = ', 'DLDFLAGS = -export-all '

  makefile_contents.gsub! /LIBS = (.*) (\S*ws2_32\S*)/i, 'LIBS = \\2 \\1'
  File.open('Makefile', 'w') { |f| f.write makefile_contents }
end

