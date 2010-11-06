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
    gem.add_development_dependency "rspec", "~> 2.0.0"
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

def make(makedir)
  Dir.chdir(makedir) { sh 'make' }
end

def extconf(dir)
  Dir.chdir(dir) { ruby "extconf.rb" }
end

def setup_extension(dir, extension)
  ext = "ext/#{dir}"
  ext_so = "#{ext}/#{extension}.#{Config::CONFIG['DLEXT']}"
  ext_files = FileList[
    "#{ext}/*.c",
    "#{ext}/*.h",
    "#{ext}/extconf.rb",
    "#{ext}/Makefile",
  ] 

  desc "Builds just the #{extension} extension"
  task extension.to_sym => ["#{ext}/Makefile", ext_so ]

  file "#{ext}/Makefile" => ["#{ext}/extconf.rb"] do
    extconf "#{ext}"
  end

  file ext_so => ext_files do
    make "#{ext}"
    cp ext_so, "lib"
  end
end

setup_extension("cool.io", "cool.io_ext")
setup_extension("http11_client", "http11_client")

task :compile => %w(cool.io_ext http11_client)

# Rebuild parser Ragel
task :http11_parser do
  Dir.chdir "ext/http11_client" do
    target = "http11_parser.c"
    File.unlink target if File.exist? target
    sh "ragel http11_parser.rl | rlgen-cd -G2 -o #{target}"
    raise "Failed to build C source" unless File.exist? target
  end
end

CLEAN.include ["build/*", "**/*.o", "**/*.so", "**/*.a", "**/*.log", "pkg"]
CLEAN.include ["ext/**/Makefile", "lib/cool.io_ext.*", "lib/http11_client.*"]
CLEAN.include ["ext/**/*.#{Config::CONFIG["DLEXT"]}"]
