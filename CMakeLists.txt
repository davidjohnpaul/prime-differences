# Prime Differences
# BSD-2-Clause License
# Copyright (c) 2025-2026 David Paul
# This file is part of Prime Differences. See LICENSE for details.
cmake_minimum_required(VERSION 3.30)
project(prime_differences VERSION 1.2.0 LANGUAGES CXX)

# File to configure and build Prime Differences using CMake.
# Mostly vibe coded using Claude 4.6 Opus, then corrected manually

# -----------------------------------------------------------------------------
# Configuration
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (Debug, Release)" FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")

# Testing switches
option(BUILD_TESTING "Build unit tests" ON)

# Benchmark switches
option(BUILD_BENCHMARKS "Build benchmark suite" ON)

# Installation switches
option(ENABLE_INSTALL "Enable installation of built targets" ON)

# Profile Guided Optimisation Switches
option(PGO_GENERATE "Build with Profile Guided Optimization (Generation phase)" OFF)
option(PGO_USE      "Build with Profile Guided Optimization (Use phase)"      OFF)

if(PGO_GENERATE AND PGO_USE)
    message(FATAL_ERROR "PGO_GENERATE and PGO_USE are mutually exclusive")
endif()

# Linking Switches
# Default is OFF (Semi-Static), which links C++ runtime statically but uses system libc/libm.
# Set to ON for a fully stand-alone binary (requires glibc-static / libm.a installed on OS).
option(BUILD_FULLY_STATIC "Link everything (including glibc) statically" OFF)

# Variant switches
option(BUILD_ALL_VARIANTS "Build all algorithm variants" OFF)

# Variant selection
if(NOT DEFINED SELECTED_VARIANT)
    set(SELECTED_VARIANT "0")
endif()

if(NOT SELECTED_VARIANT MATCHES "^[0-9]+$")
    message(WARNING "SELECTED_VARIANT='${SELECTED_VARIANT}' is not a number, defaulting to 0")
    set(SELECTED_VARIANT "0")
endif()

set(SELECTED_VARIANT "${SELECTED_VARIANT}" CACHE STRING "Algorithm variant to build 0=optimized, 1=experimental, 2=baseline")
set_property(CACHE SELECTED_VARIANT PROPERTY STRINGS "0" "1" "2")

# -----------------------------------------------------------------------------
# Define Available Variants
# -----------------------------------------------------------------------------

set(VARIANT_DEFINITIONS
    "0:optimized:src/worker_optimized.cpp"
    "1:experimental:src/worker_experimental.cpp"
    "2:baseline:src/worker_baseline.cpp"
)

# -----------------------------------------------------------------------------
# Dependency: Primesieve (static build)
# -----------------------------------------------------------------------------
include(FetchContent)
message(STATUS "Configuring Primesieve...")

FetchContent_Declare(
    primesieve
    GIT_REPOSITORY https://github.com/kimwalisch/primesieve.git
    GIT_TAG        v12.12
)

# Force Primesieve to build as a static library
set(BUILD_SHARED_LIBS_SAVE ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON  CACHE BOOL "" FORCE)
set(BUILD_TESTS       OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES    OFF CACHE BOOL "" FORCE)
set(BUILD_DOC         OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(primesieve)
set(BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS_SAVE})

# -----------------------------------------------------------------------------
# Required setup
# -----------------------------------------------------------------------------
if(NOT APPLE)
    add_compile_options(-march=native -mtune=native)
endif()
find_package(Threads REQUIRED)

# -----------------------------------------------------------------------------
# Function to create variant-specific builds
# -----------------------------------------------------------------------------

