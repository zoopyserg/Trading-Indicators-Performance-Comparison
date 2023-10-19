module Indicators
  class EMA
    attr_reader :period, :values

    def initialize(values, period)
      @values = values
      @period = period
    end

    def calculate
      sma = @values.first(@period).sum / @period.to_f
      multiplier = 2.0 / (@period + 1)

      ema_values = [@values.first(@period).sum / @period.to_f]

      @values.drop(@period).each do |value|
        ema_values << (value - ema_values.last) * multiplier + ema_values.last
      end

      ema_values.last
    end
  end
end
