// =============================================================================
// Logger.h — Lightweight game logger
//
// Provides four log levels, relative timestamps, coloured console output, and
// an optional log file. Use the macros at the bottom — they are shorter to
// type and automatically strip Debug-level calls from Release builds.
//
// Typical usage:
//
//   Logger::init();                       // call once, early in Game ctor
//
//   LOG_INFO("Window created: %dx%d", w, h);
//   LOG_DEBUG("Fixed timestep: %.1f Hz", 1.0 / FIXED_DT);
//   LOG_WARN("No gamepad detected");
//   LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
//
//   Logger::shutdown();                   // call in Game dtor
// =============================================================================

#pragma once
#include <cstdarg>  // va_list for printf-style formatting

// -----------------------------------------------------------------------------
// LogLevel — severity ordering (higher value = more severe)
// -----------------------------------------------------------------------------
enum class LogLevel {
    Debug   = 0,  // Verbose developer detail; stripped from Release builds
    Info    = 1,  // Normal lifecycle events (window created, loop started…)
    Warning = 2,  // Non-fatal problems (no gamepad, slow frame…)
    Error   = 3,  // Serious failures; game may not recover
};

// -----------------------------------------------------------------------------
// Logger — all-static class (essentially a global service with state)
//
// Thread safety: NOT thread-safe. This is a single-threaded game — if a second
// thread is added later, wrap the write with a std::mutex.
// -----------------------------------------------------------------------------
class Logger {
public:
    // Open the log file and record the start time used for timestamps.
    // logFilePath defaults to "scroller.log" next to the working directory.
    // Safe to call multiple times — subsequent calls are ignored.
    static void init(const char* logFilePath = "scroller.log");

    // Flush and close the log file. Call in Game::~Game().
    static void shutdown();

    // Set the lowest level that will actually be written. Messages below this
    // are discarded with zero overhead after the comparison.
    //   Debug builds default to LogLevel::Debug (see Logger.cpp).
    //   Release builds default to LogLevel::Info.
    static void setMinLevel(LogLevel level);

    // Write a formatted message. Prefer the macros below.
    // printf-style format string and variadic arguments.
    static void log(LogLevel level, const char* fmt, ...);

    // Variadic-list version — useful if you build your own wrapper.
    static void logV(LogLevel level, const char* fmt, va_list args);

private:
    Logger() = delete; // purely static — never instantiated
};

// -----------------------------------------------------------------------------
// Convenience macros
//
// LOG_DEBUG is compiled out entirely in Release builds (NDEBUG defined).
// The others are always active so warnings and errors are never silently lost.
//
// The do { } while(0) wrapper makes the macro safe to use inside an if without
// braces:
//   if (failed) LOG_ERROR("...");   ← expands correctly
// -----------------------------------------------------------------------------
#ifndef NDEBUG
#  define LOG_DEBUG(...) Logger::log(LogLevel::Debug,   __VA_ARGS__)
#else
#  define LOG_DEBUG(...) do {} while(0)   // zero-cost in Release
#endif

#define LOG_INFO(...)  Logger::log(LogLevel::Info,    __VA_ARGS__)
#define LOG_WARN(...)  Logger::log(LogLevel::Warning, __VA_ARGS__)
#define LOG_ERROR(...) Logger::log(LogLevel::Error,   __VA_ARGS__)
