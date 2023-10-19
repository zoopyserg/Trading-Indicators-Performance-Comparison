module Indicators
  class BollingerBands
    attr_reader :candle, :period, :multiplier

    def initialize(candle, candles, period = 20, multiplier = 2)
      @candle = candle
      @period = period
      @multiplier = multiplier
      @candles = candles.select{ |c| c['id'] <= candle['id'] }.sort_by{ |c| c['id'] }.reverse
    end

    def upper_band
      sma + (multiplier * standard_deviation)
    end

    def lower_band
      sma - (multiplier * standard_deviation)
    end

    private

    def sma
      @sma ||= @candles.first(period).collect{ |c| c['close'] }.reject(&:nil?).sum / period
    end

    def standard_deviation
      @standard_deviation ||= Math.sqrt(@candles.first(period).sum{ |c| (c['close'] - sma)**2 } / period)
    end
  end
end
