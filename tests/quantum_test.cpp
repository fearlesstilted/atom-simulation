#include "quantum.hpp"

#include <cmath>
#include <complex>
#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void checkNear(double actual, double expected, double tolerance,
               const char* message)
{
    check(std::abs(actual - expected) <= tolerance, message);
}

void checkNearComplex(std::complex<double> actual,
                      std::complex<double> expected,
                      double tolerance,
                      const char* message)
{
    check(std::abs(actual - expected) <= tolerance, message);
}

} // namespace

int main()
{
    using quantum::isValid;

    check(isValid({1, 0, 0, 1}), "hydrogen 1s is valid");
    check(isValid({8, 7, -7, 2}), "upper supported state is valid");
    check(!isValid({0, 0, 0, 1}), "n starts at one");
    check(!isValid({9, 0, 0, 1}), "n stops at eight");
    check(!isValid({2, 2, 0, 1}), "l must be below n");
    check(!isValid({2, 1, 2, 1}), "absolute m cannot exceed l");
    check(!isValid({1, 0, 0, 0}), "nuclear charge is positive");

    constexpr double pi = 3.14159265358979323846;
    checkNear(quantum::probabilityDensity({0, 0, 0}, {1, 0, 0, 1}),
              1.0 / pi, 1e-12, "1s density at origin");
    checkNear(quantum::probabilityDensity({2, 0, 0}, {2, 0, 0, 1}),
              0.0, 1e-12, "2s radial node");
    checkNear(quantum::probabilityDensity({1, 0, 0}, {2, 1, 0, 1}),
              0.0, 1e-12, "2p m0 angular node");
    check(std::isfinite(
              quantum::probabilityDensity({3, -2, 1}, {4, 2, 2, 1})),
          "density remains finite");

    const auto yPositive = quantum::wavefunction({1, 2, 3}, {3, 2, 1, 1});
    const auto yNegative = quantum::wavefunction({1, 2, 3}, {3, 2, -1, 1});
    checkNearComplex(yNegative, -std::conj(yPositive), 1e-12,
                     "negative m conjugation relation");

    return failures == 0 ? 0 : 1;
}
