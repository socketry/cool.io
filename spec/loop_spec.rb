require File.expand_path('../spec_helper', __FILE__)
require 'rbconfig'
require 'open3'

describe Cool.io::Loop do
  it "ignores $LIBEV_FLAGS when skip_environment is true" do
    script = <<~'RUBY'
      require 'cool.io'
      loop = Cool.io::Loop.new(skip_environment: true)
      fired = false
      timer = Cool.io::TimerWatcher.new(0.01)
      timer.on_timer { fired = true; timer.detach }
      timer.attach(loop)
      loop.run
      print fired
    RUBY
    lib = File.expand_path('../lib', __dir__)
    # No CI platform has the Solaris event port backend, so honoring this would leave no backend.
    env = { 'LIBEV_FLAGS' => Cool.io::Loop::EVBACKEND_PORT.to_s }
    output, error, status = Open3.capture3(env, RbConfig.ruby, '-I', lib, '-e', script)
    expect(status.success?).to eq(true), error
    expect(output).to eq("true")
  end

  it "raises an error when no requested backend is usable" do
    backend = RUBY_PLATFORM.include?("solaris") ? :kqueue : :port
    expect {
      Cool.io::Loop.new(skip_environment: true, backend: backend)
    }.to raise_error(RuntimeError, /no usable backend/)
  end
end
