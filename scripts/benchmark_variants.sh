#!/usr/bin/env bash
# Run benchmarks for prime_differences variants
# Mostly vibe coded using Claude 4.6 Opus, then corrected manually

set -euo pipefail

# Configuration
BUILD_DIR="${BUILD_DIR:-build}"
RESULTS_DIR="${RESULTS_DIR:-benchmark_results}"
WORKFILES_DIR="${RESULTS_DIR}/workfiles"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
WARMUP_RUNS=${WARMUP_RUNS:-1}
BENCHMARK_RUNS=${BENCHMARK_RUNS:-3}

usage() {
    cat << EOF
Usage: $0 [WORKLOAD...]

Run benchmarks for prime_differences variants.

Each WORKLOAD should be in the format:
    START:END::DIFFERENCES:DESCRIPTION

Where:
    START           - Start of range
    END             - End of range
    DIFFERENCES     - Comma-separated list of differences to track or one of the built-in difference sets (small,medium,large,huge)
    DESCRIPTION     - Short name for this workload

If no workloads are specified, default workloads will be used.

Examples:
    # Use default workloads
    $0

    # Single custom workload
    $0 "10000000000:11000000000:2:1e9_at_1e10"

    # Multiple custom workloads
    $0 "0:100000000::2:100M" \\
       "0:1000000000::2,4,6:1B_multi"

    # Using built-in difference set
    $0 "10000000000:11000000000::medium:1e9_at_1e10"

Environment variables:
    BUILD_DIR            Build directory (default: build)
    RESULTS_DIR          Results output directory (default: benchmark_results)
    WARMUP_RUNS          Number of warmup runs (default: 1)
    BENCHMARK_RUNS       Number of benchmark runs (default: 3)

EOF
    exit 0
}

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_header() {
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
}

print_success() {
    echo -e "${GREEN}✓${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}⚠${NC} $1"
}

print_error() {
    echo -e "${RED}✗${NC} $1"
}

# Check for help flag
if [[ "${1:-}" == "-h" ]] || [[ "${1:-}" == "--help" ]]; then
    usage
fi

# Check if build directory exists
if [[ ! -d "$BUILD_DIR" ]]; then
    print_error "Build directory '$BUILD_DIR' not found"
    echo "Run: cmake -B $BUILD_DIR -DCMAKE_BUILD_TYPE=Release -DBUILD_ALL_VARIANTS=ON -DBUILD_BENCHMARKS=ON"
    exit 1
fi

# Create results and workfiles directories
mkdir -p "$RESULTS_DIR"
mkdir -p "$WORKFILES_DIR"
RESULTS_FILE="$RESULTS_DIR/benchmark_${TIMESTAMP}.csv"
SUMMARY_FILE="$RESULTS_DIR/benchmark_${TIMESTAMP}_summary.txt"

# Find all benchmark executables
BENCHMARKS=()
for exe in "$BUILD_DIR"/prime_benchmark_*; do
    if [[ -x "$exe" ]]; then
        BENCHMARKS+=("$exe")
    fi
done

