require File.dirname(__FILE__) + '/../../spec_helper'
require File.dirname(__FILE__) + '/fixtures/classes'

describe "Array#reverse" do
  it "returns a new array with the elements in reverse order" do
    [].reverse.should == []
    [1, 3, 5, 2].reverse.should == [2, 5, 3, 1]
  end

  it "returns subclass instance on Array subclasses" do
    ArraySpecs::MyArray[1, 2, 3].reverse.class.should == ArraySpecs::MyArray
  end

  it "properly handles recursive arrays" do
    empty = ArraySpecs.empty_recursive_array
    empty.reverse.should == empty

    array = ArraySpecs.recursive_array
    array.reverse.should == [array, array, array, array, array, 3.0, 'two', 1]
  end
end

describe "Array#reverse!" do
  it "reverses the elements in place" do
    a = [6, 3, 4, 2, 1]
    a.reverse!.should equal(a)
    a.should == [1, 2, 4, 3, 6]
    [].reverse!.should == []
  end

  it "properly handles recursive arrays" do
    empty = ArraySpecs.empty_recursive_array
    empty.reverse!.should == [empty]

    array = ArraySpecs.recursive_array
    array.reverse!.should == [array, array, array, array, array, 3.0, 'two', 1]
  end

  ruby_version_is "" .. "1.9" do
    it "raises a TypeError on a frozen array" do
      lambda { ArraySpecs.frozen_array.reverse! }.should raise_error(TypeError)
    end
  end

  ruby_version_is "1.9" do
    it "raises a RuntimeError on a frozen array" do
      lambda { ArraySpecs.frozen_array.reverse! }.should raise_error(RuntimeError)
    end
  end
end
