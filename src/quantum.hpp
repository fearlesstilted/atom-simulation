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

enum class RealOrbital {
    Px,
    Py,
    Pz,
    Dxy,
    Dxz,
    Dyz,
    Dz2,
    Dx2Y2,
};

struct RealState {
    int n;
    RealOrbital orbital;
    int nuclearCharge;
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
PositionAu probabilityCurrentVelocity(PositionAu position,
                                      const Superposition& state,
                                      double timeAu);
bool isValid(const RealState& state);
const char* name(RealOrbital orbital);
std::complex<double> wavefunction(PositionAu position,
                                  const RealState& state);
double probabilityDensity(PositionAu position, const RealState& state);
PositionAu probabilityCurrentVelocity(PositionAu position,
                                      const RealState& state);

} // namespace quantum
