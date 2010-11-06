$LOAD_PATH.unshift File.expand_path('../../lib', __FILE__)

require 'rubygems'
require 'cool.io'

l = Coolio::Loop.default
c = Coolio::HttpClient.connect("www.google.com", 80).attach(l)
c.request('GET', '/search', :query => { :q => 'feces'})
l.run