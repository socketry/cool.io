require 'rake'
require 'rake/testtask'
require 'rake/clean'
require 'rake/gempackagetask'
require 'rake/rdoctask'
require 'tools/rakehelp'
require 'fileutils'
include FileUtils

setup_tests
setup_clean ['ext/rev/Makefile', 'pkg']
setup_rdoc ['README', 'LICENSE', 'lib/**/*.rb', 'doc/**/*.rdoc', 'ext/rev/*.c']

desc "Does a full compile, test run"
task :default => [:compile] #, :test]

desc "Compiles all extensions"
task :compile => [:rev_ext, :http11_client]
task :package => [:clean]

setup_extension("rev", "rev_ext")
setup_extension("http11_client", "http11_client")

summary = "Ruby 1.9 binding to the libev high performance event library"
test_file = "spec/rev_spec.rb"
setup_gem("rev", "0.1.5",  "Tony Arcieri", summary, [], test_file)
