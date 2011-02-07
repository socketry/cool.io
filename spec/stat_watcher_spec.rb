require File.expand_path('../spec_helper', __FILE__)

TEMP_FILE_PATH = "./test.txt"

class MyStatWatcher < Cool.io::StatWatcher
  attr_accessor :accessed, :previous, :current

  def initialize(path)
    super path, 0.010
  end

  def on_change(previous, current)
    self.accessed = true
    self.previous = previous
    self.current  = current
  end
end

def run_with_file_change(path)
  reactor = Cool.io::Loop.default
  sw = MyStatWatcher.new(path)
  sw.attach(reactor)
  File.open(path, "a+") { |f| f.write(rand.to_s) }
  reactor.run_once
  sw
end

describe Cool.io::StatWatcher do

  before :each do
    `touch #{TEMP_FILE_PATH}`
  end

  after :each do
    `rm #{TEMP_FILE_PATH}`
  end

  it "fire on_change when the file it is watching is modified" do
    sw = run_with_file_change(TEMP_FILE_PATH)

    sw.accessed.should eql(true)
  end

  it "should pass previous and current file stat info given a stat watcher" do
    sw = run_with_file_change(TEMP_FILE_PATH)

    sw.previous.ino.should eql(sw.current.ino)
  end

end
