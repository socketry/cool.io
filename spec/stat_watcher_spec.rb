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

def run_with_file_change(path, compact: false)
  reactor = Cool.io::Loop.new

  sw = MyStatWatcher.new(path)
  sw.attach(reactor)

  # libev retains the path pointer passed to ev_stat_init() for the lifetime of
  # the watcher.  If the watcher held RSTRING_PTR(@path) directly, a compaction
  # that relocates the @path String would leave libev dereferencing a stale
  # address on every stat.  Force a maximal relocation here so the watcher keeps
  # operating against a path buffer it owns rather than Ruby-managed memory.
  if compact
    if GC.respond_to?(:verify_compaction_references)
      # verify_compaction_references moves every movable object to a fresh slot.
      # Its keyword arguments have varied across Ruby versions, so fall back.
      begin
        GC.verify_compaction_references(expand_heap: true, toplevel: true)
      rescue ArgumentError
        GC.verify_compaction_references(expand_heap: true)
      end
    elsif GC.respond_to?(:compact)
      GC.compact
    end
  end

  tw = Cool.io::TimerWatcher.new(INTERVAL, true)
  tw.on_timer do
    reactor.stop if sw.accessed
    write_file(path)
  end
  tw.attach(reactor)

  reactor.run

  tw.detach
  sw.detach

  sw
end

def write_file(path)
  File.open(path, "w+") { |f| f.write(rand.to_s) }
end

def delete_file(path)
  File.delete(TEMP_FILE_PATH)
end

describe Cool.io::StatWatcher do

  let :watcher do
    run_with_file_change(TEMP_FILE_PATH)
  end

  before :each do
    write_file(TEMP_FILE_PATH)
  end

  after :each do
    delete_file(TEMP_FILE_PATH)
  end

  it "fire on_change when the file it is watching is modified" do
    expect(watcher.accessed).to eq(true)
  end

  it "should pass previous and current file stat info given a stat watcher" do
    expect(watcher.previous.ino).to eq(watcher.current.ino)
  end

  it "keeps firing on_change after GC compaction relocates objects" do
    skip "GC compaction not available" unless GC.respond_to?(:compact)

    watcher = run_with_file_change(TEMP_FILE_PATH, compact: true)
    expect(watcher.accessed).to eq(true)
  end

  it "raises ArgumentError when the path contains a null byte" do
    expect { MyStatWatcher.new("foo\0bar") }.to raise_error(ArgumentError)
  end

  it "should raise when the handler does not take 2 parameters" do
    class MyStatWatcher < Cool.io::StatWatcher
      remove_method :on_change
      def on_change
      end
    end
    expect { watcher.accessed }.to raise_error(ArgumentError)
  end

end
