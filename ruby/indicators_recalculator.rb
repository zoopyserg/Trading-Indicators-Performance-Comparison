require 'tempfile'
require 'json'

class IndicatorsRecalculator
  def initialize(asset_pair, interval, limit = nil, candle_id = nil)
    if candle_id && limit
      @candles = asset_pair.send(IntervalsPresenter.new(interval).to_association_name).where('id <= ?', candle_id).last(limit)
    elsif limit && !candle_id
      @candles = asset_pair.send(IntervalsPresenter.new(interval).to_association_name).order('id DESC').limit(limit)
    else
      @candles = asset_pair.send(IntervalsPresenter.new(interval).to_association_name)
    end
  end

  def call
    @final_candles = @candles

    aroon_period = GlobalSetting.get(:aroon_period)

    bollinger_period = GlobalSetting.get(:bollinger_period)
    bollinger_multiplier = GlobalSetting.get(:bollinger_multiplier)

    macd_long_period = GlobalSetting.get(:macd_long_period)
    macd_short_period = GlobalSetting.get(:macd_short_period)
    macd_signal_period = GlobalSetting.get(:macd_signal_period)

    rsi_period = GlobalSetting.get(:rsi_period)

    stochastic_k_period = GlobalSetting.get(:stochastic_k_period)
    stochastic_d_period = GlobalSetting.get(:stochastic_d_period)

    obv_period = GlobalSetting.get(:obv_period)

    if @candles.present? && @candles.any?
      @candles.each_with_index do |candle, index|
        if index != @candles.size - 1
          aroon_indicator = Indicators::Aroon.new(candle, @candles, aroon_period)
          @final_candles[index].aroon_up = aroon_indicator.aroon_up
          @final_candles[index].aroon_down = aroon_indicator.aroon_down

          bollinger_indicator = Indicators::BollingerBands.new(candle, @candles, bollinger_period, bollinger_multiplier)
          @final_candles[index].bollinger_bands_upper = bollinger_indicator.upper_band
          @final_candles[index].bollinger_bands_lower = bollinger_indicator.lower_band

          kst_indicator = Indicators::KST.new(candle, @candles)
          @final_candles[index].kst = kst_indicator.kst_value
          @final_candles[index].kst_signal = kst_indicator.kst_signal

          macd_indicator = Indicators::MACD.new(candle, @candles, macd_short_period, macd_long_period, macd_signal_period)
          @final_candles[index].macd_line = macd_indicator.macd_line
          @final_candles[index].macd_signal_line = macd_indicator.signal_line

          rsi_indicator = Indicators::RSI.new(candle, @candles, rsi_period)
          @final_candles[index].rsi = rsi_indicator.value

          stochastic_oscillator_indicator = Indicators::StochasticOscillator.new(candle, @candles, stochastic_k_period, stochastic_d_period)
          @final_candles[index].stochastic_k = stochastic_oscillator_indicator.k
          @final_candles[index].stochastic_d = stochastic_oscillator_indicator.d

          chaikin_oscillator_indicator = Indicators::ChaikinOscillator.new(candle, @candles)
          @final_candles[index].chaikin_oscillator = chaikin_oscillator_indicator.value

          last_values = @candles[index..(index + 19)].reverse.map(&:close)
          ema_indicator = Indicators::EMA.new(last_values, 10)
          @final_candles[index].ema_10 = ema_indicator.calculate

          sma_indicator = Indicators::SMA.new(last_values, 10)
          @final_candles[index].sma_10 = sma_indicator.calculate
        end
      end

      save_candlesticks
    end
  end

  def save_candlesticks
    if @final_candles.present? && @final_candles.any?
      # remove last 30 candles (latest ones) from @final_candles, because they have calculation artifacts
      @final_candles = @final_candles[0..-30]
      klass = @candles.first.class
      table_name = klass.table_name

      query = <<-SQL.strip_heredoc
        UPDATE #{table_name}
        SET
          aroon_up = c.aroon_up,
          aroon_down = c.aroon_down,
          bollinger_bands_upper = c.bollinger_bands_upper,
          bollinger_bands_lower = c.bollinger_bands_lower,
          kst = c.kst,
          kst_signal = c.kst_signal,
          macd_line = c.macd_line,
          macd_signal_line = c.macd_signal_line,
          rsi = c.rsi,
          stochastic_k = c.stochastic_k,
          stochastic_d = c.stochastic_d,
          obv = c.obv,
          chaikin_oscillator = c.chaikin_oscillator,
          ema_10 = c.ema_10,
          sma_10 = c.sma_10
        FROM (VALUES
          #{ActiveRecord::Base.sanitize_sql(
      @final_candles.map do |c|
        "(#{c['id']},
          #{c['asset_pair_id']},
          #{c['aroon_up'].present? ? c['aroon_up'] : 0},
          #{c['aroon_down'].present? ? c['aroon_down'] : 0},
          #{c['bollinger_bands_upper'].present? ? c['bollinger_bands_upper'] : 0},
          #{c['bollinger_bands_lower'].present? ? c['bollinger_bands_lower'] : 0},
          #{c['kst'].present? ? c['kst'] : 0},
          #{c['kst_signal'].present? ? c['kst_signal'] : 0},
          #{c['macd_line'].present? ? c['macd_line'] : 0},
          #{c['macd_signal_line'].present? ? c['macd_signal_line'] : 0},
          #{c['rsi'].present? ? c['rsi'] : 0},
          #{c['stochastic_k'].present? ? c['stochastic_k'] : 0},
          #{c['stochastic_d'].present? ? c['stochastic_d'] : 0},
          #{c['obv'].present? ? c['obv'] : 0},
          #{c['chaikin_oscillator'].present? ? c['chaikin_oscillator'] : 0},
          #{c['ema_10'].present? ? c['ema_10'] : 0},
          #{c['sma_10'].present? ? c['sma_10'] : 0}
        )"
      end.join(",")
          )}
        ) AS c (
          id,
          asset_pair_id,
          aroon_up,
          aroon_down,
          bollinger_bands_upper,
          bollinger_bands_lower,
          kst,
          kst_signal,
          macd_line,
          macd_signal_line,
          rsi,
          stochastic_k,
          stochastic_d,
          obv,
          chaikin_oscillator,
          ema_10,
          sma_10
        )
        WHERE c.id = #{table_name}.id AND c.asset_pair_id = #{table_name}.asset_pair_id
      SQL

      ActiveRecord::Base.connection.execute(query)
    end
  rescue => e
    byebug
  end
end
