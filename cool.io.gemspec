require_relative "lib/cool.io/version"

Gem::Specification.new do |s|
  s.name        = "cool.io"
  s.version     = Coolio::VERSION
  s.authors     = ["Tony Arcieri", "Masahiro Nakagawa"]
  s.email       = ["tony.arcieri@gmail.com", "repeatedly@gmail.com"]
  s.homepage    = "https://github.com/socketry/cool.io"
  s.metadata    = {
    "bug_tracker_uri" => "https://github.com/socketry/cool.io/issues",
    "changelog_uri" => "https://github.com/socketry/cool.io/blob/main/CHANGES.md",
    "source_code_uri" => "https://github.com/socketry/cool.io.git",
    "rubygems_mfa_required" => "true",
  }
  s.summary     = "A cool framework for doing high performance I/O in Ruby"
  s.description = "Cool.io provides a high performance event framework for Ruby which uses the libev C library"
  s.extensions = ["ext/cool.io/extconf.rb"]
  s.licenses   = ["MIT"]

  s.files         = `git ls-files`.split("\n")
  s.test_files    = `git ls-files -- {test,spec,features}/*`.split("\n")
  s.executables   = `git ls-files -- bin/*`.split("\n").map{ |f| File.basename(f) }
  s.require_paths = ["lib"]
  s.required_ruby_version = ">= 3.2"

  s.add_development_dependency "rake-compiler", "~> 1.0"
  s.add_development_dependency "rspec", ">= 2.13.0"
  s.add_development_dependency "rdoc", ">= 3.6.0"
end
