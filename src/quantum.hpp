#pragma once

#include <array>
#include <complex>

namespace quantum {

struct PositionAu {
    double x;
    double y;
    double z;
};

struct ComplexState {
    int n;
    int l;
    int m;
    int nuclearCharge;
};

struct StateTerm {
    ComplexState state;
    std::complex<double> coefficient;
};

struct Superposition {
    std::array<StateTerm, 2> terms;
};

bool isValid(const ComplexState& state);
std::complex<double> wavefunction(PositionAu position,
                                  const ComplexState& state);
double probabilityDensity(PositionAu position,
                          const ComplexState& state);
PositionAu probabilityCurrentVelocity(PositionAu position,
                                      const ComplexState& state);
double energyHartree(const ComplexState& state);
bool isValidSuperposition(const Superposition& state);
Superposition equalSuperposition(const ComplexState& first,
                                 const ComplexState& second);
std::complex<double> wavefunction(PositionAu position,
                                  const Superposition& state,
                                  double timeAu);
double probabilityDensity(PositionAu position,
                          const Superposition& state,
                          double timeAu);

} // namespace quantum
