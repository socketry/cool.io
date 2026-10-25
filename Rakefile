require 'bundler/gem_tasks'
require 'rake/clean'

require 'rspec/core/rake_task'
RSpec::Core::RakeTask.new

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

spec = Gem::Specification.load("cool.io.gemspec")

Rake::ExtensionTask.new('cool.io_ext', spec) do |ext|
  ext.ext_dir = 'ext/cool.io'
end

if RUBY_PLATFORM.include?('linux')
  require 'ruby_memcheck'
  require 'ruby_memcheck/rspec/rake_task'
  RubyMemcheck.config(binary_name: 'cool.io_ext')
  namespace :spec do
    RubyMemcheck::RSpec::RakeTask.new(valgrind: :compile)
  end
end

CLEAN.include "**/*.o", "**/*.so", "**/*.bundle"
CLEAN.exclude "vendor/**/*.o", "vendor/**/*.so", "vendor/**/*.bundle"
