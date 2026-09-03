#pragma once

#include "quantum.hpp"

namespace sequence {

quantum::ComplexState nextState(const quantum::ComplexState& current,
                                int maximumN = 5);

} // namespace sequence
