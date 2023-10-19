# KST Binance Indicator
# Calculated like so:
# KST=(RCMA #1×1)+(RCMA #2×2)+(RCMA #3×3)+(RCMA #4×4)
# where:
# RCMA #1=10-period SMA of 10-period ROC
# RCMA #2=10-period SMA of 15-period ROC
# RCMA #3=10-period SMA of 20-period ROC
# RCMA #4=15-period SMA of 30-period ROC
#
# Candles come into initialize in the order "latest first, oldest last"
#
# Signal line is taking the 9-period SMA of the KST.
module Indicators
  class KST
    KST_PERIODS = [[10, 10], [15, 10], [20, 10], [30, 15]]

    def initialize(candle, candles)
      @candles = candles.select { |c| c['id'] <= candle['id'] }.reverse
    end

    def kst_value
      kst = 0
      KST_PERIODS.each_with_index do |period, index|
        kst += rcma(period.first, period.last) * (index + 1)
      end
      kst
    end

    def kst_signal
      @candles.last(9).collect { |candle| candle['kst'] }.reject(&:nil?).sum / 9
    end

    private
    def rcma(period, roc_period)
      roc = roc(roc_period)
      sma(roc, period)
    end

    def roc(period)
      @candles.each_cons(period + 1).map do |candles|
        ((candles.last['close'] - candles.first['close']) / candles.first['close']) * 100
      end
    end

    def sma(values, period)
      values.last(period).sum / period
    end
  end
end
