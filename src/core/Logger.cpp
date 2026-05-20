// =============================================================================
// Logger.cpp
// =============================================================================

#include "core/Logger.h"
#include <cstdio>    // fprintf, vsnprintf, fopen, fflush, fclose
#include <cstring>   // strlen
#include <chrono>    // steady_clock for timestamps
#include <mutex>     // reserved for future thread-safety (unused for now)
#ifndef _WIN32
#  include <unistd.h>  // isatty, STDERR_FILENO
#endif

// =============================================================================
// Internal state  (anonymous namespace = file-private, no external linkage)
// =============================================================================
namespace {

// Wall-clock point recorded in Logger::init() — all timestamps are relative to
// this so the log reads "seconds since game start" rather than epoch time.
std::chrono::steady_clock::time_point g_startTime;

// Minimum level filter. Messages below this are discarded immediately.
LogLevel g_minLevel =
#ifdef NDEBUG
    LogLevel::Info;   // Release: suppress Debug noise
#else
    LogLevel::Debug;  // Debug: show everything
#endif

// File handle for the log file. nullptr until init() is called.
std::FILE* g_file = nullptr;

// Set to true after init() so the second call is ignored.
bool g_initialised = false;

// ── ANSI terminal colours ─────────────────────────────────────────────────────
// These escape codes are understood by macOS Terminal, iTerm2, VS Code's
// integrated terminal, and most Linux terminals. They have no effect on plain
// text files — we only inject them into the console output path, not the file.
//
// \033[ = ESC [ (Control Sequence Introducer)
// The number selects a colour:  33 = yellow, 31 = red, 36 = cyan, 0 = reset
//
// Checked at runtime via isatty() so that piped output (Xcode debug console,
// CI log files, redirected stderr) never sees raw escape bytes.
// isatty() returns 1 when the file descriptor is connected to a real terminal
// (TTY), and 0 when it is a pipe or file — exactly the condition we need.
bool g_ansiEnabled = false;

const char* levelColour(LogLevel level) {
    if (!g_ansiEnabled) return "";
    switch (level) {
    case LogLevel::Debug:   return "\033[36m";  // cyan
    case LogLevel::Info:    return "\033[0m";   // default (white/grey)
    case LogLevel::Warning: return "\033[33m";  // yellow
    case LogLevel::Error:   return "\033[31m";  // red
    }
    return "\033[0m";
}

const char* ansiReset() { return g_ansiEnabled ? "\033[0m" : ""; }

// ── Level label ───────────────────────────────────────────────────────────────
// Five characters wide so columns line up:
//   [DEBUG] [INFO ] [WARN ] [ERROR]
const char* levelLabel(LogLevel level) {
    switch (level) {
    case LogLevel::Debug:   return "DEBUG";
    case LogLevel::Info:    return "INFO ";
    case LogLevel::Warning: return "WARN ";
    case LogLevel::Error:   return "ERROR";
    }
    return "?????";
}

// ── Format timestamp ──────────────────────────────────────────────────────────
// Returns seconds elapsed since g_startTime, formatted as "   1.234s".
// The fixed-width field keeps all log lines left-aligned regardless of uptime.
double elapsedSeconds() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now - g_startTime).count();
}

} // namespace


// =============================================================================
// Public API
// =============================================================================

void Logger::init(const char* logFilePath) {
    if (g_initialised) return;
    g_initialised = true;

    g_startTime = std::chrono::steady_clock::now();

    // Enable ANSI colour only when stderr is a real colour-capable terminal.
    //
    // isatty() alone is not enough — Xcode allocates a pseudo-TTY for its
    // debug console so isatty() returns 1, but the console can't render ANSI
    // escape codes and displays them as raw text instead.
    //
    // The extra signal: real terminal emulators (Terminal.app, iTerm2, etc.)
    // always set the TERM environment variable (e.g. "xterm-256color"). Xcode
    // leaves it unset. Requiring TERM to be present and non-empty catches that
    // case without hard-coding anything Xcode-specific.
#ifdef _WIN32
    g_ansiEnabled = false;  // Windows needs a different colour API
#else
    const char* term = getenv("TERM");
    bool isTTY       = (isatty(STDERR_FILENO) == 1);
    bool termIsColor = (term != nullptr) && (term[0] != '\0') && (strcmp(term, "dumb") != 0);
    g_ansiEnabled    = isTTY && termIsColor;
#endif

    // Open the log file in write mode (truncates any previous session's log).
    // Use "a" instead of "w" if you want logs to accumulate across runs.
    g_file = std::fopen(logFilePath, "w");
    // If fopen fails (e.g. read-only filesystem) we silently continue —
    // console output still works and the game runs normally.

    // First line in both console and file so it's easy to find the session start.
    LOG_INFO("Logger initialised — writing to %s", logFilePath);
}

void Logger::shutdown() {
    LOG_INFO("Logger shutting down");
    if (g_file) {
        std::fflush(g_file);
        std::fclose(g_file);
        g_file = nullptr;
    }
    g_initialised = false;
}

void Logger::setMinLevel(LogLevel level) {
    g_minLevel = level;
}

// =============================================================================
// Core write path
// =============================================================================

void Logger::logV(LogLevel level, const char* fmt, va_list args) {
    // Lazy init: if something logs before Logger::init() is explicitly called
    // (e.g. a member-variable constructor that runs before Game::Game() body),
    // auto-initialise with defaults so the timestamp and file are correct.
    if (!g_initialised) init();

    // Fast early-out: discard anything below the current minimum level.
    // In Release builds LOG_DEBUG expands to nothing, but this guard also
    // handles runtime level changes (e.g. a debug console command).
    if (level < g_minLevel) return;

    // ── Format the caller's message into a stack buffer ───────────────────────
    // 1 KB is plenty for a game log line. vsnprintf always null-terminates and
    // never overflows the buffer (it just truncates and returns the would-be
    // length). Using the stack avoids heap allocation on every log call.
    char msg[1024];
    std::vsnprintf(msg, sizeof(msg), fmt, args);

    double t = elapsedSeconds();

    // ── Console output (stderr) ───────────────────────────────────────────────
    // We use stderr rather than stdout so log lines aren't mixed into any
    // piped stdout output (e.g. if the game ever prints data for a tool).
    std::fprintf(stderr,
        "%s[%8.3fs] [%s] %s%s\n",
        levelColour(level),  // empty string when ANSI is off
        t,
        levelLabel(level),
        msg,
        ansiReset()          // empty string when ANSI is off
    );

    // ── File output (plain text, no ANSI) ────────────────────────────────────
    if (g_file) {
        std::fprintf(g_file,
            "[%8.3fs] [%s] %s\n",
            t,
            levelLabel(level),
            msg
        );
        // Always flush so the file is up to date even if the process is killed
        // by a signal (SIGTERM, crash) before the destructor runs.
        std::fflush(g_file);
    }
}

void Logger::log(LogLevel level, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logV(level, fmt, args);
    va_end(args);
}
