#include "quantum.hpp"

#include <array>
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

struct IntegralResult {
    double normalization;
    double meanRadius;
};

IntegralResult integrate(const quantum::ComplexState& state)
{
    constexpr double pi = 3.14159265358979323846;
    constexpr int radialBins = 320;
    constexpr int cosineBins = 48;
    constexpr int azimuthBins = 64;

    const double rMax = 8.0 * state.n * state.n / state.nuclearCharge;
    const double dr = rMax / radialBins;
    const double dCosTheta = 2.0 / cosineBins;
    const double dPhi = 2.0 * pi / azimuthBins;
    double normalization = 0.0;
    double radiusMoment = 0.0;

    for (int ir = 0; ir < radialBins; ++ir) {
        const double r = (ir + 0.5) * dr;
        for (int it = 0; it < cosineBins; ++it) {
            const double cosTheta = -1.0 + (it + 0.5) * dCosTheta;
            const double sinTheta = std::sqrt(1.0 - cosTheta * cosTheta);
            for (int ip = 0; ip < azimuthBins; ++ip) {
                const double phi = (ip + 0.5) * dPhi;
                const quantum::PositionAu position{
                    r * sinTheta * std::cos(phi),
                    r * sinTheta * std::sin(phi),
                    r * cosTheta,
                };
                const double mass = quantum::probabilityDensity(position, state)
                    * r * r * dr * dCosTheta * dPhi;
                normalization += mass;
                radiusMoment += r * mass;
            }
        }
    }

    return {normalization, radiusMoment / normalization};
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

    constexpr std::array states{
        quantum::ComplexState{1, 0, 0, 1},
        quantum::ComplexState{2, 0, 0, 1},
        quantum::ComplexState{2, 1, 0, 1},
        quantum::ComplexState{4, 2, 2, 1},
    };
    for (const auto& state : states) {
        const auto integral = integrate(state);
        const double expectedRadius =
            (3.0 * state.n * state.n - state.l * (state.l + 1.0))
            / (2.0 * state.nuclearCharge);
        checkNear(integral.normalization, 1.0, 0.02,
                  "state is normalized");
        checkNear(integral.meanRadius, expectedRadius,
                  0.05 * expectedRadius,
                  "mean radius matches hydrogenic expectation");
    }

    return failures == 0 ? 0 : 1;
}
