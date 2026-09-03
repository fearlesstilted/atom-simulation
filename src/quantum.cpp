#include "quantum.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <cstdlib>

namespace quantum {

bool isValid(const ComplexState& state)
{
    return state.n >= 1 && state.n <= 8
        && state.l >= 0 && state.l < state.n
        && std::abs(state.m) <= state.l
        && state.nuclearCharge >= 1;
}

std::complex<double> wavefunction(PositionAu position,
                                  const ComplexState& state)
{
    constexpr double pi = 3.14159265358979323846;

    const double r = std::sqrt(
        position.x * position.x
        + position.y * position.y
        + position.z * position.z
    );
    const double cosTheta = r == 0.0
        ? 1.0
        : std::clamp(position.z / r, -1.0, 1.0);
    const double phi = std::atan2(position.y, position.x);

    const double n = state.n;
    const double z = state.nuclearCharge;
    const int l = state.l;
    const int absM = std::abs(state.m);
    const double rho = 2.0 * z * r / n;

    const double radialNorm = std::sqrt(
        std::pow(2.0 * z / n, 3)
        * std::tgamma(state.n - l)
        / (2.0 * n * std::tgamma(state.n + l + 1.0))
    );
    const double radial = radialNorm * std::exp(-rho / 2.0)
        * std::pow(rho, l)
        * std::assoc_laguerre(state.n - l - 1, 2 * l + 1, rho);

    const double angularNorm = std::sqrt(
        (2.0 * l + 1.0) / (4.0 * pi)
        * std::tgamma(l - absM + 1.0)
        / std::tgamma(l + absM + 1.0)
    );
    const double magnitude = angularNorm
        * std::assoc_legendre(l, absM, cosTheta);
    const double condonShortley = absM % 2 == 0 ? 1.0 : -1.0;
    const auto positive = condonShortley * magnitude
        * std::polar(1.0, absM * phi);
    const auto angular = state.m >= 0
        ? positive
        : condonShortley * std::conj(positive);

    return radial * angular;
}

double probabilityDensity(PositionAu position, const ComplexState& state)
{
    return std::norm(wavefunction(position, state));
}

PositionAu probabilityCurrentVelocity(PositionAu position,
                                      const ComplexState& state)
{
    const double cylindricalRadiusSquared =
        position.x * position.x + position.y * position.y;
    if (state.m == 0 || cylindricalRadiusSquared < 1e-20) {
        return {0.0, 0.0, 0.0};
    }

    const double scale = state.m / cylindricalRadiusSquared;
    return {-scale * position.y, scale * position.x, 0.0};
}

double energyHartree(const ComplexState& state)
{
    const double z = state.nuclearCharge;
    const double n = state.n;
    return -(z * z) / (2.0 * n * n);
}

bool isValidSuperposition(const Superposition& state)
{
    const auto& first = state.terms[0];
    const auto& second = state.terms[1];
    const bool distinct = first.state.n != second.state.n
        || first.state.l != second.state.l
        || first.state.m != second.state.m;
    const double coefficientNorm = std::norm(first.coefficient)
        + std::norm(second.coefficient);
    const bool finiteCoefficients =
        std::isfinite(first.coefficient.real())
        && std::isfinite(first.coefficient.imag())
        && std::isfinite(second.coefficient.real())
        && std::isfinite(second.coefficient.imag());
    return isValid(first.state)
        && isValid(second.state)
        && first.state.nuclearCharge == second.state.nuclearCharge
        && distinct
        && finiteCoefficients
        && std::abs(coefficientNorm - 1.0) < 1e-12;
}

Superposition equalSuperposition(const ComplexState& first,
                                 const ComplexState& second)
{
    constexpr double inverseRootTwo = 0.70710678118654752440;
    Superposition state{
        std::array<StateTerm, 2>{
            StateTerm{first, {inverseRootTwo, 0.0}},
            StateTerm{second, {inverseRootTwo, 0.0}},
        }
    };
    if (!isValidSuperposition(state)) {
        throw std::invalid_argument("invalid superposition states");
    }
    return state;
}

std::complex<double> wavefunction(PositionAu position,
                                  const Superposition& state,
                                  double timeAu)
{
    std::complex<double> amplitude{0.0, 0.0};
    for (const auto& term : state.terms) {
        const auto timePhase = std::polar(
            1.0, -energyHartree(term.state) * timeAu);
        amplitude += term.coefficient
            * wavefunction(position, term.state)
            * timePhase;
    }
    return amplitude;
}

double probabilityDensity(PositionAu position,
                          const Superposition& state,
                          double timeAu)
{
    return std::norm(wavefunction(position, state, timeAu));
}

} // namespace quantum
