---
description: Generate an Xcode project (macOS only)
---

Generate an Xcode project for the Scroller game.

1. Verify we're on macOS. If not, tell the user this command is macOS-only.

2. Create the Xcode build directory and generate:
```
cmake -S . -B build-xcode -G Xcode
```

3. On success, open the project:
```
open build-xcode/Scroller.xcodeproj
```

4. Remind the user:
   - Select the "Scroller" scheme in Xcode (top-left dropdown)
   - Press Cmd+R to build and run
   - The `build-xcode/` directory is gitignored — regenerate it any time with /xcode
   - Don't edit CMakeLists.txt from inside Xcode; edit the source files directly and re-run /xcode if you add/remove files

5. If Xcode isn't installed, direct the user to the Mac App Store.
