require File.dirname(__FILE__) + '/../lib/rev'

describe Rev::Buffer do
  before :each do
    @buffer = Rev::Buffer.new
    @buffer.size.should == 0
  end
  
  it "appends data" do
    @buffer.append "foo"
    @buffer.size.should == 3
    
    @buffer.append "bar"
    @buffer.size.should == 6
    
    @buffer.read.should == "foobar"
    @buffer.size.should == 0
  end
  
  it "prepends data" do
    @buffer.prepend "foo"
    @buffer.size.should == 3
    
    @buffer.prepend "bar"
    @buffer.size.should == 6
    
    @buffer.read.should == "barfoo"
    @buffer.size.should == 0
  end
  
  it "mixes prepending and appending properly" do
    source_data = %w{foo bar baz qux}
    actions = permutator([:append, :prepend] * 2)
    
    actions.each do |sequence|
      sequence.each_with_index do |entry, i|
        @buffer.send(entry, source_data[i])
      end
      
      @buffer.size.should == sequence.size * 3
      
      i = 0
      expected = sequence.inject('') do |str, action|
        case action
        when :append
          str << source_data[i]
        when :prepend
          str = source_data[i] + str
        end
        
        i += 1
        str
      end
      
      @buffer.read.should == expected
    end
  end
  
  it "reads data in chunks properly" do
    @buffer.append "foobarbazqux"
    
    @buffer.read(1).should == 'f'
    @buffer.read(2).should == 'oo'
    @buffer.read(3).should == 'bar'
    @buffer.read(4).should == 'bazq'
    @buffer.read(1).should == 'u'
    @buffer.read(2).should == 'x'
  end
  
  #######
  private
  #######
  
  def permutator(input)
    output = []
    return output if input.empty?
    
    (0..input.size - 1).inject([]) do |a, n|
      if a.empty?
        input.each { |x| output << [x] }
      else
        input.each { |x| output += a.map { |y| [x, *y] } }
      end
      
      output.dup
    end
  end
end