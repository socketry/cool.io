require File.expand_path('../spec_helper', __FILE__)


describe IO::Buffer do

  let :buffer do
    IO::Buffer.new
  end
  
  it "provides a subset of the methods available in Strings" do
    expect(buffer << "foo").to eq "foo"
    expect(buffer << "bar").to eq "bar"
    expect(buffer.to_str).to eq "foobar"
    expect(buffer.to_str).to eq "foobar"
    expect(buffer.size).to eq 6
  end

  it "provides append and prepend" do
    expect(buffer.append "bar").to eq "bar"
    expect(buffer.prepend "foo").to eq "foo"
    expect(buffer.append "baz").to eq "baz"
    expect(buffer.to_str).to eq "foobarbaz"
  end
  
  context "#read" do
    it "can be used to retrieve the contents of a buffer" do
      expect(buffer << "foo").to eq "foo"
      expect(buffer.read 2).to eq "fo"
      expect(buffer << "bar").to eq "bar"
      expect(buffer.read 2).to eq "ob"
      expect(buffer << "baz").to eq "baz"
      expect(buffer.read 3).to eq "arb"
    end
  end
  
  describe "provides methods for performing non-blocking I/O" do
    require "tempfile"
    
    context "#read_from" do
      context "using local file", :env => :exclude_win do
        let :tmp do
          t = Tempfile.open "read_from"
          t << "foobar"
          t.rewind
          t
        end
        
        it "will read as much data as possible" do
          expect(buffer.read_from tmp).to eq 6
          expect(buffer.to_str).to eq "foobar"
        end
      end
      
      context "using udp socket" do
        before :each do
          @receiver = UDPSocket.open
          @receiver.bind nil, 0
          
          @sender = UDPSocket.open
          @sender.connect "localhost", @receiver.addr[1]
        end
        after :each do
          @receiver.close
          @sender.close
        end
        
        it "will read as much data as possible" do
          select [], [@sender]
          @sender.send "foo", 0
          select [@receiver]
          expect(buffer.read_from @receiver).to eq 3
          expect(buffer.to_str).to eq "foo"
          
          select [], [@sender]
          @sender.send "barbaz", 0
          select [@receiver]
          expect(buffer.read_from @receiver).to eq 6
          expect(buffer.to_str).to eq "foobarbaz"
        end
      end
    end
    
    context "#write_to" do
      context "using local file", :env => :exclude_win do
        let :tmp do
          Tempfile.open "write_to"
        end
        it "writes the contents of the buffer" do
          buffer << "foo"
          expect(buffer.write_to tmp).to eq 3
          tmp.rewind
          expect(tmp.read 3).to eq "foo"
        end
      end
      
      context "using udp socket" do
        before :each do
          @receiver = UDPSocket.open
          @receiver.bind nil, 0
          
          @sender = UDPSocket.open
          @sender.connect "localhost", @receiver.addr[1]
        end
        after :each do
          @receiver.close
          @sender.close
        end
        
        it "will read as much data as possible" do
          buffer << "foo"
          select [], [@sender]
          expect(buffer.write_to @sender).to eq 3
          select [@receiver]
          expect(@receiver.recvfrom_nonblock(3)[0]).to eq "foo"
        end
      end
    end
  end
  
  context "#clear" do
    it "clear all data" do
      buffer << "foo"
      expect(buffer.size).to eq 3
      expect(buffer.empty?).to eq false
      buffer.clear
      expect(buffer.size).to eq 0
      expect(buffer.empty?).to eq true
    end
  end
  
  context "#read_frame" do
    it "Read up to and including the given frame marker" do
      buffer << "foo\nbarbaz"
      data = ""
      expect(buffer.read_frame data, "\n".ord).to eq true
      expect(buffer.empty?).to eq false
      expect(data).to eq "foo\n"
      expect(buffer.to_str).to eq "barbaz"
      
      expect(buffer.read_frame data, "\n".ord).to eq false
      expect(buffer.empty?).to eq true
      expect(data).to eq "foo\nbarbaz"
      expect(buffer.to_str).to eq ""
    end
  end

end