if [[ ${#BENCHMARKS[@]} -eq 0 ]]; then
    print_error "No benchmark executables found in $BUILD_DIR"
    echo "Build benchmarks with: cmake --build $BUILD_DIR --target prime_benchmarks --parallel"
    exit 1
fi

# Determine workloads: use command-line arguments if provided, otherwise use defaults
if [[ $# -gt 0 ]]; then
    # Use command-line arguments as workloads
    WORKLOADS=("$@")
    echo "Using ${#WORKLOADS[@]} custom workload(s) from command line"
else
    # Default workloads (start:end:differences:description)
    WORKLOADS=(
        "1000000000000:1001000000000:large:1e9_at_1e12_large"
        "100000000000000:100001000000000:large:1e9_at_1e14_large"
        "10000000000000000:10000001000000000:large:1e9_at_1e16_large"
    )
    echo "Using ${#WORKLOADS[@]} default workload(s)"
fi

# Create all variant × workload combinations and randomize
BENCHMARK_PAIRS=()
for benchmark_exe in "${BENCHMARKS[@]}"; do
    for workload in "${WORKLOADS[@]}"; do
        BENCHMARK_PAIRS+=("$benchmark_exe|$workload")
    done
done

# Randomize the execution order to avoid systematic bias
readarray -t BENCHMARK_PAIRS < <(printf '%s\n' "${BENCHMARK_PAIRS[@]}" | shuf)

# Function to create worker state file
create_worker_state() {
    local file=$1
    local start=$2
    local end=$3
    local differences=$4
    local num_diffs
    local current_counts

    # Count the number of differences and generate matching zeros for current_counts
    num_diffs=$(echo "$differences" | awk -F',' '{print NF}')
    current_counts=$(printf '0,%.0s' $(seq 1 "$num_diffs"))
    current_counts=${current_counts%,}  # Remove trailing comma

    cat > "$file" << EOF
start: $start
end: $end
current_position: $start
checkpoint_frequency: 0
cpu_time: 0.0
differences: $differences
current_counts: $current_counts
parameters: 
EOF
}

print_header "Prime Differences Benchmark Suite"
echo "Results directory: $RESULTS_DIR"
echo "Found ${#BENCHMARKS[@]} variant(s) × ${#WORKLOADS[@]} workload(s) = ${#BENCHMARK_PAIRS[@]} total combinations"
echo ""
echo "Execution order (randomized):"
for i in "${!BENCHMARK_PAIRS[@]}"; do
    IFS='|' read -r benchmark_exe workload <<< "${BENCHMARK_PAIRS[$i]}"
    variant_name=$(basename "$benchmark_exe" | sed 's/prime_benchmark_//')
    IFS=':' read -r start end differences workload_name <<< "$workload"
    echo "  $((i+1)). $variant_name @ $workload_name"
done
echo ""

# CSV header
echo "timestamp,variant,workload,start,end,differences,run,elapsed_ms,numbers_processed,throughput" > "$RESULTS_FILE"

# Summary file header
{
    echo "Prime Differences Benchmark Results"
    echo "===================================="
    echo "Date: $(date)"
    echo "Hostname: $(hostname)"
    echo "CPU: $(lscpu | grep 'Model name' | cut -d':' -f2 | xargs)"
    echo "Cores: $(nproc)"
    echo "Build directory: $BUILD_DIR"
    echo "Variants: ${#BENCHMARKS[@]}"
    echo "Workloads: ${#WORKLOADS[@]}"
    echo "Total combinations: ${#BENCHMARK_PAIRS[@]}"
    echo ""
    echo "Execution order (randomized):"
    for i in "${!BENCHMARK_PAIRS[@]}"; do
        IFS='|' read -r benchmark_exe workload <<< "${BENCHMARK_PAIRS[$i]}"
        variant_name=$(basename "$benchmark_exe" | sed 's/prime_benchmark_//')
        IFS=':' read -r start end differences workload_name <<< "$workload"
        echo "  $((i+1)). $variant_name @ $workload_name"
    done
    echo ""
} > "$SUMMARY_FILE"

# Run benchmarks in randomized order
for i in "${!BENCHMARK_PAIRS[@]}"; do
    pair="${BENCHMARK_PAIRS[i]}"
    IFS='|' read -r benchmark_exe workload <<< "$pair"
    variant_name=$(basename "$benchmark_exe" | sed 's/prime_benchmark_//')
    IFS=':' read -r start end differences workload_name <<< "$workload"

    # Expand built-in differences
    case "$differences" in
        "small")
            differences="0,2,4,6"
            ;;
        "medium")
            differences="0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,50,52,54,56,58,60,62,64"
            ;;
        "large")
            differences="0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,50,52,54,56,58,60,62,64,32768"
            ;;
        "huge")
            differences="
0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,50,52,54,56,58,60,62,64,66,68,70,72,74,76,78,80,82,84,86,88,90,92,94,96,98,100,108,110,112,118,120,122,124,126,128,130,132,148,150,152,154,156,178,180,182,184,208,210,212,238,240,242,254,256,258,268,270,272,284,286,288,298,300,302,328,330,332,358,360,362,388,390,392,418,420,422,448,450,452,460,462,464,478,480,482,508,510,512,514,538,540,542,544,546,548,568,570,572,598,600,602,628,630,632,658,660,662,688,690,692,718,720,722,748,750,752,768,770,772,778,780,782,808,810,812,838,840,842,868,870,872,898,900,902,908,910,912,928,930,932,958,960,962,988,990,992,1018,1020,1022,1024,1026,1048,1050,1052,1078,1080,1082,1108,1110,1112,1130,1132,1134,1138,1140,1142,1168,1170,1172,1182,1184,1186,1198,1200,1202,1218,1220,1222,1228,1230,1232,1246,1248,1250,1258,1260,1262,1270,1272,1274,1288,1290,1292,1318,1320,1322,1326,1328,1330,1348,1350,1352,1354,1356,1358,1368,1370,1372,1378,1380,1382,1408,1410,1412,1438,1440,1442,1444,1468,1470,1472,1474,1476,1478,1498,1500,1502,2046,2048,2050,2308,2310,2312,4094,4096,4098,8190,8192,8194,16382,16384,16386,30028,30030,30032,32766,32768,32770"
            ;;
        *)
            # Leaving as is...
    esac
    # Validate workload format
    if [[ -z "$start" ]] || [[ -z "$end" ]] || [[ -z "$differences" ]] || [[ -z "$workload_name" ]]; then
        print_error "Invalid workload format: $workload"
        echo "Expected format: START:END:DIFFERENCES:DESCRIPTION"
        continue
    fi

    current_time=$(date "+%Y-%m-%d %H:%M:%S")
    print_header "$current_time [$((i+1))/${#BENCHMARK_PAIRS[@]}] $variant_name @ $workload_name"
    echo "  Range: $start - $end"
    echo "  Differences: $differences"

    # Create worker state file
    worker_state_file="$WORKFILES_DIR/${variant_name}_${workload_name}.state"
    create_worker_state "$worker_state_file" "$start" "$end" "$differences"

    # Warmup run
    if [[ $WARMUP_RUNS -gt 0 ]]; then
        for run in $(seq 1 "$WARMUP_RUNS"); do
            echo "  Warmup run $run/$WARMUP_RUNS..."
            "$benchmark_exe" -w "$worker_state_file" -q > /dev/null 2>&1 || true
            # Recreate state file after warmup (it may have been modified)
            create_worker_state "$worker_state_file" "$start" "$end" "$differences"
        done
    fi

    # Benchmark runs
    for run in $(seq 1 "$BENCHMARK_RUNS"); do
        echo -n "  Run $run/$BENCHMARK_RUNS: "

        # Recreate state file for each run
        create_worker_state "$worker_state_file" "$start" "$end" "$differences"

        # Run and capture output
        output=$("$benchmark_exe" -w "$worker_state_file" -q 2>&1 || echo "ERROR")

        if [[ "$output" == "ERROR" ]] || [[ -z "$output" ]]; then
            print_error "Failed"
            continue
        fi

        # Parse output
        elapsed_ms=$(echo "$output" | grep -oP 'Elapsed time: \K[0-9.]+' || echo "0")
        numbers_processed=$(echo "$output" | grep -oP 'Numbers processed: \K[0-9]+' || echo "0")
        throughput=$(echo "$output" | grep -oP 'Throughput: \K[0-9.]+' || echo "0")

        if [[ "$elapsed_ms" == "0" ]]; then
            print_error "Failed to parse output"
            continue
        fi

        echo -e "${GREEN}${elapsed_ms} ms${NC} (${numbers_processed} numbers, ${throughput} numbers/sec)"

        # Write to CSV (escape commas in differences field)
        differences_escaped=${differences//,/;}
        echo "$TIMESTAMP,$variant_name,$workload_name,$start,$end,$differences_escaped,$run,$elapsed_ms,$numbers_processed,$throughput" >> "$RESULTS_FILE"
    done

    echo ""
done

# Generate summary statistics
print_header "Generating Summary Statistics"

{
    echo ""
    echo "Summary Statistics (average over $BENCHMARK_RUNS runs)"
    echo "======================================================="
    echo ""
} >> "$SUMMARY_FILE"

# Calculate averages per variant per workload
for workload in "${WORKLOADS[@]}"; do
    IFS=':' read -r start end differences workload_name <<< "$workload"

    {
    echo "Workload: $workload_name (Range: $start - $end)"
    echo "  Differences: $differences"
    echo "----------------------------------------"
    } >> "$SUMMARY_FILE"

    for benchmark_exe in "${BENCHMARKS[@]}"; do
        variant_name=$(basename "$benchmark_exe" | sed 's/prime_benchmark_//')

        # Calculate average elapsed time
        avg_time=$(awk -F',' -v var="$variant_name" -v wl="$workload_name" \
            '$2 == var && $3 == wl { sum += $8; count++ } END { if (count > 0) print sum/count; else print 0 }' \
            "$RESULTS_FILE")

        # Calculate average throughput
        avg_throughput=$(awk -F',' -v var="$variant_name" -v wl="$workload_name" \
            '$2 == var && $3 == wl { sum += $10; count++ } END { if (count > 0) print sum/count; else print 0 }' \
            "$RESULTS_FILE")

        printf "  %-20s: %10.2f ms  (%10.2f numbers/sec)\n" "$variant_name" "$avg_time" "$avg_throughput" >> "$SUMMARY_FILE"
    done

    echo "" >> "$SUMMARY_FILE"
done

# Print summary to console
cat "$SUMMARY_FILE"

print_success "Benchmark complete!"
echo ""
echo "Results saved to:"
echo "  CSV data:  $RESULTS_FILE"
echo "  Summary:   $SUMMARY_FILE"
echo "  Workfiles: $WORKFILES_DIR"
echo ""
echo "To view results:"
echo "  cat $SUMMARY_FILE"
echo "  column -t -s',' $RESULTS_FILE | less -S"
