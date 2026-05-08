# Prime Differences

**Prime Differences** is a high-performance, multi-threaded C++ utility designed to count occurrences of specific prime differences over massive ranges.

It utilizes the [primesieve](https://github.com/kimwalisch/primesieve) library for fast prime generation and employs custom bitwise algorithms optimized with AVX/AVX2 intrinsics to count prime differences efficiently.

## Features

- **High Performance**: Uses `std::popcount`, loop unrolling, and AVX2-friendly bit manipulation to process segments efficiently.
- **System-Aware**: Automatically detects L1/L2 cache sizes to tune the memory segmentation size for optimal cache locality.
- **Multi-threaded**: Distributes the workload across available cores automatically by default.
- **Resumable**: Periodically saves state to disk. If the program is interrupted (e.g., via `Ctrl+C`), it can be resumed from the last checkpoint without losing significant progress.
- **Zero-Dependency Build**: Automatically fetches and compiles `primesieve` locally via CMake to produce a single, portable, (optionally) static binary.

## Building

### Requirements

- **Linux, macOS, or Windows**: Only Linux has been extensively tested.
- **C++ Compiler**: GCC 11+, Clang 14+, or MSVC 19.30+ (VS2022) should work. GCC 14 and Clang 19 have been tested on Linux.
  - _Must support C++23 `<bit>` header._
- **CMake**: Version 3.20 or later.

### Compilation

#### Quick Start (Default Build)

Builds the **optimized variant** with semi-static linking (statically links C++ runtime, uses system `libc`/`libm`):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

_Executable: `build/prime_differences`_

---

#### Algorithm Variants

The project supports multiple algorithm implementations. By default, **variant 0 (optimized)** is built.

**Available variants:**

- `0` = optimized (production algorithm)
- `1` = experimental (development/testing) - for testing new implementations
- `2` = baseline (reference implementation)

**Build a specific variant:**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DSELECTED_VARIANT=2
cmake --build build --parallel
```

_Executable: `build/prime_differences` (copy of selected variant)_

**Build all variants at once:**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_ALL_VARIANTS=ON
cmake --build build --parallel
```

_Executables:_

- `build/prime_differences` (copy of optimized)
- `build/prime_differences_optimized`
- `build/prime_differences_experimental`
- `build/prime_differences_baseline`

---

#### Build Options

**Fully Static Build:**
Creates a standalone binary (requires `glibc-static` and `libxcrypt-static` installed):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_FULLY_STATIC=ON
cmake --build build --parallel
```

**Disable Tests/Benchmarks:**
By default, both are built. To disable:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DBUILD_BENCHMARKS=OFF
cmake --build build --parallel
```

**Debug Build:**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

---

#### Profile Guided Optimization (Maximum Performance)

For large runs, use Profile Guided Optimization (PGO).
This compiles twice: first with instrumentation, then optimized based on real execution data.

```bash
# Step 1: Build with instrumentation
cmake -B build_pgo -DCMAKE_BUILD_TYPE=Release -DPGO_GENERATE=ON
cmake --build build_pgo --parallel

# Step 2: Run training workload (generates profile data)
cd build_pgo
./prime_differences -l 100000000 -j "" -c 0 -t 1
cd ..

# Step 3: Rebuild using profile data
cmake -B build_pgo -DCMAKE_BUILD_TYPE=Release -DPGO_GENERATE=OFF -DPGO_USE=ON
cmake --build build_pgo --parallel
```

_Executables optimized with PGO: `build_pgo/prime_differences`_

**PGO with all variants:**

```bash
# Generate phase
cmake -B build_pgo -DCMAKE_BUILD_TYPE=Release -DBUILD_ALL_VARIANTS=ON -DPGO_GENERATE=ON
cmake --build build_pgo --parallel

# Train each variant
cd build_pgo
./prime_differences_optimized -l 100000000 -j "" -c 0 -t 1
./prime_differences_experimental -l 100000000 -j "" -c 0 -t 1
./prime_differences_baseline -l 100000000 -j "" -c 0 -t 1
cd ..

# Use phase
cmake -B build_pgo -DCMAKE_BUILD_TYPE=Release -DBUILD_ALL_VARIANTS=ON -DPGO_GENERATE=OFF -DPGO_USE=ON
cmake --build build_pgo --parallel
```

---

#### Running Tests

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Run all tests
cd build
ctest --output-on-failure

# Or run directly
./prime_tests
```

**3. Cleaning:**

```bash
rm -rf build build_pgo
```

## Usage

```bash
./prime_differences [OPTIONS]
```

**\*Note:** For best performance, the thread count (`-t`) should match your **Physical Cores**, not Logical Cores (Hyperthreading).\*

### Options

| Flag | Description                                                                                                                               | Default                       |
| :--- | :---------------------------------------------------------------------------------------------------------------------------------------- | :---------------------------- |
| `-d` | **Difference**: The (even) difference(s) to search for (comma separated). Can be a CSV file to load them from.                            | `2`                           |
| `-s` | **Start**: The number to start searching from.                                                                                            | `0`                           |
| `-l` | **Report Level**: The value(s) at which the count should be output (comma separated, inclusive). Can be a CSV file to load them from.     | `10000000000`                 |
| `-t` | **Threads**: Number of worker threads.                                                                                                    | All Available Cores           |
| `-k` | **Tasks**: Number of tasks per thread to schedule for better load balancing between threads.                                              | `8`                           |
| `-j` | **Journal Filename**: Filename where results are stored. An empty value means no results are stored.                                      | `prime_state`                 |
| `-r` | **Report Filename**: At completion, creates a CSV file with this name that contains all counts for all differences at all report levels.  | `prime_state.csv`             |
| `-c` | **Checkpoint Frequency**: Save frequency in seconds.                                                                                      | `600` (10 mins)               |
| `-p` | **Worker Parameters**: Parameters that depend on the variant being used.                                                                  |                               |

### Parameters for Optimized Variant

`SEGMENT_SIZE[,DIFFERENCE_TILE_SIZE]` where:

*  `SEGMENT_SIZE` is the amount of memory that should be used for each task. Must be a power of 2 greater than the largest difference being counted.
*  `DIFFERENCE_TILE_SIZE` is the number of differences considered in a single loop. Must be one of `1`, `2`, `4`, `8`, `12`, `16`, `32`, `64`, or `128`.

### Examples

**Get help information with more examples**

```bash
./build/prime_differences -h
```

**Count twin primes up to 10 billion**

```bash
./build/prime_differences -d 2 -l 10000000000
```

**Count twin, cousin, and sexy primes with multiple report levels**

```bash
./build/prime_differences -d 2,4,6 -l 1000000,10000000,100000000,1000000000
```

**Count all primes in range 1B-2B using 8 threads**

```bash
./build/prime_differences -s 1000000000 -d 0 -l 2000000000 -t 8
```

**Use differences from a file, checkpoint every 5 minutes, store results in results/run1**

```bash
./build/prime_differences -d diffs.txt -l 10000000000 -c 300 -j results/run1
```

**Quick test (should finish quickly)**

```bash
./build/prime_differences -d 2,4,6 -l 1000000
```

## Output Format

The program prints progress to the console. Upon completion, it writes a csv file containing CSV-like data:

```text
x,h,pi_h(x)
100,0,25
100,2,8
...
Total CPU Time:,123.45
```

- **x**: References the reporting threshold (e.g., total count up to x (inclusive)).
- **h**: The even difference size being counted (2, 4, 6, etc.). A difference of 0 gives a count of all primes.
- **pi_h(x)**: The number of times this difference appeared between primes.
- **Total CPU Time:**: The time (in seconds) taken to process all counts.

## Contributors

### Core maintainers

- David Paul — The University of New England, Australia. GitHub: https://github.com/davidjohnpaul

We welcome contributions. See CONTRIBUTING.md for how to contribute, coding standards, and the PR process. For citation information, see CITATION.cff.

**Development Assistance:** **Gemini 3 Pro** was used during the initial development of this project for sanity checking, documentation assistance, and some code generation (e.g., cross-platform compatibility). All AI output was checked and verified by a human before being committed.

**Code Review:** Claude Sonnet 4.5 (Thinking) provided a code review and some of its suggestions were incorporated in the project.


## License

This project is released under the BSD 2-Clause license - see LICENSE for details.
The [`primesieve`](https://github.com/kimwalisch/primesieve) library is downloaded as a sub-component and is also subject to the BSD 2-Clause license.
The [`doctest`](https://github.com/doctest/doctest) framework is downloaded as a sub-component and is subject to the MIT license.
