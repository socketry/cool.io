require 'rubygems'

GEMSPEC = Gem::Specification.new do |s|
  s.name = "rev"
  s.version = "0.3.2"
  s.authors = "Tony Arcieri"
  s.email = "tony@medioh.com"
  s.date = "2009-10-13"
  s.summary = "Rev is a Ruby binding to the libev high performance event library"
  s.platform = Gem::Platform::RUBY
  s.required_ruby_version = '>= 1.8.6'

  # Gem contents
  s.files = Dir.glob("{lib,ext,examples}/**/*") + ['Rakefile', 'rev.gemspec']
  
  # Dependencies
  s.add_dependency "iobuffer", ">= 0.1.0"

  # RubyForge info
  s.homepage = "http://rev.rubyforge.org"
  s.rubyforge_project = "rev"

  # RDoc settings
  s.has_rdoc = true
  s.rdoc_options = %w(--title Rev --main README.textile --line-numbers)
  s.extra_rdoc_files = ["LICENSE", "README.textile", "CHANGES"]

  # Extensions
  s.extensions = FileList["ext/**/extconf.rb"].to_a
end
