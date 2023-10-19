module Indicators
  class StochasticOscillator
    attr_reader :k_period, :d_period, :candle

    def initialize(candle, candles, k_period, d_period)
      @k_period = k_period
      @d_period = d_period
      @candle = candle
      @candles = candles.select{ |c| c['id'] <= candle['id'] }.first(@k_period)
    end

    def k
      return nil if @candles.size < @k_period

      high_price = @candles.map { |c| c.high }.max
      low_price = @candles.map { |c| c.low }.min
      close_price = @candle.close

      ((close_price - low_price) / (high_price - low_price)) * 100
    end

    def d
      return nil if @candles.size < @d_period

      last_d_candles = @candles.first(@d_period)

      return nil if last_d_candles.any? { |c| c.stochastic_k.nil? }

      last_d_candles.map(&:stochastic_k).reject(&:nil?).sum / @d_period
    end
  end
end
