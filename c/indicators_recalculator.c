// gcc c/indicators_recalculator.c -o c/indicators_recalculator -I/usr/include/postgresql -lpq -lm
// ./c/indicators_recalculator
// todo: it is 5% different from Python version, but still it's amazing that it more or less works

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libpq-fe.h>
#include <math.h>

const char *connection = "dbname=dbname user=user password=password host=localhost";

typedef struct {
    int id;
    int asset_pair_id;
    int open_time;
    int close_time;
    float open;
    float high;
    float low;
    float close;
    float volume;
    int trades;
    float aroon_up;
    float aroon_down;
    float bollinger_bands_upper;
    float bollinger_bands_lower;
    float kst;
    float kst_signal;
    float macd_line;
    float macd_signal_line;
    float rsi;
    float stochastic_k;
    float stochastic_d;
    int created_at;
    // temporary fields
    float ad;
    float chaikin_3_ema;
    float chaikin_10_ema;
    float chaikin_oscillator;
    float ema_10;
    float sma_10;
} Candlestick;

char* join_columns(const char *columns[], int num_columns) {
    static char buffer[1024];
    int offset = 0;

    for (int i = 0; i < num_columns; i++) {
        if (i > 0) {
            buffer[offset++] = ',';
            buffer[offset++] = ' ';
        }
        strcpy(buffer + offset, columns[i]);
        offset += strlen(columns[i]);
    }
    buffer[offset] = '\0';

    return buffer;
}

typedef struct {
    int aroon_period;
    int bollinger_period;
    float bollinger_multiplier;
    int macd_long_period;
    int macd_short_period;
    int macd_signal_period;
    int rsi_period;
    int stochastic_k_period;
    int stochastic_d_period;
} GlobalSettings;

GlobalSettings fetch_global_settings() {
    PGconn *conn = PQconnectdb(connection);
    if (PQstatus(conn) == CONNECTION_BAD) {
        fprintf(stderr, "Connection to database failed: %s\n", PQerrorMessage(conn));
        PQfinish(conn);
        exit(1);
    }

    const char *query = "SELECT * FROM global_settings ORDER BY id DESC LIMIT 1";
    PGresult *res = PQexec(conn, query);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "SELECT failed: %s\n", PQerrorMessage(conn));
        PQclear(res);
        PQfinish(conn);
        exit(1);
    }

    GlobalSettings settings;
    settings.aroon_period = atoi(PQgetvalue(res, 0, PQfnumber(res, "aroon_period")));
    settings.bollinger_period = atoi(PQgetvalue(res, 0, PQfnumber(res, "bollinger_period")));
    settings.bollinger_multiplier = atof(PQgetvalue(res, 0, PQfnumber(res, "bollinger_multiplier")));
    settings.macd_long_period = atoi(PQgetvalue(res, 0, PQfnumber(res, "macd_long_period")));
    settings.macd_short_period = atoi(PQgetvalue(res, 0, PQfnumber(res, "macd_short_period")));
    settings.macd_signal_period = atoi(PQgetvalue(res, 0, PQfnumber(res, "macd_signal_period")));
    settings.rsi_period = atoi(PQgetvalue(res, 0, PQfnumber(res, "rsi_period")));
    settings.stochastic_k_period = atoi(PQgetvalue(res, 0, PQfnumber(res, "stochastic_k_period")));
    settings.stochastic_d_period = atoi(PQgetvalue(res, 0, PQfnumber(res, "stochastic_d_period")));

    PQclear(res);
    PQfinish(conn);

    return settings;
}

PGresult* fetch_data(const char *columns[], int num_columns, const char *table_name) {
    PGconn *conn = PQconnectdb(connection);
    if (PQstatus(conn) == CONNECTION_BAD) {
        fprintf(stderr, "Connection to database failed: %s\n", PQerrorMessage(conn));
        PQfinish(conn);
        exit(1);
    }

    char query[1024];
    char *joined_columns = join_columns(columns, num_columns);
    snprintf(query, sizeof(query), "SELECT %s FROM %s ORDER BY id ASC", joined_columns, table_name);

    PGresult *res = PQexec(conn, query);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "SELECT failed: %s\n", PQerrorMessage(conn));
        PQclear(res);
        PQfinish(conn);
        exit(1);
    }

    PQfinish(conn);
    return res;
}

