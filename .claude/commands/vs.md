---
description: Generate a Visual Studio solution (Windows only)
---

Generate a Visual Studio solution for the Scroller game.

1. Verify we're on Windows. If not, tell the user this command is Windows-only.

2. Detect the installed Visual Studio version (prefer the latest):
   - VS 2022: `-G "Visual Studio 17 2022"`
   - VS 2019: `-G "Visual Studio 16 2019"`
   - VS 2017: `-G "Visual Studio 15 2017"`

3. Generate:
```
cmake -S . -B build-vs -G "Visual Studio 17 2022" -A x64
```

4. Open the solution:
```
start build-vs\Scroller.sln
```

5. Remind the user:
   - Set "Scroller" as the startup project (right-click → Set as Startup Project)
   - Press F5 to build and run with debugger
   - SDL2 DLLs are automatically copied next to the .exe by CMake
   - The `build-vs/` directory is gitignored — regenerate with /vs if you add/remove files

6. If Visual Studio isn't installed, link to: https://visualstudio.microsoft.com/downloads/
