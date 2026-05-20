// =============================================================================
// main.cpp — Entry point
//
// Responsibility: construct the Game object and hand control to it.
// Keeping main.cpp thin means all real logic lives in Game, which makes it
// easier to test and to swap platform details later.
// =============================================================================

#include "core/Game.h"
#include <SDL.h>
#include <iostream>

// On Windows, SDL2 replaces the standard `main` with its own `SDL_main` via a
// preprocessor macro so it can perform platform setup before your code runs.
// Including SDL_main.h activates that rename. On macOS/Linux the header exists
// but the macro is a no-op, so it is safe to include unconditionally behind the
// guard — we guard anyway to make the intent explicit.
#ifdef _WIN32
#  include <SDL_main.h>
#endif

// SDL2 requires main to accept (int, char**) even if you don't use the
// arguments, because on some platforms (Android, Windows) SDL intercepts them.
// The /*name*/ syntax suppresses "unused parameter" compiler warnings without
// removing the signature SDL expects.
int main(int /*argc*/, char* /*argv*/[]) {
    try {
        // GameConfig uses default values (1280×720, vsync on, title "Scroller").
        // Pass a custom GameConfig{} struct here if you ever need to change them
        // at startup — e.g. from a settings file or command-line flag.
        Game game;

        // run() blocks until the player quits, then returns 0 (success) or a
        // non-zero error code that the OS can inspect (e.g. via `echo $?`).
        return game.run();

    } catch (const std::exception& ex) {
        // Any unrecoverable error in the Game constructor or run() is thrown as
        // a std::exception (usually std::runtime_error wrapping SDL_GetError()).
        // We surface it two ways:
        //   1. SDL message box — visible even if the terminal is hidden (common
        //      when shipping a .app bundle or .exe without a console).
        //   2. stderr — useful when running from a terminal or CI pipeline.
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR, "Fatal error", ex.what(), nullptr);
        std::cerr << "Fatal: " << ex.what() << '\n';
        return 1;
    }
}
