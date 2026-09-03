#pragma once

#include "quantum.hpp"

#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

namespace sampling {

struct SamplerConfig {
    std::size_t walkerCount = 30000;
    std::size_t updatesPerAdvance = 768;
    std::uint32_t seed = 42;
    double stepScale = 0.60;
    double flowTimeScale = 1.0;
};

struct Walker {
    quantum::PositionAu position;
    double phase;
};

struct Diagnostics {
    std::uint64_t attempted = 0;
    std::uint64_t accepted = 0;

    double acceptanceRate() const;
};

class Sampler {
public:
    explicit Sampler(const quantum::ComplexState& state,
                     SamplerConfig config = {});

    void reset(const quantum::ComplexState& state);
    void advance();
    const std::vector<Walker>& walkers() const;
    Diagnostics diagnostics() const;

private:
    quantum::PositionAu proposalMean(quantum::PositionAu position) const;
    void advanceWalker(Walker& walker);
    void applyProbabilityFlow(Walker& walker) const;
    void initializeWalkers();

    quantum::ComplexState state_;
    SamplerConfig config_;
    std::mt19937 random_;
    std::vector<Walker> walkers_;
    std::size_t nextWalker_ = 0;
    Diagnostics diagnostics_;
};

} // namespace sampling
