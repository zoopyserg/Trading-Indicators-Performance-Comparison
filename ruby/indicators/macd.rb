# MACD Technical Indicator.
#
# Moving average convergence/divergence (MACD, or MAC-D) is a trend-following momentum indicator that shows the relationship between two exponential moving averages (EMAs) of a security’s price. The MACD line is calculated by subtracting the 26-period EMA from the 12-period EMA.
# The result of that calculation is the MACD line. A nine-day EMA of the MACD line is called the signal line, which is then plotted on top of the MACD line, which can function as a trigger for buy or sell signals. Traders may buy the security when the MACD line crosses above the signal line and sell—or short—the security when the MACD line crosses below the signal line. MACD indicators can be interpreted in several ways, but the more common methods are crossovers, divergences, and rapid rises/falls
#
# Calculated like so:
# MACD=12-Period EMA − 26-Period EMA
#
# Candles come into initialize in the order "latest first, oldest last"
# The exponential moving average (EMA) is a technical chart indicator that tracks the price of an investment (like a stock or commodity) over time. The EMA is a type of weighted moving average (WMA) that gives more weighting or importance to recent price data. Like the simple moving average (SMA), the EMA is used to see price trends over time, and watching several EMAs at the same time is easy to do with moving average ribbons.
# EMA=Price(t)×k+EMA(y)×(1−k)
# where:
# t=today
# y=yesterday
# N=number of days in EMA
# k=2÷(N+1)
#
# Signal line is taking the 9-period SMA of the macd_line.
#
# keep in mind that sometimes there may be less candles than needed (so ema may crash if I don't handle it)

module Indicators
  class MACD
    def initialize(candle, candles, short_period = 12, long_period = 26, signal_period = 9)
      @candles = candles.select { |c| c['id'] <= candle['id'] }.sort_by { |c| c['id'] }.reverse
      @short_period = 12
      @long_period = 26
      @signal_period = signal_period
      @candles_reversed = @candles.reverse
    end

    def macd_line
      short_ema = ema(@short_period)
      long_ema = ema(@long_period)

      return 0 if short_ema == 0 || long_ema == 0

      ema(@short_period) - ema(@long_period)
    end

    def signal_line
      sma(@signal_period)
    end

    private

    def ema(period)
      return 0 if @candles_reversed.size < period

      k = 2.0 / (period + 1)
      ema = @candles_reversed.first[:close]

      @candles_reversed.each_with_index do |candle, i|
        next if i == 0

        ema = (candle[:close] - ema) * k + ema
      end

      ema
    end

    def sma(period)
      @candles.first(period).collect { |candle| candle['macd_line'] }.reject(&:nil?).sum / period
    end
  end
end
