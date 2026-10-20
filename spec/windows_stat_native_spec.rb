require File.expand_path('../spec_helper', __FILE__)
require 'tmpdir'
require 'fileutils'
require 'rbconfig'

describe 'Windows stat notification internals', :if => RUBY_PLATFORM =~ /mingw|mswin/ do
  before(:context) do
    @build = Dir.mktmpdir('coolio-stat-native-')
    source = File.expand_path('fixtures/windows_stat_native.c', __dir__)
    libev = File.expand_path('../ext/libev', __dir__)
    FileUtils.cp(source, @build)
    File.write(File.join(@build, 'extconf.rb'), <<~RUBY)
      begin
        require 'devkit' if RUBY_PLATFORM =~ /mingw/
      rescue LoadError
        # Other Windows toolchains can provide the compiler through PATH.
      end
      require 'mkmf'
      $INCFLAGS << ' -I"#{libev}"'
      abort 'ws2_32 is required' unless have_library('ws2_32')
      create_makefile('coolio_stat_native')
    RUBY
    File.open(File.join(@build, 'extconf.rb'), 'a') do |file|
      file.write <<~'RUBY'
        # Match cool.io's Windows link order so Winsock calls resolve to ws2_32.
        contents = File.read('Makefile')
        contents.gsub!(/LIBS = (.*) (\S*ws2_32\S*)/i) do
          "LIBS = #{Regexp.last_match(2)} #{Regexp.last_match(1)}"
        end
        File.write('Makefile', contents)
        make = ENV['MAKE'] || RbConfig::CONFIG['MAKE'] || (RUBY_PLATFORM =~ /mswin/ ? 'nmake' : 'make')
        abort 'native stat test build failed' unless system(make)
      RUBY
    end
    output = IO.popen([RbConfig.ruby, 'extconf.rb'], chdir: @build, err: [:child, :out], &:read)
    raise output unless $?.success?
  end

  after(:context) do
    FileUtils.remove_entry(@build) if @build && File.directory?(@build)
  end

  def native_result(script, *args)
    error_log = File.join(@build, 'native-error.log')
    command = [RbConfig.ruby, '-I', @build, '-rcoolio_stat_native', '-e',
      "$stdout.binmode; #{script}", *args]
    output = IO.popen(command, 'rb', err: error_log, &:read)
    expect($?.success?).to eq(true), File.read(error_log)
    Marshal.load(output)
  end

  def native_paths(paths)
    args = paths.flat_map { |path, parent, _| [path, parent ? '1' : '0'] }
    native_result(<<~'RUBY', *args)
      paths = ARGV.each_slice(2).map { |path, parent| CoolioStatNativeSpec.path(path, parent == '1') }
      $stdout.write(Marshal.dump(paths))
    RUBY
  end

  it 'preserves ordinary and extended-length drive roots' do
    paths = [
      ['C:\\file.txt', true, 'C:\\'],
      ['C:\\', false, 'C:\\'],
      ['C:\\', true, 'C:\\'],
      ['\\\\?\\C:\\file.txt', true, '\\\\?\\C:\\'],
      ['\\\\?\\C:\\', false, '\\\\?\\C:\\'],
      ['\\\\?\\C:\\', true, '\\\\?\\C:\\'],
      ['\\\\?\\C:\\dir\\file', true, '\\\\?\\C:\\dir']
    ]
    expect(native_paths(paths)).to eq(paths.map(&:last))
  end

  it 'preserves ordinary and extended-length UNC share roots' do
    paths = [
      ['\\\\server\\share\\', false, '\\\\server\\share\\'],
      ['\\\\server\\share\\', true, '\\\\server\\share\\'],
      ['\\\\server\\share\\file', true, '\\\\server\\share\\'],
      ['\\\\?\\UNC\\server\\share\\', false, '\\\\?\\UNC\\server\\share\\'],
      ['\\\\?\\UNC\\server\\share\\file', true, '\\\\?\\UNC\\server\\share\\']
    ]
    expect(native_paths(paths)).to eq(paths.map(&:last))
  end

  it 'falls back to polling on pipe failure and recovers without creating a second pipe' do
    path = File.join(@build, 'watched.txt')
    result = native_result('$stdout.write(Marshal.dump(CoolioStatNativeSpec.check_pipe(ARGV[0])))', path)
    expect(result).to eq(true)
  end

  it 'destroys the loop without waiting for delayed worker completions' do
    path = File.join(@build, 'shutdown.txt')
    result = native_result('$stdout.write(Marshal.dump(CoolioStatNativeSpec.check_shutdown(ARGV[0])))', path)
    expect(result).to eq(true)
  end

  [[0, 'polling'], [1, 'notifications'], [2, 'polling after an unrelated name change']].each do |mode, description|
    it "checks a shared directory once without alias lookups during #{description}" do
      result = native_result(<<~'RUBY', @build, mode.to_s)
        result = CoolioStatNativeSpec.path_work(ARGV[0], ARGV[1].to_i)
        $stdout.write(Marshal.dump(result))
      RUBY
      # One open and two identity queries for 32 changed files; no alias work.
      expect(result).to eq([1, 2, 0, 0, true])
    end
  end

  it 'keeps finding each watch as watchers stop, restart and re-register' do
    result = native_result('$stdout.write(Marshal.dump(CoolioStatNativeSpec.watch_index(ARGV[0])))', @build)
    expect(result).to eq([1, 2, 0, 0, true, true])
  end

  it 'refreshes aliases after replacement records and retries failed lookups' do
    path = File.join(@build, 'aliases.txt')
    result = native_result('$stdout.write(Marshal.dump(CoolioStatNativeSpec.alias_recovery(ARGV[0])))', path)
    expect(result).to eq([true, true, true, true])
  end

  it 'uses effective notification flags and respects EVFLAG_NOENV' do
    path = File.join(@build, 'flags.txt')
    # EVFLAG_NOINOTIFY = 0x00100000; EVFLAG_NOENV = 0x01000000.
    [[0, 1048576, false], [16777216, 1048576, true],
     [1048576, 0, true], [17825792, 0, false]].each do |flags, environment, expected|
      result = native_result(<<~'RUBY', path, flags.to_s, environment.to_s)
        ENV['LIBEV_FLAGS'] = ARGV[2]
        enabled = CoolioStatNativeSpec.notification_enabled(ARGV[0], ARGV[1].to_i)
        $stdout.write(Marshal.dump(enabled))
      RUBY
      expect(result).to eq(expected), "flags=#{flags}, LIBEV_FLAGS=#{environment}"
    end
  end
end
