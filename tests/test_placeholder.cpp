#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <algorithm>

// Smoke test — verifies the build pipeline is working end-to-end.
// Replace with real unit tests as engine systems are added.

TEST_CASE("clamp stays within bounds", "[math]") {
    REQUIRE(std::clamp(5.f,  0.f, 10.f) == Catch::Approx(5.f));
    REQUIRE(std::clamp(-1.f, 0.f, 10.f) == Catch::Approx(0.f));
    REQUIRE(std::clamp(15.f, 0.f, 10.f) == Catch::Approx(10.f));
}

TEST_CASE("fixed timestep arithmetic", "[loop]") {
    constexpr double FIXED_DT   = 1.0 / 120.0;
    double accumulator = 0.016; // ~one 60fps frame
    int    steps       = 0;
    while (accumulator >= FIXED_DT) { accumulator -= FIXED_DT; ++steps; }
    // One 60fps frame should produce ~1-2 physics steps at 120Hz
    REQUIRE(steps >= 1);
    REQUIRE(steps <= 2);
    REQUIRE(accumulator >= 0.0);
    REQUIRE(accumulator < FIXED_DT);
}
