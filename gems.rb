source "https://rubygems.org"

# Specify your gem's dependencies in cool.io.gemspec
gemspec

group :maintenance, optional: true do
  gem "bake-gem"
  gem "bake-modernize"
end
