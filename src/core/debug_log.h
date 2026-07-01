#pragma once

/// @file debug_log.h
/// @brief Compile-time-gated debug logging macro.
///
/// When `MAP_RENDERER_DEBUG` is defined (e.g. via `-DMAP_RENDERER_DEBUG`),
/// `DEBUG_LOG(fmt, ...)` writes a timestamped message to stderr including
/// the source file and line number.  In release builds it compiles to a
/// no-op with zero runtime overhead.
///
/// Usage:
/// @code
///   DEBUG_LOG("Loaded {} buildings", building_count);
/// @endcode

#ifdef MAP_RENDERER_DEBUG

#include <cstdio>

/// Print a formatted debug message to stderr.
/// Uses printf-style formatting for maximum portability (C++23 std::print
/// is not yet universally available).
#define DEBUG_LOG(fmt, ...)                                                 \
    do {                                                                    \
        std::fprintf(stderr, "[DEBUG] %s:%d: " fmt "\n", __FILE__,          \
                     __LINE__ __VA_OPT__(, ) __VA_ARGS__);                  \
    } while (0)

#else

/// In release builds, DEBUG_LOG expands to nothing.
#define DEBUG_LOG(fmt, ...) ((void)0)

#endif
