require File.expand_path('../spec_helper', __FILE__)
require 'tempfile'

describe Cool.io::UNIXServer, :env => :exclude_win do

  before :each do
    @tmp = Tempfile.new('coolio_unix_server_spec')
    expect(File.unlink(@tmp.path)).to eq(1)
    expect(File.exist?(@tmp.path)).to eq(false)
  end

  it "creates a new Cool.io::UNIXServer" do
    listener = Cool.io::UNIXListener.new(@tmp.path)
    listener.listen(24)
    expect(File.socket?(@tmp.path)).to eq(true)
  end

  it "builds off an existing ::UNIXServer" do
    unix_server = ::UNIXServer.new(@tmp.path)
    expect(File.socket?(@tmp.path)).to eq(true)
    listener = Cool.io::UNIXServer.new(unix_server, Coolio::UNIXSocket)
    listener.listen(24)
    expect(File.socket?(@tmp.path)).to eq(true)
    expect(listener.fileno).to eq(unix_server.fileno)
  end

end
