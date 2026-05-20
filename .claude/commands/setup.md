---
description: Configure CMake build (auto-detects platform, creates build/ directory)
---

Configure the CMake build for the current platform. Run these steps:

1. Detect the OS. On macOS use the Ninja or Unix Makefiles generator. On Windows use Visual Studio (see /vs for that). On Linux use Ninja or Unix Makefiles.

2. Run:
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
```

3. If cmake isn't found, tell the user to install it:
   - macOS: `brew install cmake`
   - Windows: download from cmake.org
   - Linux: `sudo apt install cmake`

4. Report what generator was chosen and whether configuration succeeded. If it failed, show the error and suggest a fix.

The build/ directory is the default target for /build and /run commands.
