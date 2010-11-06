require File.expand_path('../spec_helper', __FILE__)

describe Cool.io::TimerWatcher do

  interval = 0.010

  it "can have the on_timer callback defined after creation" do
    @watcher = Cool.io::TimerWatcher.new(interval, true)
    nr = '0'
    @watcher.on_timer { nr.succ! }.should == nil
    @watcher.attach(Cool.io::Loop.default).should == @watcher
    nr.should == '0'
    sleep interval
    Cool.io::Loop.default.run_once
    nr.should == '1'
  end

  it "can be subclassed" do
    class MyTimerWatcher < Cool.io::TimerWatcher
      TMP = '0'

      def on_timer
        TMP.succ!
      end
    end
    @watcher = MyTimerWatcher.new(interval, true)
    @watcher.attach(Cool.io::Loop.default).should == @watcher
    MyTimerWatcher::TMP.should == '0'
    sleep interval
    Cool.io::Loop.default.run_once
    MyTimerWatcher::TMP.should == '1'
  end

  it "can have the on_timer callback redefined between runs" do
    @watcher = Cool.io::TimerWatcher.new(interval, true)
    nr = '0'
    @watcher.on_timer { nr.succ! }.should == nil
    @watcher.attach(Cool.io::Loop.default).should == @watcher
    nr.should == '0'
    sleep interval
    Cool.io::Loop.default.run_once
    nr.should == '1'
    @watcher.detach
    @watcher.on_timer { nr = :foo }.should == nil
    @watcher.attach(Cool.io::Loop.default).should == @watcher
    nr.should == '1'
    sleep interval
    Cool.io::Loop.default.run_once
    nr.should == :foo
  end

  after :each do
    @watcher.detach if defined?(@watcher)
  end
end
