def cool_require(gem)
  begin
    m = /(\d+.\d+)/.match(RUBY_VERSION)
    ver = m[1]
    require "#{ver}/#{gem}.so"
  rescue LoadError
    require gem
  end
end
