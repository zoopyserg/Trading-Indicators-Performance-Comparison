import sys
import pandas as pd
import numpy as np
from order_book_analyzer_components.database_connector import DatabaseConnector
import psycopg2

pd.set_option('display.max_rows', None)

class IndicatorsRecalculator:
    def calculate_indicators(self, candles, params):
        df = pd.DataFrame(candles)
        df.sort_values(by='id', inplace=True) # <- Sort by id to make sure the candles are in ascending order

        # Aroon
        df['days_since_high'] = df['high'].rolling(window=params['aroon_period']).apply(lambda x: x.argmax(), raw=True)
        df['days_since_low'] = df['low'].rolling(window=params['aroon_period']).apply(lambda x: x.argmin(), raw=True)
        df['aroon_up'] = 100 * df['days_since_high'] / params['aroon_period']
        df['aroon_down'] = 100 * df['days_since_low'] / params['aroon_period']

        # Bollinger Bands
        df['sma'] = df['close'].rolling(window=params['bollinger_period']).mean()
        df['rolling_std'] = df['close'].rolling(window=params['bollinger_period']).std()
        df['bollinger_bands_upper'] = df['sma'] + params['bollinger_multiplier'] * df['rolling_std']
        df['bollinger_bands_lower'] = df['sma'] - params['bollinger_multiplier'] * df['rolling_std']

        # Chaikin Oscillator
        df['ad'] = ((2 * df['close'] - df['low'] - df['high']) / (df['high'] - df['low'])) * df['volume']
        df['chaikin_3_ema'] = df['ad'].ewm(span=3, adjust=False).mean()
        df['chaikin_10_ema'] = df['ad'].ewm(span=10, adjust=False).mean()
        df['chaikin_oscillator'] = df['chaikin_3_ema'] - df['chaikin_10_ema']

        # EMA
        df['ema_10'] = df['close'].ewm(span=10, adjust=False).mean()

        # KST
        for period in params['kst_periods']:
            df[f'roc_{period[0]}'] = df['close'].diff(period[0])
        df['kst'] = sum(df[f'roc_{period[0]}'].rolling(window=period[1]).mean() for period in params['kst_periods'])
        df['kst_signal'] = df['kst'].rolling(window=9).mean()

        # MACD
        df['ema_short'] = df['close'].ewm(span=params['macd_short_period'], adjust=False).mean()
        df['ema_long'] = df['close'].ewm(span=params['macd_long_period'], adjust=False).mean()
        df['macd_line'] = df['ema_short'] - df['ema_long']
        df['macd_signal_line'] = df['macd_line'].ewm(span=params['macd_signal_period'], adjust=False).mean()

        # RSI
        delta = df['close'].diff()
        gain = delta.where(delta > 0, 0)
        loss = -delta.where(delta < 0, 0)
        avg_gain = gain.rolling(window=params['rsi_period']).mean()
        avg_loss = loss.rolling(window=params['rsi_period']).mean()
        rs = avg_gain / avg_loss
        df['rsi'] = 100 - (100 / (1 + rs))

        # SMA
        df['sma_10'] = df['close'].rolling(window=10).mean()

        # Stochastic Oscillator
        df['stochastic_high'] = df['high'].rolling(window=params['stochastic_k_period']).max()
        df['stochastic_low'] = df['low'].rolling(window=params['stochastic_k_period']).min()
        df['stochastic_k'] = 100 * (df['close'] - df['stochastic_low']) / (df['stochastic_high'] - df['stochastic_low'])
        df['stochastic_d'] = df['stochastic_k'].rolling(window=params['stochastic_d_period']).mean()

        df = df.iloc[::-1] # <- Reverse dataframe because Ruby side expect it to be in descending order to store correctly in DB
        return df.to_dict(orient='records')
        # return df[['id', 'high', 'low', 'days_since_high', 'days_since_low', 'aroon_up', 'aroon_down']].tail(200)

    def value_or_null(self, val):
        return 'NULL' if pd.isna(val) else val

    def save_candlesticks(self, final_candles, table_name):
        # Exclude the last 30 candles
        # final_candles = final_candles[:-30]

        # Create the SQL UPDATE statement
        values = ', '.join([
            f"({c['id']}, {c['asset_pair_id']}, {self.value_or_null(c.get('aroon_up', 0))}, {self.value_or_null(c.get('aroon_down', 0))}, {self.value_or_null(c.get('bollinger_bands_upper', 0))}, {self.value_or_null(c.get('bollinger_bands_lower', 0))}, {self.value_or_null(c.get('kst', 0))}, {self.value_or_null(c.get('kst_signal', 0))}, {self.value_or_null(c.get('macd_line', 0))}, {self.value_or_null(c.get('macd_signal_line', 0))}, {self.value_or_null(c.get('rsi', 0))}, {self.value_or_null(c.get('stochastic_k', 0))}, {self.value_or_null(c.get('stochastic_d', 0))}, {self.value_or_null(c.get('obv', 0))}, {self.value_or_null(c.get('chaikin_oscillator', 0))}, {self.value_or_null(c.get('ema_10', 0))}, {self.value_or_null(c.get('sma_10', 0))})"
            for c in final_candles
        ])

        query = f"""
        UPDATE {table_name}
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
        FROM (VALUES {values}) AS c (
            id, asset_pair_id, aroon_up, aroon_down, bollinger_bands_upper, bollinger_bands_lower, kst, kst_signal, macd_line, macd_signal_line, rsi, stochastic_k, stochastic_d, obv, chaikin_oscillator, ema_10, sma_10
        )
        WHERE c.id = {table_name}.id AND c.asset_pair_id = {table_name}.asset_pair_id;
        """

        # Connect to the database and execute the query
        try:
            connection = psycopg2.connect(dbname='dbname', user='username', password='password', host='localhost')
            cursor = connection.cursor()
            cursor.execute(query)
            connection.commit()
        except (Exception, psycopg2.DatabaseError) as error:
            print(f"Error: {error}")
        finally:
            cursor.close()
            connection.close()

if __name__ == "__main__":
    recalculator = IndicatorsRecalculator()

    # Assuming the table name is passed as the first command line argument.
    table_name = sys.argv[1]

    db_connector = DatabaseConnector(
        dbname='dbname',
        user='username',
        password='password',
        host='localhost'
    )

    desired_columns = [
        "id", "asset_pair_id", "open_time", "close_time", "open", "high", "low", "close", "volume", "trades",
        "aroon_up", "aroon_down", "bollinger_bands_upper", "bollinger_bands_lower",
        "kst", "kst_signal", "macd_line", "macd_signal_line", "rsi", "stochastic_k", "stochastic_d", "created_at"
    ]

    candles = db_connector.fetch_data(columns=desired_columns, table_name=table_name)

    settings = db_connector.fetch_global_settings()

    # Example call
    params = {
        'aroon_period': settings['aroon_period'],
        'bollinger_period': settings['bollinger_period'],
        'bollinger_multiplier': settings['bollinger_multiplier'],
        'macd_short_period': settings['macd_short_period'],
        'macd_long_period': settings['macd_long_period'],
        'macd_signal_period': settings['macd_signal_period'],
        'rsi_period': settings['rsi_period'],
        'stochastic_k_period': settings['stochastic_k_period'],
        'stochastic_d_period': settings['stochastic_d_period'],
        'kst_periods': [[10, 10], [15, 10], [20, 10], [30, 15]]
    }

    final_candles = recalculator.calculate_indicators(candles, params)

    recalculator.save_candlesticks(final_candles, table_name)