void calculate_aroon(Candlestick *candles, int num_candles, int period) {
    for (int i = period; i < num_candles; i++) {
        int high_index = i;
        int low_index = i;
        for (int j = i; j > i - period; j--) {
            if (candles[j].high > candles[high_index].high) high_index = j;
            if (candles[j].low < candles[low_index].low) low_index = j;
        }
        candles[i].aroon_up = 100.0 * (period - (i - high_index)) / period;
        candles[i].aroon_down = 100.0 * (period - (i - low_index)) / period;
    }
}

void calculate_bollinger_bands(Candlestick *candles, int num_candles, int period, float multiplier) {
    for (int i = period - 1; i < num_candles; i++) {
        float sum = 0;
        for (int j = i; j > i - period; j--) {
            sum += candles[j].close;
        }
        float mean = sum / period;
        float squared_diff_sum = 0;
        for (int j = i; j > i - period; j--) {
            squared_diff_sum += (candles[j].close - mean) * (candles[j].close - mean);
        }
        float std_dev = sqrt(squared_diff_sum / period);
        candles[i].bollinger_bands_upper = mean + multiplier * std_dev;
        candles[i].bollinger_bands_lower = mean - multiplier * std_dev;
    }
}

float calculate_ema(float previous_ema, float close_price, int period) {
    float multiplier = 2.0f / (period + 1);
    return (close_price - previous_ema) * multiplier + previous_ema;
}

void calculate_macd(Candlestick *candles, int num_candles, int short_period, int long_period, int signal_period) {
    float short_ema[num_candles];
    float long_ema[num_candles];
    short_ema[0] = candles[0].close;
    long_ema[0] = candles[0].close;

    // Calculate initial EMAs
    for (int i = 1; i < num_candles; i++) {
        short_ema[i] = calculate_ema(short_ema[i - 1], candles[i].close, short_period);
        long_ema[i] = calculate_ema(long_ema[i - 1], candles[i].close, long_period);
    }

    // Calculate MACD line
    for (int i = 0; i < num_candles; i++) {
        candles[i].macd_line = short_ema[i] - long_ema[i];
    }

    // Calculate Signal line
    candles[0].macd_signal_line = candles[0].macd_line;
    for (int i = 1; i < num_candles; i++) {
        candles[i].macd_signal_line = calculate_ema(candles[i - 1].macd_signal_line, candles[i].macd_line, signal_period);
    }
}

void calculate_rsi(Candlestick *candles, int num_candles, int period) {
    if (num_candles < period) return; // Not enough data

    float avg_gain = 0;
    float avg_loss = 0;

    // Calculate initial average gain and average loss
    for (int i = 1; i <= period; i++) {
        float change = candles[i].close - candles[i - 1].close;
        if (change > 0) {
            avg_gain += change;
        } else {
            avg_loss -= change;
        }
    }
    avg_gain /= period;
    avg_loss /= period;

    candles[period].rsi = 100.0f - (100.0f / (1.0f + avg_gain / avg_loss));

    // Calculate subsequent RSI values
    for (int i = period + 1; i < num_candles; i++) {
        float change = candles[i].close - candles[i - 1].close;
        if (change > 0) {
            avg_gain = (avg_gain * (period - 1) + change) / period;
            avg_loss = (avg_loss * (period - 1)) / period;
        } else {
            avg_gain = (avg_gain * (period - 1)) / period;
            avg_loss = (avg_loss * (period - 1) - change) / period;
        }
        candles[i].rsi = 100.0f - (100.0f / (1.0f + avg_gain / avg_loss));
    }
}

void calculate_stochastic(Candlestick *candles, int num_candles, int k_period, int d_period) {
    for (int i = k_period - 1; i < num_candles; i++) {
        float highest_high = candles[i].high;
        float lowest_low = candles[i].low;

        for (int j = i; j > i - k_period; j--) {
            if (candles[j].high > highest_high) highest_high = candles[j].high;
            if (candles[j].low < lowest_low) lowest_low = candles[j].low;
        }

        candles[i].stochastic_k = (candles[i].close - lowest_low) / (highest_high - lowest_low) * 100.0f;

        if (i >= k_period + d_period - 2) {
            float sum_d = 0.0;
            for (int j = i; j > i - d_period; j--) {
                sum_d += candles[j].stochastic_k;
            }
            candles[i].stochastic_d = sum_d / d_period;
        }
    }
}

