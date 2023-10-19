module Indicators
  class SMA
    attr_reader :period, :values

    # requires values in order "oldest first -> newest last"
    def initialize(values, period)
      @values = values
      @period = period
    end

    def calculate
      return nil if @values.size < @period

      @values.last(@period).sum / @period.to_f
    end
  end
end
