require File.expand_path('../spec_helper', __FILE__)

describe Cool.io::StatWatcher do

  before :each do
    `touch ./test.txt`
  end

  after :each do
    `rm ./test.txt`
  end

  it "fire on change when the file it is watching is modified" do
    class MyStatWatcher < Cool.io::StatWatcher
      attr_accessor :accessed

      def on_change(previous, current)
        self.accessed = true
      end
    end

    sw = MyStatWatcher.new("./test.txt", 0.010)
    sw.attach(Cool.io::Loop.default)
    File.open("./test.txt", "a+") { |f| f.write("content") }
    Cool.io::Loop.default.run_once

    sw.accessed.should_not be_nil
  end

end
