require File.expand_path('../spec_helper', __FILE__)

TEMP_FILE_PATH = "./test.txt"

INTERVAL = 0.010

class MyStatWatcher < Cool.io::StatWatcher
  attr_accessor :accessed, :previous, :current

  def initialize(path)
    super path, INTERVAL
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

  tw = Cool.io::TimerWatcher.new(INTERVAL, true)
  tw.on_timer do
    reactor.stop if sw.accessed
    File.open(path, "w+") { |f| f.write(rand.to_s) }
  end
  tw.attach(reactor)

  reactor.run

  tw.detach
  sw.detach

  sw
end

describe Cool.io::StatWatcher do

  let :sw do
    run_with_file_change(TEMP_FILE_PATH)
  end

  before :each do
    `touch #{TEMP_FILE_PATH}`
  end

  after :each do
    `rm #{TEMP_FILE_PATH}`
  end

  it "fire on_change when the file it is watching is modified" do
    sw.accessed.should eql(true)
  end

  it "should pass previous and current file stat info given a stat watcher" do
    sw.previous.ino.should eql(sw.current.ino)
  end

end