float ROC(const Candlestick *candles, int current, int period) {
    return ((candles[current].close - candles[current - period].close) / candles[current - period].close) * 100.0f;
}

void calculate_kst(Candlestick *candles, int num_candles) {
    int max_period = 30 + 15;  // max ROC period + max SMA period
    for (int i = max_period; i < num_candles; i++) {
        float roc1 = ROC(candles, i, 10);
        float roc2 = ROC(candles, i, 15);
        float roc3 = ROC(candles, i, 20);
        float roc4 = ROC(candles, i, 30);

        float kst_value = 0.0;
        for (int j = 0; j < 10; j++) {
            kst_value += (1 * ROC(candles, i - j, 10));
            kst_value += (2 * ROC(candles, i - j, 15));
            kst_value += (3 * ROC(candles, i - j, 20));
        }
        for (int j = 0; j < 15; j++) {
            kst_value += (4 * ROC(candles, i - j, 30));
        }

        candles[i].kst = kst_value / (10 + 20 + 30 + 40);  // total of weights

        float signal_value = 0.0;
        for (int j = 0; j < 9; j++) {
            signal_value += candles[i - j].kst;
        }
        candles[i].kst_signal = signal_value / 9.0;
    }
}

// New function to calculate Chaikin Oscillator
void calculate_chaikin_oscillator(Candlestick *candles, int num_candles) {
    float multiplier_3 = 2.0 / (3 + 1);
    float multiplier_10 = 2.0 / (10 + 1);

    for (int i = 0; i < num_candles; i++) {
        if (candles[i].high == candles[i].low) {
            candles[i].ad = 0;  // Avoid division by zero
        } else {
            candles[i].ad = ((2 * candles[i].close - candles[i].low - candles[i].high) / (candles[i].high - candles[i].low)) * candles[i].volume;
        }

        // Calculate 3-day EMA of AD
        if (i == 0) {
            candles[i].chaikin_3_ema = candles[i].ad;
        } else {
            candles[i].chaikin_3_ema = (candles[i].ad - candles[i-1].chaikin_3_ema) * multiplier_3 + candles[i-1].chaikin_3_ema;
        }

        // Calculate 10-day EMA of AD
        if (i == 0) {
            candles[i].chaikin_10_ema = candles[i].ad;
        } else {
            candles[i].chaikin_10_ema = (candles[i].ad - candles[i-1].chaikin_10_ema) * multiplier_10 + candles[i-1].chaikin_10_ema;
        }

        // Compute the Chaikin Oscillator
        candles[i].chaikin_oscillator = candles[i].chaikin_3_ema - candles[i].chaikin_10_ema;
    }
}

// New function to calculate EMA and SMA
void calculate_ema_sma(Candlestick *candles, int num_candles) {
    float multiplier = 2.0 / (10 + 1);
    float sum_sma = 0;

    for (int i = 0; i < num_candles; i++) {
        // Calculate EMA
        if (i == 0) {
            candles[i].ema_10 = candles[i].close;  // For the first candle, EMA is same as close
        } else {
            candles[i].ema_10 = (candles[i].close - candles[i-1].ema_10) * multiplier + candles[i-1].ema_10;
        }

        // Calculate SMA
        if (i < 9) {
            sum_sma += candles[i].close;
            candles[i].sma_10 = NAN;  // Not enough data points for the first 9 candles
        } else {
            sum_sma += candles[i].close;
            candles[i].sma_10 = sum_sma / 10;
            sum_sma -= candles[i - 9].close;  // Subtract the oldest close value to maintain the sum of last 10 closes
        }
    }
}

void value_or_null(double value, char* buffer, size_t buf_size) {
    if (isnan(value)) {
        strncpy(buffer, "NULL", buf_size);
    } else {
        snprintf(buffer, buf_size, "%.15f", value); // Assuming you want 5 decimal places for the value
    }
    buffer[buf_size - 1] = '\0'; // Ensure null termination
}

