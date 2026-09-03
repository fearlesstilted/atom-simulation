#include "quantum.hpp"

#include <cstdlib>

namespace quantum {

bool isValid(const ComplexState& state)
{
    return state.n >= 1 && state.n <= 8
        && state.l >= 0 && state.l < state.n
        && std::abs(state.m) <= state.l
        && state.nuclearCharge >= 1;
}

} // namespace quantum
