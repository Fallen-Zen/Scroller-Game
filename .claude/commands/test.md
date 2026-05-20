---
description: Build and run the test suite
---

Build and run all Catch2 unit tests.

1. Build the test target:
```
cmake --build build --target ScrollerTests -j
```
   If `build/` doesn't exist, run /setup first.

2. Run via CTest for structured output:
```
ctest --test-dir build --output-on-failure -C Debug
```

3. Alternatively, run the binary directly for verbose Catch2 output:
   - macOS/Linux: `./build/tests/ScrollerTests`
   - Windows: `build\tests\Debug\ScrollerTests.exe`

4. Report pass/fail counts. If any test fails, show the failure details and help diagnose.

5. When adding new tests, they go in `tests/` as `test_<system>.cpp` files and are automatically discovered by CMake via `catch_discover_tests`.
