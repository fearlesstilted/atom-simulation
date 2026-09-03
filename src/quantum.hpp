#pragma once

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

} // namespace quantum
