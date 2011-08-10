require 'bundler/gem_tasks'
require 'rake/clean'

require 'rspec/core/rake_task'
RSpec::Core::RakeTask.new

RSpec::Core::RakeTask.new(:rcov) do |task|
  task.rcov = true
end

task :default => %w(compile spec)

require 'rdoc/task'
Rake::RDocTask.new do |rdoc|
  version = File.exist?('VERSION') ? File.read('VERSION') : ""

  rdoc.rdoc_dir = 'rdoc'
  rdoc.title = "cool.io #{version}"
  rdoc.rdoc_files.include('README*')
  rdoc.rdoc_files.include('lib/**/*.rb')
end

require 'rake/extensiontask'
Rake::ExtensionTask.new('http11_client') do |ext|
end

Rake::ExtensionTask.new('cool.io_ext') do |ext|
  ext.ext_dir = 'ext/cool.io'
end

# Rebuild parser Ragel
task :http11_parser do
  Dir.chdir "ext/http11_client" do
    target = "http11_parser.c"
    File.unlink target if File.exist? target
    sh "ragel http11_parser.rl | rlgen-cd -G2 -o #{target}"
    raise "Failed to build C source" unless File.exist? target
  end
end

# adapted from http://flavoriffic.blogspot.com/2009/06/easily-valgrind-gdb-your-ruby-c.html
def specs_command
  require "find"
  files = []
  Find.find("spec") do |f|
    files << f if File.basename(f) =~ /.*spec.*\.rb$/
  end
  cmdline = "#{RUBY} -I.:lib:ext:spec \
               -e '%w[#{files.join(' ')}].each { |f| require f }'"
end

namespace :spec do
  desc "run specs with valgrind"
  task :valgrind => :compile do
    system "valgrind --num-callers=15 \
      --partial-loads-ok=yes --undef-value-errors=no \
      --tool=memcheck --leak-check=yes --track-fds=yes \
      --show-reachable=yes #{specs_command}"
  end
end

CLEAN.include "**/*.rbc", "**/*.o", "**/*.so", "**/*.bundle"