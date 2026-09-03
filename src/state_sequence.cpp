#include "state_sequence.hpp"

#include <stdexcept>

namespace sequence {

quantum::ComplexState nextState(const quantum::ComplexState& current,
                                int maximumN)
{
    if (!quantum::isValid(current)
        || maximumN < 1
        || maximumN > 8
        || current.n > maximumN) {
        throw std::invalid_argument("invalid state sequence");
    }

    auto next = current;
    if (next.m < next.l) {
        ++next.m;
    } else if (next.l < next.n - 1) {
        ++next.l;
        next.m = -next.l;
    } else if (next.n < maximumN) {
        ++next.n;
        next.l = 0;
        next.m = 0;
    } else {
        next = {1, 0, 0, current.nuclearCharge};
    }
    return next;
}

} // namespace sequence
