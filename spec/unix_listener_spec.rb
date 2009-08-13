require File.dirname(__FILE__) + '/../lib/rev'
require 'tempfile'

describe Rev::UNIXListener do

  before :each do
    @tmp = Tempfile.new('rev_unix_listener_spec')
    File.unlink(@tmp.path).should == 1
    File.exist?(@tmp.path).should == false
  end

  it "creates a new UNIXListener" do
    listener = Rev::UNIXListener.new(@tmp.path)
    File.socket?(@tmp.path).should == true
  end

  it "builds off an existing UNIXServer" do
    unix_server = UNIXServer.new(@tmp.path)
    File.socket?(@tmp.path).should == true
    listener = Rev::UNIXListener.new(unix_server)
    File.socket?(@tmp.path).should == true
    listener.fileno.should == unix_server.fileno
  end

end
