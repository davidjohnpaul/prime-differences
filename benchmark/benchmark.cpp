// Prime Differences
// Copyright (c) 2025-2026 David Paul
// SPDX-License-Identifier: BSD-2-Clause
// This file is part of Prime Differences. See LICENSE for details.

// Code to benchmark a prime_differences worker implementation
// Mostly vibe coded using Claude 4.6 Opus, then corrected manually

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <print>
#include <stdexcept>

#include "parsing.hpp"
#include "sys_info.hpp"
#include "types.hpp"
#include "worker_impl.hpp"

std::atomic_flag KEEP_RUNNING = ATOMIC_FLAG_INIT;

void usage(const std::string& cmd, const std::string_view message = {}) {
  if (!message.empty()) {
    std::println("{}\n", message);
  }
  std::println(stderr, "Usage:\t{} [OPTIONS]\n", cmd);
  std::println(stderr, "Options:");
  std::println(stderr, "\t-w WORKER_STATE_FILE  The worker state to benchmark");
  std::println(stderr, "\t-q                    Quiet mode (minimal output for scripting)");
  std::println(stderr, "\t-v                    Verbose mode (show detailed statistics)");
  std::println(stderr, "\t-h                    Show this help message");
  std::println(stderr, "Examples:");
  std::println(stderr, "\t{} -w worker.state", cmd);
  std::println(stderr, "\t{} -w worker.state -q", cmd);
  std::println(stderr, "\t{} -w worker.state -v", cmd);
  std::println(stderr, "Where worker.state contains something like:");
  std::println(stderr, "\tstart: 0");
  std::println(stderr, "\tend: 10000");
  std::println(stderr, "\tcurrent_position: 5");
  std::println(stderr, "\tcheckpoint_frequency: 0");
  std::println(stderr, "\tcpu_time: 0.5");
  std::println(stderr, "\tdifferences: 0,2");
  std::println(stderr, "\tcurrent_counts: 2,1");
  std::println(stderr,
               "Which means search for primes and twin primes from 0 to 10000, "
               "starting from 5 (so two primes and one twin prime have been "
               "found previously, taking 0.5s), and without any checkpointing "
               "(a positive value for checkpoint_frequency specifies after how "
               "many seconds a checkpoint should be saved)");
}

struct BenchmarkConfig {
  worker_state state;
  value_type start_position;
  bool quiet = false;
  bool verbose = false;
};

void parse_args(int argc, char* argv[], BenchmarkConfig& config) {
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "-h" || arg == "--help" || arg == "/?") {
      usage(argv[0]);
      return;
    } else if (arg == "-q" || arg == "--quiet") {
      config.quiet = true;
    } else if (arg == "-v" || arg == "--verbose") {
      config.verbose = true;
    } else if (arg == "-w" || arg == "--worker") {
      if (i + 1 >= argc) {
        throw std::runtime_error("Error: -s requires an argument");
      }
      auto filename = argv[++i];
      config.state = parse_worker_state_from_file(filename);
      config.start_position = config.state.current_position;
    } else {
      throw std::runtime_error(std::format("Error: Unknown option '{}'", arg));
    }
  }
}

std::string format_duration(double milliseconds) {
  if (milliseconds < 1000) {
    return std::format("{} ms", milliseconds);
  }
  double seconds = milliseconds / 1000.0;
  if (seconds < 60) {
    return std::format("{:.2f} s", seconds);
  }
  int minutes = static_cast<int>(seconds / 60);
  seconds = seconds - minutes * 60;
  return std::format("{} m {:.2f} s", minutes, seconds);
}

std::string format_number(value_type num) {
  std::string str = std::to_string(num);
  std::string result;
  int count = 0;
  for (auto it = str.rbegin(); it != str.rend(); ++it) {
    if (count > 0 && count % 3 == 0) {
      result = ',' + result;
    }
    result = *it + result;
    ++count;
  }
  return result;
}

void print_header(const BenchmarkConfig& config) {
  std::println("═══════════════════════════════════════════════════════");
  std::println("Prime Differences Benchmark (Variant {})", SELECTED_VARIANT);
  std::println("═══════════════════════════════════════════════════════");

  // System info
  auto cache_sizes = get_cache_sizes();
  std::println("System Information:");
  std::println("\tNumber of Cores:  {}", DEFAULT_NUM_THREADS);
  std::println("\tL1 Cache Size:    {}", cache_sizes.l1);
  std::println("\tL2 Cache Size:    {}", cache_sizes.l2);
  std::println("\tL3 Cache Size:    {}", cache_sizes.l3);
  std::println("");

  std::println("═══════════════════════════════════════════════════════");
  std::println("Benchmark Configuration:");
  std::println("\tstart:                 {}", config.state.start);
  std::println("\tend:                   {}", config.state.end);
  std::println("\tcurrent_position:      {}", config.state.current_position);
  std::println("\tcheckpoint_frequency:  {}", config.state.checkpoint_frequency);
  std::println("\tcpu_time:              {}", config.state.cpu_time);
  std::println("\tdifferences:           {}", vector_type_to_string(config.state.differences));
  std::println("\tcurrent_count:         {}", vector_type_to_string(config.state.current_counts));
  std::println("═══════════════════════════════════════════════════════");

  std::println();
}

void print_results(const BenchmarkConfig& config, auto start_time, auto end_time) {
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  double elapsed_ms = duration.count();

  auto numbers_processed = config.state.current_position - config.start_position;

  double throughput = (elapsed_ms > 0) ? numbers_processed * 1000 / elapsed_ms : 0;

  if (config.quiet) {
    std::println("Elapsed time: {} ms", static_cast<value_type>(elapsed_ms));
    std::println("Numbers processed: {}", numbers_processed);
    std::println("Throughput: {:2} numbers/s", throughput);
  } else {
    std::println("═══════════════════════════════════════════════════════");
    std::println("Results");
    std::println("═══════════════════════════════════════════════════════");
    std::println("Elapsed time: {} ms", format_duration(elapsed_ms));
    std::println("Numbers processed: {}", format_number(numbers_processed));
    std::println("Throughput: {:2} numbers/s", throughput);
    std::println("═══════════════════════════════════════════════════════");

    if (config.verbose) {
      for (size_t i = 0; i < config.state.differences.size(); i++) {
        auto& diff = config.state.differences[i];
        auto& count = config.state.current_counts[i];
        std::println("Number of primes with difference {} found: {}", format_number(diff),
                     format_number(count));
      }
      std::println("═══════════════════════════════════════════════════════");
    }
  }
}

int benchmark(BenchmarkConfig& config, Worker& worker) {
  // Start timing
  auto start_time = std::chrono::high_resolution_clock::now();
  // Run computation
  try {
    worker.start();
  } catch (const std::exception& e) {
    std::println(stderr, "Error during computation: {}", e.what());
    return 1;
  }
  // End timing
  auto end_time = std::chrono::high_resolution_clock::now();

  print_results(config, start_time, end_time);
  return 0;
}

int main(int argc, char* argv[]) {
  KEEP_RUNNING.test_and_set();
  BenchmarkConfig config;

  try {
    parse_args(argc, argv, config);
  } catch (std::runtime_error& e) {
    usage(argv[0], e.what());
    return 1;
  }

  // Print header unless quiet
  if (!config.quiet) {
    print_header(config);
  }

  // Create worker
  try {
    Worker worker = Worker(config.state);
    return benchmark(config, worker);
  } catch (std::runtime_error& e) {
    std::println(stderr, "Error creating worker from {}: Error: {}", config.state.worker_filename,
                 e.what());
    return 1;
  }
}
