require File.expand_path('../spec_helper', __FILE__)

describe 'Watcher reattachment' do
  [:timer, :io].each do |type|
    [false, true].each do |disabled|
      [false, true].each do |same_loop|
        it "reattaches a #{disabled ? 'disabled' : 'enabled'} #{type} watcher to #{same_loop ? 'the same' : 'another'} loop" do
          original = Coolio::Loop.new
          destination = same_loop ? original : Coolio::Loop.new
          if type == :io
            server = TCPServer.new('127.0.0.1', 0)
            writer = TCPSocket.new('127.0.0.1', server.addr[1])
            reader = server.accept
            server.close
            watcher = Coolio::IOWatcher.new(reader)
          else
            watcher = Coolio::TimerWatcher.new(0.01, true)
          end
          fired = false
          callback = proc { fired = true }
          if type == :io
            watcher.on_readable(&callback)
          else
            watcher.on_timer(&callback)
          end

          watcher.attach(original)
          watcher.disable if disabled
          expect(watcher.attach(destination)).to equal(watcher)
          expect(watcher.evloop).to equal(destination)
          expect(watcher).to be_enabled
          expect(destination.instance_variable_get(:@active_watchers)).to eq(1)
          unless same_loop
            expect(original.watchers).to be_empty
            expect(original.instance_variable_get(:@active_watchers)).to eq(0)
          end

          writer.write('a') if writer
          sleep 0.02 if type == :timer
          destination.run_once(0.1)
          expect(fired).to eq(true)
          watcher.detach
          expect(destination.watchers).to be_empty
          expect(destination.instance_variable_get(:@active_watchers)).to eq(0)
        ensure
          watcher.detach if watcher && watcher.attached?
          reader.close if reader
          writer.close if writer
          server.close if server && !server.closed?
        end
      end
    end
  end
end
