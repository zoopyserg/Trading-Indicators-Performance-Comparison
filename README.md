# Trading Indicators Library
(Implemented in Ruby, Python, and C)

This repository contains a collection of trading indicator calculations, demonstrating the efficiency and effectiveness of various programming languages in processing financial data. The indicators covered include:

- Stochastic Oscillator
- Relative Strength Index (RSI)
- Know Sure Thing (KST)
- Chaikin Oscillator
- Moving Average Convergence Divergence (MACD)
- Bollinger Bands
- Exponential Moving Average (EMA)
- Simple Moving Average (SMA)

## Performance Benchmarks
The performance of each implementation was benchmarked on a dataset spanning one week. The results are as follows:

- Ruby implementation: ~2 hours
- Python implementation: ~2 minutes
- C implementation: ~2 seconds

## Benchmark Methodology

The performance benchmarks were conducted to compare the execution time of the trading indicators calculations across different programming languages. The methodology employed is straightforward and aimed at providing a relative performance comparison rather than an absolute measurement of efficiency. Here are the steps followed:

1. **Environment Setup**:
    - The benchmarks were conducted on a machine with AMD Ryzen 5950X CPU and 120Gb of RAM.
    - The dataset used for the benchmarks spans one week of trading data.

2. **Benchmark Execution**:
    - The `time` command available in Linux was used to measure the execution time of each implementation.
    - Each implementation was executed using the following pattern:
        - For C:
            ```bash
            time ./c/indicators_recalculator <table_name>
            ```
        - For Python:
            ```bash
            time python3 python/indicators_recalculator.py <table_name>
            ```
        - For Ruby:
            ```bash
            time ruby ruby/indicators_recalculator.rb <table_name>
            ```

3. **Performance Measurement**:
    - The real time (wall-clock time) reported by the `time` command was used as the performance metric.
    - The benchmarks were executed multiple times to ensure consistency in the results, and the average execution time was recorded.

4. **Limitations**:
    - The benchmarks provide a relative comparison of performance but do not account for other factors such as code optimization, system load, or external system interactions that might affect the execution time.
    - The Ruby implementation is legacy and was not updated or optimized for this benchmark.

By documenting the methodology, it aims to provide a clear understanding of the benchmark process, enabling an informed interpretation of the results.

## Project Context
This library is a subset of a larger project, extracted and organized into a separate repository for demonstration and benchmarking purposes.

## Usage Instructions

This project contains implementations of trading indicators in C, Python, and Ruby. Below are the instructions on how to run each version:

### C:
1. Ensure you have the PostgreSQL library installed on your machine.
2. Compile the C program using the following command:
    ```bash
    gcc c/indicators_recalculator.c -o c/indicators_recalculator -I/usr/include/postgresql -lpq -lm
    ```
3. Run the compiled program with the following command, replacing `<table_name>` with the name of your database table:
    ```bash
    ./c/indicators_recalculator <table_name>
    ```

### Python:
1. Run the Python program with the following command, replacing `<table_name>` with the name of your database table:
    ```bash
    python3 python/indicators_recalculator.py <table_name>
    ```

### Ruby:
(Note: The Ruby version is legacy and may not work out-of-the-box. It was originally part of a Rails application with associated migrations.)

1. To use the Ruby version, you would typically initialize an `IndicatorsRecalculator` object with the asset pair and interval as arguments, then call the `call` method on that object:
    ```ruby
    IndicatorsRecalculator.new(asset_pair, interval).call
    ```

## Database Setup
This project assumes the existence of a specific database schema. The repository does not currently include migration scripts to create the necessary database tables and columns. If needed, you may need to create these manually based on the code's expectations.

## Contact Information

For more information about this project, or to get in touch, please visit my [LinkedIn profile](https://www.linkedin.com/in/serge-vinogradoff/).


# Copyright

Copyright © 2023 Serhii Vynohradov (GitHub: [zoopyserg](https://github.com/zoopyserg)). All rights reserved.

The code in this repository is owned by Serhii Vynohradov. You are not granted rights or licenses to the trademarks or copyrights in this repository, including without limitation, any right to copy, distribute, reproduce, modify, or create derivative works thereof.
