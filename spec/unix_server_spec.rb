require File.dirname(__FILE__) + '/../lib/rev'
require 'tempfile'

describe Rev::UNIXServer do

  before :each do
    @tmp = Tempfile.new('rev_unix_server_spec')
    File.unlink(@tmp.path).should == 1
    File.exist?(@tmp.path).should == false
  end

  it "creates a new Rev::UNIXServer" do
    listener = Rev::UNIXListener.new(@tmp.path)
    File.socket?(@tmp.path).should == true
  end

  it "builds off an existing ::UNIXServer" do
    unix_server = ::UNIXServer.new(@tmp.path)
    File.socket?(@tmp.path).should == true
    listener = Rev::UNIXServer.new(unix_server)
    File.socket?(@tmp.path).should == true
    listener.fileno.should == unix_server.fileno
  end

end
