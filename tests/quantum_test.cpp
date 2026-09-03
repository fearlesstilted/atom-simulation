#include "quantum.hpp"

#include <algorithm>
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

double integrateSuperposition(const quantum::Superposition& state,
                              double timeAu)
{
    constexpr double pi = 3.14159265358979323846;
    constexpr int radialBins = 240;
    constexpr int cosineBins = 40;
    constexpr int azimuthBins = 48;
    const int maximumN = std::max(state.terms[0].state.n,
                                  state.terms[1].state.n);
    const double dr = 8.0 * maximumN * maximumN / radialBins;
    const double dCosTheta = 2.0 / cosineBins;
    const double dPhi = 2.0 * pi / azimuthBins;
    double normalization = 0.0;

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
                normalization += quantum::probabilityDensity(
                    position, state, timeAu) * r * r
                    * dr * dCosTheta * dPhi;
            }
        }
    }
    return normalization;
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

    const auto still = quantum::probabilityCurrentVelocity(
        {1, 2, 3}, {3, 2, 0, 1});
    checkNear(still.x, 0.0, 1e-12, "m0 current has no x velocity");
    checkNear(still.y, 0.0, 1e-12, "m0 current has no y velocity");
    checkNear(still.z, 0.0, 1e-12, "m0 current has no z velocity");

    const auto positiveFlow = quantum::probabilityCurrentVelocity(
        {1, 2, 3}, {3, 2, 1, 1});
    const auto negativeFlow = quantum::probabilityCurrentVelocity(
        {1, 2, 3}, {3, 2, -1, 1});
    checkNear(positiveFlow.x * 1.0 + positiveFlow.y * 2.0,
              0.0, 1e-12, "probability flow is tangential");
    checkNear(negativeFlow.x, -positiveFlow.x, 1e-12,
              "negative m reverses x flow");
    checkNear(negativeFlow.y, -positiveFlow.y, 1e-12,
              "negative m reverses y flow");

    const auto axisFlow = quantum::probabilityCurrentVelocity(
        {0, 0, 2}, {3, 2, 1, 1});
    check(std::isfinite(axisFlow.x) && std::isfinite(axisFlow.y),
          "current remains finite on axis");

    checkNear(quantum::energyHartree({1, 0, 0, 1}), -0.5, 1e-12,
              "hydrogen 1s energy");
    checkNear(quantum::energyHartree({2, 0, 0, 1}), -0.125, 1e-12,
              "hydrogen n2 energy");

    const auto superposition = quantum::equalSuperposition(
        {1, 0, 0, 1}, {2, 1, 0, 1});
    check(quantum::isValidSuperposition(superposition),
          "equal superposition is valid");
    checkNear(integrateSuperposition(superposition, 0.0),
              1.0, 0.025, "superposition normalized at t0");
    const double halfBeat = pi
        / std::abs(quantum::energyHartree({2, 1, 0, 1})
                   - quantum::energyHartree({1, 0, 0, 1}));
    checkNear(integrateSuperposition(superposition, halfBeat),
              1.0, 0.025, "superposition stays normalized over time");
    const double densityNow = quantum::probabilityDensity(
        {0, 0, 1}, superposition, 0.0);
    const double densityLater = quantum::probabilityDensity(
        {0, 0, 1}, superposition, halfBeat);
    check(std::abs(densityNow - densityLater) > 0.01,
          "interference changes density over time");

    return failures == 0 ? 0 : 1;
}
