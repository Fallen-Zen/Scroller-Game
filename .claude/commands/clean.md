---
description: Delete build directories (build/, build-xcode/, build-vs/)
---

Clean all generated build artifacts.

1. Ask the user which build directories to remove:
   - `build/`       — default CMake build
   - `build-xcode/` — Xcode project
   - `build-vs/`    — Visual Studio solution
   - All of the above

2. Confirm before deleting (these directories can take several minutes to regenerate because SDL2 compiles from source via FetchContent).

3. Delete the chosen directories using `rm -rf` (macOS/Linux) or `Remove-Item -Recurse` (Windows).

4. After cleaning, tell the user to run /setup to reconfigure.
