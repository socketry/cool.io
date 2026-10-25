require File.expand_path('../spec_helper', __FILE__)
require 'tmpdir'
require 'fileutils'
require 'rbconfig'
require 'timeout'

describe 'kqueue stat notification internals', :if => RUBY_PLATFORM =~ /darwin/ do
  def check_native(scenario)
    log = File.join(@build, 'native-output')
    path = File.join(@build, 'watched')
    script = 'CoolioKqueueNativeSpec.check(ARGV[0], ARGV[1])'
    pid = Process.spawn(RbConfig.ruby, '-I', @build, '-rcoolio_kqueue_native',
      '-e', script, scenario, path, out: log, err: [:child, :out])
    begin
      _, status = Timeout.timeout(10) { Process.wait2(pid) }
    rescue Timeout::Error
      Process.kill('KILL', pid)
      Process.wait(pid)
      raise "native stat test timed out: #{scenario}\n#{File.read(log)}"
    end
    expect(status.success?).to eq(true), File.read(log)
  end

  [
    ['enabled', ['-DEV_USE_KQUEUE=1']],
    ['EV_USE_KQUEUE_STAT=0', ['-DEV_USE_KQUEUE=1', '-DEV_USE_KQUEUE_STAT=0']],
    ['EV_USE_KQUEUE=0', ['-DEV_USE_KQUEUE=0']]
  ].each do |configuration, definitions|
    context configuration do
      before(:context) do
        @build = Dir.mktmpdir('coolio-kqueue-native-')
        source = File.expand_path('fixtures/kqueue_stat_native.c', __dir__)
        coolio = File.expand_path('../ext/cool.io', __dir__)
        FileUtils.cp(source, @build)
        File.write(File.join(@build, 'extconf.rb'), <<~RUBY)
          require 'mkmf'
          $INCFLAGS << ' -I"#{coolio}"'
          $defs.concat(#{definitions.inspect})
          create_makefile('coolio_kqueue_native')
        RUBY
        log = IO.popen([RbConfig.ruby, 'extconf.rb'], chdir: @build, err: [:child, :out], &:read)
        raise log unless $?.success?
        make = ENV['MAKE'] || RbConfig::CONFIG['MAKE'] || 'make'
        log = IO.popen([make], chdir: @build, err: [:child, :out], &:read)
        raise log unless $?.success?
      end

      after(:context) do
        FileUtils.remove_entry(@build) if @build && File.directory?(@build)
      end

      if configuration == 'enabled'
        it 'keeps polling through EMFILE and retries native initialization' do
          check_native('init_failure')
        end

        it 'recovers from repeated post-fork kqueue failures without leaking references' do
          check_native('fork_failure')
        end

        it 'retries slot allocation only for active watchers' do
          check_native('slot_failure')
        end

        it 'does not create a kqueue when NOINOTIFY is set' do
          check_native('disabled')
        end

        it 'preserves reused descriptors when recreating the kqueue after fork' do
          check_native('fork_fd_reuse')
        end

        it 'preserves reused descriptors when destroying an inherited loop' do
          check_native('destroy_fd_reuse')
        end

        it 'destroys loop resources without accessing collected watcher storage' do
          check_native('gc_cleanup')
        end
      else
        it 'builds and delivers changes through polling without stat kqueue support' do
          check_native('disabled_build')
        end
      end
    end
  end
end
