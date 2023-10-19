module Indicators
  class RSI
    # todo: N+1 Optimization (perhaps pure SQL function)
    attr_reader :candle

    def initialize(candle, candles, period = 14)
      @period = period
      @candle = candle
      @candles = candles.select{ |c| c['id'] <= candle['id'] }.first(@period) # note - it takes first X candles when say OBV I had to sort_by(&:id).last(period)
    end

    def value
      return 100 if actual_period == 0
      average_gain = get_gains.sum / actual_period.to_f
      average_loss = get_losses.sum / actual_period.to_f
      return 100 if average_loss.zero?

      rs = average_gain / average_loss
      rsi = 100 - (100 / (1 + rs))
    end

    def actual_period
      @candles.size
    end

    def get_gains
      gains = []
      @candles.each_cons(2) do |current_candle, previous_candle|
        gain = current_candle.close - previous_candle.close
        gains << gain if gain.positive?
      end
      gains
    end

    def get_losses
      losses = []
      @candles.each_cons(2) do |current_candle, previous_candle|
        loss = previous_candle.close - current_candle.close
        losses << loss if loss.positive?
      end
      losses
    end
  end
end
