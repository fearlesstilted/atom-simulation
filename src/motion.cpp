#include "motion.hpp"

#include <cmath>

namespace motion {

CriticalSpringStep::CriticalSpringStep(double angularFrequency,
                                       double deltaTime)
    : angularFrequency_(angularFrequency),
      deltaTime_(deltaTime),
      decay_(std::exp(-angularFrequency * deltaTime))
{
}

SpringState CriticalSpringStep::advance(SpringState state, double target) const
{
    if (angularFrequency_ <= 0.0 || deltaTime_ <= 0.0) return state;

    const double displacement = state.position - target;
    const double impulse = (state.velocity
                            + angularFrequency_ * displacement) * deltaTime_;
    return {
        target + (displacement + impulse) * decay_,
        (state.velocity - angularFrequency_ * impulse) * decay_,
    };
}

} // namespace motion
