##
# Contains all logic for gathering and displaying backtraces.

class Backtrace
  include Enumerable

  MAX_WIDTH = 36

  attr_accessor :first_color
  attr_accessor :kernel_color
  attr_accessor :eval_color

  # If passed nil, we assume someone forgot to create a backtrace
  # in the VM.
  def self.backtrace(locations)
    locations ? new(locations) : new([Rubinius::Location::Missing.new])
  end

  def initialize(locations)
    color_config = Rubinius::RUBY_CONFIG["rbx.colorize_backtraces"]
    if ENV['RBX_NOCOLOR'] or color_config == "no" or color_config == "NO"
      @colorize = false
    else
      @colorize = true
    end

    @locations = locations
    @first_color = "\033[0;31m"
    @kernel_color = "\033[0;34m"
    @eval_color = "\033[0;33m"

    @width = Rubinius::TERMINAL_WIDTH
  end

  def [](index)
    @locations[index]
  end

  def show(sep="\n")
    first = true
    if @colorize
      clear = "\033[0m"
    else
      clear = ""
    end

    max = 0
    lines = []
    last_method = nil
    times = 0

    @locations.each do |loc|
      if loc.method == last_method
        times += 1
      else
        lines.last[-1] = times if lines.size > 0
        last_method = loc.method

        str = loc.describe
        max = str.size if str.size > max
        lines << [str, loc, 1]
        times = 0
      end
    end
    max = MAX_WIDTH if max > MAX_WIDTH

    str = ""
    lines.each do |recv, location, rec_times|
      pos  = location.position(Dir.getwd)
      color = color_from_loc(pos, first) if @colorize
      first = false # special handling for first line
      spaces = max - recv.size
      spaces = 0 if spaces < 0

      # trim the path unless we're debugging.
      #unless $DEBUG
      #  pos = "...#{pos[pos.size-max-3..-1]}" if pos.size > max
      #end

      start = " #{' ' * spaces}#{recv} at "

      line_break = @width - start.size - 1

      if pos.size >= line_break
        indent = start.size

        new_pos = ""
        bit = ""
        parts = pos.split("/")
        file = parts.pop

        first = true
        parts.each do |part|
          if bit.size + part.size > line_break
            new_pos << bit << "\n" << (' ' * indent)
            bit = ""
          end

          bit << "/" unless first
          first = false
          bit << part
        end

        new_pos << bit
        if bit.size + file.size > line_break
          new_pos << "\n" << (' ' * indent)
        end
        new_pos << "/" << file
        str << color
        str << start
        str << new_pos
        str << clear
      else
        str << "#{color} #{start}#{pos}#{clear}"
      end

      if rec_times > 1
        str << " (#{rec_times} times)"
      end

      if location.is_jit and $DEBUG
        str << " (jit)"
      end

      str << sep
    end

    return str
  end

  def join(sep)
    show
  end

  alias_method :to_s, :show

  def color_from_loc(loc, first)
    return @first_color if first
    if loc =~ /^kernel/
      @kernel_color
    elsif loc =~ /\(eval\)/
      @eval_color
    else
      ""
    end
  end

  def each
    @locations.each { |f| yield f }
    self
  end

  # HACK: This should be MRI compliant-ish. --rue
  #
  def to_mri()
    @locations.map do |loc|
      "#{loc.position}:in `#{loc.describe_method}'"
    end
  end
end
