module Indicators
  class Aroon
    attr_reader :candle, :period

    def initialize(candle, candles, period = 25)
      @candle = candle
      @period = period
      @candles = candles.select{ |c| c['id'] <= candle['id'] }.first(@period)
    end

    def aroon_up
      return nil if @candles.size < @period

      highest_index = @candles.each_with_index.max_by { |c, i| c[:high] }[1]
      (100 * (period - highest_index).to_f / period).round(2)
    end

    def aroon_down
      return nil if @candles.size < @period

      lowest_index = @candles.each_with_index.min_by { |c, i| c[:low] }[1]
      (100 * (period - lowest_index).to_f / period).round(2)
    end
  end
end
