#pragma once

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

bool isValid(const ComplexState& state);
std::complex<double> wavefunction(PositionAu position,
                                  const ComplexState& state);
double probabilityDensity(PositionAu position,
                          const ComplexState& state);

} // namespace quantum
