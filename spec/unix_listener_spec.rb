require File.expand_path('../spec_helper', __FILE__)
require 'tempfile'

describe Cool.io::UNIXListener do

  before :each do
    @tmp = Tempfile.new('coolio_unix_listener_spec')
    File.unlink(@tmp.path).should == 1
    File.exist?(@tmp.path).should == false
  end

  it "creates a new UNIXListener" do
    listener = Cool.io::UNIXListener.new(@tmp.path)
    File.socket?(@tmp.path).should == true
  end

  it "builds off an existing UNIXServer" do
    unix_server = UNIXServer.new(@tmp.path)
    File.socket?(@tmp.path).should == true
    listener = Cool.io::UNIXListener.new(unix_server)
    File.socket?(@tmp.path).should == true
    listener.fileno.should == unix_server.fileno
  end

end
