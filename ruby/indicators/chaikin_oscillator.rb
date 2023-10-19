module Indicators
  class ChaikinOscillator
    attr_reader :candles

    def initialize(candle, candles)
      @candle = candle
      @candles = candles.select { |c| c[:id] <= candle[:id] }.sort_by { |c| c[:id] }
    end

    def value
      return nil if @candles.size < 10

      adl = ad_line
      three_period_ema = Indicators::EMA.new(adl.last(6), 3).calculate
      ten_period_ema = Indicators::EMA.new(adl.last(20), 10).calculate

      three_period_ema - ten_period_ema
    end

    private

    def ad_line
      @ad_line ||= @candles.map do |candle|
        ((candle[:close] - candle[:low]) - (candle[:high] - candle[:close])) / (candle[:high] - candle[:low]) * candle[:volume]
      end
    end
  end
end
