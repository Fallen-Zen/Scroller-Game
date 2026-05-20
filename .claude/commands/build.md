---
description: Build the game (Debug by default, pass "release" for Release build)
---

Build the Scroller game. Steps:

1. Check that `build/` exists. If not, run /setup first and tell the user.

2. Build with:
```
cmake --build build --config Debug -j
```
   If the user passed "release" as an argument, use `--config Release` instead.

3. On success, tell the user the executable path:
   - macOS/Linux: `build/Scroller`
   - Windows: `build\Debug\Scroller.exe`

4. On failure, parse the compiler errors and suggest fixes. Common issues:
   - Missing SDL2: FetchContent should handle this automatically. If it failed, check internet connectivity.
   - C++ version error: ensure CMakeLists.txt has `set(CMAKE_CXX_STANDARD 17)`
