require 'rake'
require 'rake/clean'
require 'rake/rdoctask'
require 'rake/gempackagetask'
require 'fileutils'
include FileUtils

# Load Rev Gemspec
load 'rev.gemspec'

# Default Rake task is compile
task :default => :compile

# RDoc
Rake::RDocTask.new(:rdoc) do |task|
  task.rdoc_dir = 'doc'
  task.title    = 'Rev'
  task.options = %w(--title Revactor --main README --line-numbers)
  task.rdoc_files.include(['lib/**/*.rb', 'doc/**/*.rdoc', 'ext/rev/*.c'])
  task.rdoc_files.include(['README', 'LICENSE'])
end

# Gem
Rake::GemPackageTask.new(GEMSPEC) do |pkg|
  pkg.need_tar = true
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
    "lib"
  ] 
  
  task "lib" do
    directory "lib"
  end

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

setup_extension("rev", "rev_ext")
setup_extension("http11_client", "http11_client")

task :compile => [:rev_ext, :http11_client]

CLEAN.include ['build/*', '**/*.o', '**/*.so', '**/*.a', 'lib/*-*', '**/*.log']
CLEAN.include ['ext/rev/Makefile', 'lib/rev_ext.*', 'lib/http11_client.*']
