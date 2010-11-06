require File.expand_path('../spec_helper', __FILE__)

describe Rev::TimerWatcher do

  interval = 0.010

  it "can have the on_timer callback defined after creation" do
    @watcher = Rev::TimerWatcher.new(interval, true)
    nr = '0'
    @watcher.on_timer { nr.succ! }.should == nil
    @watcher.attach(Rev::Loop.default).should == @watcher
    nr.should == '0'
    sleep interval
    Rev::Loop.default.run_once
    nr.should == '1'
  end

  it "can be subclassed" do
    class MyTimerWatcher < Rev::TimerWatcher
      TMP = '0'

      def on_timer
        TMP.succ!
      end
    end
    @watcher = MyTimerWatcher.new(interval, true)
    @watcher.attach(Rev::Loop.default).should == @watcher
    MyTimerWatcher::TMP.should == '0'
    sleep interval
    Rev::Loop.default.run_once
    MyTimerWatcher::TMP.should == '1'
  end

  it "can have the on_timer callback redefined between runs" do
    @watcher = Rev::TimerWatcher.new(interval, true)
    nr = '0'
    @watcher.on_timer { nr.succ! }.should == nil
    @watcher.attach(Rev::Loop.default).should == @watcher
    nr.should == '0'
    sleep interval
    Rev::Loop.default.run_once
    nr.should == '1'
    @watcher.detach
    @watcher.on_timer { nr = :foo }.should == nil
    @watcher.attach(Rev::Loop.default).should == @watcher
    nr.should == '1'
    sleep interval
    Rev::Loop.default.run_once
    nr.should == :foo
  end

  after :each do
    @watcher.detach if defined?(@watcher)
  end
end