function(apply_lto TARGET_NAME)
    # Enable LTO if available
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_output)
    if(ipo_supported)
        set_property(TARGET ${TARGET_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
endfunction()

function(apply_pgo TARGET_NAME)
    # Apply PGO settings
    if(PGO_GENERATE)
        target_compile_options(${TARGET_NAME} PRIVATE -fprofile-generate)
        target_link_options(${TARGET_NAME} PRIVATE -fprofile-generate)
    elseif(PGO_USE)
        target_compile_options(${TARGET_NAME} PRIVATE -fprofile-use -fprofile-correction)
        target_link_options(${TARGET_NAME} PRIVATE -fprofile-use -fprofile-correction)
    endif()
endfunction()

function(apply_optimizations TARGET_NAME)
    # Apply LTO and PGO settings
    apply_lto(${TARGET_NAME})
    apply_pgo(${TARGET_NAME})
endfunction()

function(apply_static_linking TARGET_NAME)
    # Apply static linking settings
    if(BUILD_FULLY_STATIC)
        target_link_options(${TARGET_NAME} PRIVATE -static)
    else()
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_link_options(${TARGET_NAME} PRIVATE -static-libgcc -static-libstdc++)
        endif()
    endif()
endfunction()

function(add_prime_variant VARIANT_NUM VARIANT_NAME WORKER_SOURCE)
    # Add an algorithm to be generated
    set(CORE_TARGET "prime_core_${VARIANT_NAME}")
    set(EXE_TARGET "prime_differences_${VARIANT_NAME}")

    cmake_path(REPLACE_EXTENSION WORKER_SOURCE ".hpp" OUTPUT_VARIABLE WORKER_HEADER)

    # Core sources (common + variant-specific worker)
    set(CORE_SOURCES
        src/parsing.cpp
        src/state.cpp
        src/sys_info.cpp
        src/utils.cpp
        src/worker.cpp          # Common WorkerBase implementation
        ${WORKER_SOURCE}        # Variant-specific implementation
    )

    set(CORE_HEADERS
        src/bits.hpp
        src/parsing.hpp
        src/state.hpp
        src/sys_info.hpp
        src/types.hpp
        src/utils.hpp
        src/worker.hpp
        src/worker_impl.hpp
    )

    # Add variant-specific header if it exists
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${WORKER_HEADER}")
        list(APPEND CORE_HEADERS ${WORKER_HEADER})
    endif()

    # Add variant-specific header if it exists
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${WORKER_HEADER}")
        list(APPEND CORE_HEADERS ${WORKER_HEADER})
    endif()

    # Create variant-specific OBJECT library
    add_library(${CORE_TARGET} OBJECT ${CORE_SOURCES} ${CORE_HEADERS})

    target_include_directories(${CORE_TARGET} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(${CORE_TARGET} PUBLIC primesieve::primesieve)
    target_compile_features(${CORE_TARGET} PRIVATE cxx_std_23)

    # Get primesieve include dirs and re-add them as SYSTEM includes
    get_target_property(PS_INCLUDES primesieve::primesieve INTERFACE_INCLUDE_DIRECTORIES)
    if(PS_INCLUDES)
        target_include_directories(${CORE_TARGET} SYSTEM PRIVATE ${PS_INCLUDES})
    endif()

    target_compile_options(${CORE_TARGET} PRIVATE
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wall>
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wextra>
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wpedantic>
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wconversion>
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wsign-conversion>
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wshadow>
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wnon-virtual-dtor>
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wold-style-cast>
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-fno-math-errno>

        $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/W3>
        $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/O2>

        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Release>:-O3>
    )

    target_compile_definitions(${CORE_TARGET} PUBLIC SELECTED_VARIANT=${VARIANT_NUM})

    # Create executable
    add_executable(${EXE_TARGET} src/main.cpp)
    target_link_libraries(${EXE_TARGET} PRIVATE ${CORE_TARGET} Threads::Threads)

    apply_static_linking(${EXE_TARGET})

    apply_optimizations(${EXE_TARGET})

    message(STATUS "Added variant: ${VARIANT_NAME} (Variant ${VARIANT_NUM}) using ${WORKER_SOURCE}")

    if(ENABLE_INSTALL)
        install(TARGETS ${EXE_TARGET} DESTINATION bin)
    endif()
    # Export core target for tests/benchmarks
    set(PRIME_CORE_${VARIANT_NUM} ${CORE_TARGET} PARENT_SCOPE)
endfunction()

# -----------------------------------------------------------------------------
# Build Variants
# -----------------------------------------------------------------------------

message(STATUS "═══════════════════════════════════════════════════════")
if(BUILD_ALL_VARIANTS)
    message(STATUS "Building ALL variants")
else()
    message(STATUS "Building variant ${SELECTED_VARIANT}")
endif()
message(STATUS "═══════════════════════════════════════════════════════")

set(BUILT_VARIANTS "")
set(OPTIMIZED_BUILT FALSE)
set(FOUND_SELECTED FALSE)

foreach(VARIANT_DEF ${VARIANT_DEFINITIONS})
    string(REPLACE ":" ";" PARTS ${VARIANT_DEF})
    list(GET PARTS 0 VAR_NUM)
    list(GET PARTS 1 VAR_NAME)
    list(GET PARTS 2 VAR_SOURCE)

    # Build this variant if: building all variants OR this is the selected variant
    if(BUILD_ALL_VARIANTS OR VAR_NUM STREQUAL SELECTED_VARIANT)
        # Check if source file exists
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${VAR_SOURCE}")
            add_prime_variant(${VAR_NUM} ${VAR_NAME} ${VAR_SOURCE})
            list(APPEND BUILT_VARIANTS "prime_differences_${VAR_NAME}")

            # Track if baseline was built
            if(VAR_NAME STREQUAL "optimized")
                set(OPTIMIZED_BUILT TRUE)
            endif()

            # Track if we found the selected variant
            if(VAR_NUM STREQUAL SELECTED_VARIANT)
                set(FOUND_SELECTED TRUE)
            endif()
        else()
            message(STATUS "⊗ Skipping variant ${VAR_NAME} - source file not found: ${VAR_SOURCE}")

            # Fatal error if this was the specifically requested variant
            if(NOT BUILD_ALL_VARIANTS AND VAR_NUM STREQUAL SELECTED_VARIANT)
                message(FATAL_ERROR "Variant ${SELECTED_VARIANT} source file not found: ${VAR_SOURCE}")
            endif()
        endif()
    endif()
endforeach()

# Check that we found the selected variant in single-variant mode
if(NOT BUILD_ALL_VARIANTS AND NOT FOUND_SELECTED)
    message(FATAL_ERROR "Unknown SELECTED_VARIANT: ${SELECTED_VARIANT}")
endif()

# Create convenience targets
if(BUILT_VARIANTS)
    if(BUILD_ALL_VARIANTS)
        add_custom_target(all_variants ALL DEPENDS ${BUILT_VARIANTS})
        message(STATUS "Created target: all_variants (builds all available variants)")
    endif()

    # Copy baseline to standard name if it was built
    if(OPTIMIZED_BUILT)
        if(NOT BUILD_ALL_VARIANTS)
            add_custom_target(prime_differences ALL DEPENDS prime_differences_optimized)
            message(STATUS "✓ Created alias: prime_differences -> prime_differences_optimized")
        endif()

        add_custom_command(TARGET prime_differences_optimized POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                $<TARGET_FILE:prime_differences_optimized>
                ${CMAKE_BINARY_DIR}/prime_differences${CMAKE_EXECUTABLE_SUFFIX}
            COMMENT "Creating prime_differences from prime_differences_optimized"
        )
        message(STATUS "✓ Will copy: prime_differences_optimized -> prime_differences")
    endif()
endif()

message(STATUS "═══════════════════════════════════════════════════════")

# -----------------------------------------------------------------------------
# Tests
# -----------------------------------------------------------------------------

if(BUILD_TESTING)
    message(STATUS "Configuring tests...")
    enable_testing()

    FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG        v2.5.2
    )
    FetchContent_MakeAvailable(doctest)

    # Base test sources (common to all variants)
    set(TEST_SOURCES_BASE
        tests/main_test.cpp
        tests/test_bits.cpp
        tests/test_integration.cpp
        tests/test_parsing.cpp
        tests/test_performance.cpp
        tests/test_state.cpp
        tests/test_utils.cpp
        tests/test_validation.cpp
        tests/test_verification.cpp
    )

    # Build test executable for each built variant
    set(BUILT_TESTS "")

    foreach(VARIANT_DEF ${VARIANT_DEFINITIONS})
        string(REPLACE ":" ";" PARTS ${VARIANT_DEF})
        list(GET PARTS 0 VAR_NUM)
        list(GET PARTS 1 VAR_NAME)

        if(TARGET prime_core_${VAR_NAME})
            set(TEST_TARGET "prime_tests_${VAR_NAME}")

            # Create variant-specific test sources
            set(VARIANT_TEST_SOURCES ${TEST_SOURCES_BASE})

            # Add variant-specific test file if it exists
            if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_${VAR_NAME}.cpp")
                list(APPEND VARIANT_TEST_SOURCES tests/test_${VAR_NAME}.cpp)
                message(STATUS "  Including variant-specific tests: tests/test_${VAR_NAME}.cpp")
            endif()

            add_executable(${TEST_TARGET} ${VARIANT_TEST_SOURCES})
            target_link_libraries(${TEST_TARGET} PRIVATE prime_core_${VAR_NAME} doctest::doctest Threads::Threads)
            target_include_directories(${TEST_TARGET} PRIVATE ${doctest_SOURCE_DIR}/doctest)

            apply_lto(${TEST_TARGET})

            # Add test to CTest
            add_test(${TEST_TARGET} ${TEST_TARGET})

            list(APPEND BUILT_TESTS ${TEST_TARGET})
            message(STATUS "Added test executable: ${TEST_TARGET}")
        endif()
    endforeach()

    # Create convenience target to build all test executables
    if(BUILT_TESTS)
        add_custom_target(prime_tests ALL DEPENDS ${BUILT_TESTS})
        message(STATUS "Created target: prime_tests")
    endif()
endif()

# -----------------------------------------------------------------------------
# Benchmark Executable
# -----------------------------------------------------------------------------

if(BUILD_BENCHMARKS)
    message(STATUS "Configuring benchmarks...")

    # Build benchmark for each built variant
    set(BUILT_BENCHMARKS "")

    foreach(VARIANT_DEF ${VARIANT_DEFINITIONS})
        string(REPLACE ":" ";" PARTS ${VARIANT_DEF})
        list(GET PARTS 0 VAR_NUM)
        list(GET PARTS 1 VAR_NAME)

        if(TARGET prime_core_${VAR_NAME})
            set(BENCH_TARGET "prime_benchmark_${VAR_NAME}")

            add_executable(${BENCH_TARGET} benchmark/benchmark.cpp)
            target_link_libraries(${BENCH_TARGET} PRIVATE prime_core_${VAR_NAME} Threads::Threads)
            target_compile_definitions(${BENCH_TARGET} PRIVATE SELECTED_VARIANT=${VAR_NUM})

            apply_static_linking(${BENCH_TARGET})
            apply_optimizations(${BENCH_TARGET})

            list(APPEND BUILT_BENCHMARKS ${BENCH_TARGET})
            message(STATUS "Added benchmark: ${BENCH_TARGET}")
        endif()
    endforeach()

    # Create convenience target to build all benchmarks
    if(BUILT_BENCHMARKS)
        add_custom_target(prime_benchmarks ALL DEPENDS ${BUILT_BENCHMARKS})
        message(STATUS "Created target: prime_benchmarks")
    endif()
endif()

# -----------------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------------

message(STATUS "═══════════════════════════════════════════════════════")
message(STATUS "Configuration Summary:")
message(STATUS "  Build type:        ${CMAKE_BUILD_TYPE}")
message(STATUS "  CXX Standard:      C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Static linking:    ${BUILD_FULLY_STATIC}")
message(STATUS "  Build tests:       ${BUILD_TESTING}")
message(STATUS "  Build benchmarks:  ${BUILD_BENCHMARKS}")
message(STATUS "  PGO Generate:      ${PGO_GENERATE}")
message(STATUS "  PGO Use:           ${PGO_USE}")
if(BUILD_ALL_VARIANTS)
    message(STATUS "  Mode:              ALL VARIANTS")
else()
    message(STATUS "  Mode:              SINGLE VARIANT (${SELECTED_VARIANT})")
endif()
message(STATUS "═══════════════════════════════════════════════════════")
