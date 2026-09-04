#pragma once

namespace motion {

struct SpringState {
    double position;
    double velocity;
};

class CriticalSpringStep {
public:
    CriticalSpringStep(double angularFrequency, double deltaTime);
    SpringState advance(SpringState state, double target) const;

private:
    double angularFrequency_;
    double deltaTime_;
    double decay_;
};

} // namespace motion
