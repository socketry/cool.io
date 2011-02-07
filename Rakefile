require 'rubygems'
require 'rake'
require 'rake/clean'

begin
  require 'jeweler'
  Jeweler::Tasks.new do |gem|
    gem.name = "cool.io"
    gem.summary = "The cool event framework for Ruby"
    gem.description = "A Ruby wrapper around the libev high performance event library"
    gem.email = "tony@medioh.com"
    gem.homepage = "http://github.com/tarcieri/cool.io"
    gem.authors = ["Tony Arcieri"]
    gem.add_dependency "iobuffer", ">= 0.1.3"
    gem.add_development_dependency "rspec", ">= 2.1.0"
    gem.add_development_dependency "rake-compiler", "~> 0.7.5"
    gem.extensions = FileList["ext/**/extconf.rb"].to_a

    # gem is a Gem::Specification... see http://www.rubygems.org/read/chapter/20 for additional settings
  end
  Jeweler::GemcutterTasks.new
rescue LoadError
  puts "Jeweler (or a dependency) not available. Install it with: gem install jeweler"
end

require 'rspec/core/rake_task'
RSpec::Core::RakeTask.new(:spec) do |spec|
  spec.pattern = 'spec/**/*_spec.rb'
  spec.rspec_opts = %w[-fs -c -b]
end

RSpec::Core::RakeTask.new(:rcov) do |spec|
  spec.pattern = 'spec/**/*_spec.rb'
  spec.rcov = true
  spec.rspec_opts = %w[-fs -c -b]
end

task :default => %w(compile spec)
task :spec => :check_dependencies

require 'rake/rdoctask'
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

def test_suite_cmdline
  require 'find'
  files = []
  Find.find("test") do |f|
    files << f if File.basename(f) =~ /.*spec.*\.rb$/
  end
  cmdline = "#{RUBY} -w -I.:lib:ext:test \
               -e '%w[#{files.join(' ')}].each {|f| require f}'"
end

namespace :test do
  desc "run test suite under valgrind with basic ruby options"
  task :valgrind => :compile do
    system "valgrind --num-callers=50 --error-limit=no \
                         --partial-loads-ok=yes --undef-value-errors=no #{test_suite_cmdline}"
  end
end

