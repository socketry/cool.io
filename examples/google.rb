#require 'rubygems'
#require 'rev'
require File.dirname(__FILE__) + '/../lib/rev'

l = Rev::Loop.default
c = Rev::HttpClient.connect("www.google.com", 80).attach(l)
c.request('GET', '/search', :query => { :q => 'feces'})
l.run
