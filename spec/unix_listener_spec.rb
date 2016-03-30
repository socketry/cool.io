require File.expand_path('../spec_helper', __FILE__)
require 'tempfile'

describe Cool.io::UNIXListener, :env => :exclude_win do

  before :each do
    @tmp = Tempfile.new('coolio_unix_listener_spec')
    expect(File.unlink(@tmp.path)).to eq(1)
    expect(File.exist?(@tmp.path)).to eq(false)
  end

  it "creates a new UNIXListener" do
    _listener = Cool.io::UNIXListener.new(@tmp.path)
    expect(File.socket?(@tmp.path)).to eq(true)
  end

  it "builds off an existing UNIXServer" do
    unix_server = UNIXServer.new(@tmp.path)
    expect(File.socket?(@tmp.path)).to eq(true)
    listener = Cool.io::UNIXListener.new(unix_server)
    expect(File.socket?(@tmp.path)).to eq(true)
    expect(listener.fileno).to eq(unix_server.fileno)
  end

end
