require File.dirname(__FILE__) + '/../../../spec_helper'
require File.dirname(__FILE__) + '/../../../shared/file/sticky'
require File.dirname(__FILE__) + '/fixtures/classes'

describe "File::Stat#sticky?" do
  it_behaves_like :file_sticky, :sticky?, FileStat, "File::Stat#sticky?"
end