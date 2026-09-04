#include "motion.hpp"

#include <cmath>
#include <iostream>

int main()
{
    motion::SpringState singleStep{8.0, 0.0};
    motion::SpringState manySteps = singleStep;

    singleStep = motion::CriticalSpringStep(0.85, 1.0).advance(singleStep, -3.0);
    const motion::CriticalSpringStep frameStep(0.85, 1.0 / 60.0);
    for (int frame = 0; frame < 60; ++frame) {
        manySteps = frameStep.advance(manySteps, -3.0);
    }

    if (std::abs(singleStep.position - manySteps.position) > 1e-10
        || std::abs(singleStep.velocity - manySteps.velocity) > 1e-10) {
        std::cerr << "FAIL: spring motion depends on frame rate\n";
        return 1;
    }
    if (!(manySteps.position < 8.0 && manySteps.position > -3.0)) {
        std::cerr << "FAIL: spring did not approach its target without overshoot\n";
        return 1;
    }
    return 0;
}