#define MAX_BUFFER_SIZE 50000000  // 50MB, adjust as needed
void save_candlesticks(Candlestick *final_candles, int num_candles, const char* table_name) {
    char *query = malloc(MAX_BUFFER_SIZE);  // Buffer for the entire query
    char *values = malloc(MAX_BUFFER_SIZE);  // Buffer for the VALUES part of the query

    // Ensure memory was allocated
    if (!query || !values) {
        fprintf(stderr, "Failed to allocate memory\n");
        free(query);
        free(values);
        return;
    }

    values[0] = '\0';  // Initialize values to an empty string

    // Construct the VALUES part of the SQL query
    for (int i = 0; i < num_candles; i++) {
        char value[1000] = {0};  // Buffer for a single value tuple

        char aroon_up[50];
        char aroon_down[50];
        char bollinger_bands_upper[50];
        char bollinger_bands_lower[50];
        char kst[50];
        char kst_signal[50];
        char macd_line[50];
        char macd_signal_line[50];
        char rsi[50];
        char stochastic_k[50];
        char stochastic_d[50];
        char chaikin_oscillator[50];
        char ema_10[50];
        char sma_10[50];

        value_or_null(final_candles[i].aroon_up, aroon_up, sizeof(aroon_up));
        value_or_null(final_candles[i].aroon_down, aroon_down, sizeof(aroon_down));
        value_or_null(final_candles[i].bollinger_bands_upper, bollinger_bands_upper, sizeof(bollinger_bands_upper));
        value_or_null(final_candles[i].bollinger_bands_lower, bollinger_bands_lower, sizeof(bollinger_bands_lower));
        value_or_null(final_candles[i].kst, kst, sizeof(kst));
        value_or_null(final_candles[i].kst_signal, kst_signal, sizeof(kst_signal));
        value_or_null(final_candles[i].macd_line, macd_line, sizeof(macd_line));
        value_or_null(final_candles[i].macd_signal_line, macd_signal_line, sizeof(macd_signal_line));
        value_or_null(final_candles[i].rsi, rsi, sizeof(rsi));
        value_or_null(final_candles[i].stochastic_k, stochastic_k, sizeof(stochastic_k));
        value_or_null(final_candles[i].stochastic_d, stochastic_d, sizeof(stochastic_d));
        value_or_null(final_candles[i].chaikin_oscillator, chaikin_oscillator, sizeof(chaikin_oscillator));
        value_or_null(final_candles[i].ema_10, ema_10, sizeof(ema_10));
        value_or_null(final_candles[i].sma_10, sma_10, sizeof(sma_10));

        snprintf(value, sizeof(value), "(%d, %d, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s),",
                 final_candles[i].id,
                 final_candles[i].asset_pair_id,
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
                 chaikin_oscillator,
                 ema_10,
                 sma_10
                 );
        strncat(values, value, sizeof(value));
    }
    values[strlen(values) - 1] = '\0';  // Remove trailing comma

    // Construct the final SQL query
    snprintf(query, MAX_BUFFER_SIZE,
         "UPDATE %s "
         "SET aroon_up = c.aroon_up,"
             "aroon_down = c.aroon_down,"
             "bollinger_bands_upper = c.bollinger_bands_upper,"
             "bollinger_bands_lower = c.bollinger_bands_lower,"
             "kst = c.kst,"
             "kst_signal = c.kst_signal,"
             "macd_line = c.macd_line,"
             "macd_signal_line = c.macd_signal_line,"
             "rsi = c.rsi,"
             "stochastic_k = c.stochastic_k,"
             "stochastic_d = c.stochastic_d,"
             "chaikin_oscillator = c.chaikin_oscillator,"
             "ema_10 = c.ema_10,"
             "sma_10 = c.sma_10 "
         "FROM (VALUES %s) "
         "AS c (id, asset_pair_id, aroon_up, aroon_down, bollinger_bands_upper, bollinger_bands_lower, kst, kst_signal, macd_line, macd_signal_line, rsi, stochastic_k, stochastic_d, chaikin_oscillator, ema_10, sma_10) "
         "WHERE c.id = %s.id AND c.asset_pair_id = %s.asset_pair_id;",
         table_name, values, table_name, table_name);

    // Connect and execute the SQL query
    PGconn *conn = PQconnectdb("dbname=dbname user=user password=password host=localhost");
    if (PQstatus(conn) == CONNECTION_BAD) {
        fprintf(stderr, "Connection to database failed: %s\n", PQerrorMessage(conn));
        PQfinish(conn);
        return;
    }

    PGresult *res = PQexec(conn, query);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "UPDATE command failed: %s\n", PQerrorMessage(conn));
        PQclear(res);
        PQfinish(conn);
        return;
    }

    PQclear(res);
    PQfinish(conn);

    free(query);
    free(values);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <table_name>\n", argv[0]);
        exit(1);
    }
    const char *table_name = argv[1];

    const char *desired_columns[] = {
        "id",
        "asset_pair_id",
        "open_time",
        "close_time",
        "open",
        "high",
        "low",
        "close",
        "volume",
        "aroon_up",
        "aroon_down",
        "bollinger_bands_upper",
        "bollinger_bands_lower",
        "kst",
        "kst_signal",
        "macd_line",
        "macd_signal_line",
        "rsi",
        "stochastic_k",
        "stochastic_d",
        "created_at",
        "chaikin_oscillator",
        "ema_10",
        "sma_10"
    };
    int num_columns = sizeof(desired_columns) / sizeof(desired_columns[0]);

    PGresult *res = fetch_data(desired_columns, num_columns, table_name);
    int rows = PQntuples(res);

    Candlestick *candles = malloc(rows * sizeof(Candlestick));
    for (int i = 0; i < rows; i++) {
        candles[i].id = atoi(PQgetvalue(res, i, 0));
        candles[i].asset_pair_id = atoi(PQgetvalue(res, i, 1));
        candles[i].open_time = atoi(PQgetvalue(res, i, 2));
        candles[i].close_time = atoi(PQgetvalue(res, i, 3));
        candles[i].open = atof(PQgetvalue(res, i, 4));
        candles[i].high = atof(PQgetvalue(res, i, 5));
        candles[i].low = atof(PQgetvalue(res, i, 6));
        candles[i].close = atof(PQgetvalue(res, i, 7));
        candles[i].volume = atof(PQgetvalue(res, i, 8));
        candles[i].aroon_up = atof(PQgetvalue(res, i, 9));
        candles[i].aroon_down = atof(PQgetvalue(res, i, 10));
        candles[i].bollinger_bands_upper = atof(PQgetvalue(res, i, 11));
        candles[i].bollinger_bands_lower = atof(PQgetvalue(res, i, 12));
        candles[i].kst = atof(PQgetvalue(res, i, 13));
        candles[i].kst_signal = atof(PQgetvalue(res, i, 14));
        candles[i].macd_line = atof(PQgetvalue(res, i, 15));
        candles[i].macd_signal_line = atof(PQgetvalue(res, i, 16));
        candles[i].rsi = atof(PQgetvalue(res, i, 17));
        candles[i].stochastic_k = atof(PQgetvalue(res, i, 18));
        candles[i].stochastic_d = atof(PQgetvalue(res, i, 19));
        candles[i].created_at = atoi(PQgetvalue(res, i, 20));
        candles[i].chaikin_oscillator = atoi(PQgetvalue(res, i, 21));
        candles[i].ema_10 = atoi(PQgetvalue(res, i, 22));
        candles[i].sma_10 = atoi(PQgetvalue(res, i, 23));
    }

    // Step 3: Calculate the indicators
    GlobalSettings settings = fetch_global_settings();

    // Step 3: Calculate aroon values
    calculate_aroon(candles, rows, settings.aroon_period);
    calculate_bollinger_bands(candles, rows, settings.bollinger_period, settings.bollinger_multiplier);
    // Step 4: Calculate the MACD and Signal lines
    calculate_macd(candles, rows, settings.macd_short_period, settings.macd_long_period, settings.macd_signal_period);
    // Step 5: Calculate the RSI
    calculate_rsi(candles, rows, settings.rsi_period);
    // Step 6: Calculate the Stochastic Oscillator
    calculate_stochastic(candles, rows, settings.stochastic_k_period, settings.stochastic_d_period);
    // Step 7: Calculate the KST and its signal line
    calculate_kst(candles, rows);
    // Step 8: Calculate the SMA and EMA
    calculate_ema_sma(candles, rows);
    // Step 9: Calculate Chaikin
    calculate_chaikin_oscillator(candles, rows);
    // Step 10: Save the updated values to the database
    save_candlesticks(candles, rows, table_name);

    free(candles);
    PQclear(res);
    return 0;
}
