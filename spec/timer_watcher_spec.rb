require File.expand_path('../spec_helper', __FILE__)

describe Cool.io::TimerWatcher do

  interval = 0.010

  let :loop do
    Cool.io::Loop.new
  end

  it "can have the on_timer callback defined after creation" do
    @watcher = Cool.io::TimerWatcher.new(interval, true)
    nr = '0'
    expect(@watcher.on_timer { nr.succ! }).to be_nil
    expect(@watcher.attach(loop)).to eq(@watcher)
    expect(nr).to eq('0')
    sleep interval
    loop.run_once
    expect(nr).to eq('1')
  end

  it "can be subclassed" do
    class MyTimerWatcher < Cool.io::TimerWatcher
      TMP = '0'

      def on_timer
        TMP.succ!
      end
    end
    @watcher = MyTimerWatcher.new(interval, true)
    expect(@watcher.attach(loop)).to eq(@watcher)
    expect(MyTimerWatcher::TMP).to eq('0')
    sleep interval
    loop.run_once
    expect(MyTimerWatcher::TMP).to eq('1')
  end

  it "can have the on_timer callback redefined between runs" do
    @watcher = Cool.io::TimerWatcher.new(interval, true)
    nr = '0'
    expect(@watcher.on_timer { nr.succ! }).to be_nil
    expect(@watcher.attach(loop)).to eq(@watcher)
    expect(nr).to eq('0')
    sleep interval
    loop.run_once
    expect(nr).to eq('1')
    @watcher.detach
    expect(@watcher.on_timer { nr = :foo }).to be_nil
    expect(@watcher.attach(loop)).to eq(@watcher)
    expect(nr).to eq('1')
    sleep interval
    loop.run_once
    expect(nr).to eq(:foo)
  end

  after :each do
    @watcher.detach if defined?(@watcher)
  end
end
