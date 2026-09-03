#include "sampler.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace sampling {
namespace {

double logDensity(quantum::PositionAu position,
                  const quantum::ComplexState& state)
{
    constexpr double floor = 1e-300;
    return std::log(std::max(
        quantum::probabilityDensity(position, state), floor));
}

quantum::PositionAu subtract(quantum::PositionAu a, quantum::PositionAu b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

double lengthSquared(quantum::PositionAu value)
{
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

double phaseAt(quantum::PositionAu position,
               const quantum::ComplexState& state)
{
    return std::arg(quantum::wavefunction(position, state));
}

} // namespace

double Diagnostics::acceptanceRate() const
{
    return attempted == 0
        ? 0.0
        : static_cast<double>(accepted) / attempted;
}

Sampler::Sampler(const quantum::ComplexState& state, SamplerConfig config)
    : state_(state), config_(config), random_(config.seed)
{
    if (!quantum::isValid(state)
        || config.walkerCount == 0
        || config.updatesPerAdvance == 0
        || config.stepScale <= 0.0
        || config.currentStrength < 0.0) {
        throw std::invalid_argument("invalid sampler configuration");
    }
    initializeWalkers();
}

void Sampler::reset(const quantum::ComplexState& state)
{
    if (!quantum::isValid(state)) {
        throw std::invalid_argument("invalid quantum state");
    }
    state_ = state;
    random_.seed(config_.seed);
    nextWalker_ = 0;
    diagnostics_ = {};
    initializeWalkers();
}

const std::vector<Walker>& Sampler::walkers() const
{
    return walkers_;
}

Diagnostics Sampler::diagnostics() const
{
    return diagnostics_;
}

quantum::PositionAu Sampler::proposalMean(
    quantum::PositionAu position) const
{
    const double scale = static_cast<double>(state_.n * state_.n)
        / state_.nuclearCharge;
    const double delta = 0.002 * scale;
    const double xPlus = logDensity({position.x + delta, position.y, position.z}, state_);
    const double xMinus = logDensity({position.x - delta, position.y, position.z}, state_);
    const double yPlus = logDensity({position.x, position.y + delta, position.z}, state_);
    const double yMinus = logDensity({position.x, position.y - delta, position.z}, state_);
    const double zPlus = logDensity({position.x, position.y, position.z + delta}, state_);
    const double zMinus = logDensity({position.x, position.y, position.z - delta}, state_);
    quantum::PositionAu drift{
        (xPlus - xMinus) / (2.0 * delta),
        (yPlus - yMinus) / (2.0 * delta),
        (zPlus - zMinus) / (2.0 * delta),
    };

    const double step = config_.stepScale * state_.n / state_.nuclearCharge;
    const double dt = step * step;
    const auto current = quantum::probabilityCurrentVelocity(position, state_);
    drift.x = dt * (0.5 * drift.x + config_.currentStrength * current.x);
    drift.y = dt * (0.5 * drift.y + config_.currentStrength * current.y);
    drift.z = dt * (0.5 * drift.z + config_.currentStrength * current.z);

    const double driftLength = std::sqrt(lengthSquared(drift));
    const double maxDrift = 2.0 * step;
    if (driftLength > maxDrift) {
        const double factor = maxDrift / driftLength;
        drift.x *= factor;
        drift.y *= factor;
        drift.z *= factor;
    }

    return {position.x + drift.x,
            position.y + drift.y,
            position.z + drift.z};
}

void Sampler::advanceWalker(Walker& walker)
{
    const double step = config_.stepScale * state_.n / state_.nuclearCharge;
    std::normal_distribution<double> noise(0.0, step);
    std::uniform_real_distribution<double> unit(0.0, 1.0);

    const auto currentMean = proposalMean(walker.position);
    const quantum::PositionAu candidate{
        currentMean.x + noise(random_),
        currentMean.y + noise(random_),
        currentMean.z + noise(random_),
    };
    const auto candidateMean = proposalMean(candidate);

    const double logForward = -lengthSquared(subtract(candidate, currentMean))
        / (2.0 * step * step);
    const double logReverse = -lengthSquared(subtract(walker.position, candidateMean))
        / (2.0 * step * step);
    const double logAcceptance = logDensity(candidate, state_)
        - logDensity(walker.position, state_)
        + logReverse - logForward;

    ++diagnostics_.attempted;
    if (std::log(unit(random_)) < std::min(0.0, logAcceptance)) {
        walker = {candidate, phaseAt(candidate, state_)};
        ++diagnostics_.accepted;
    }
}

void Sampler::advance()
{
    const std::size_t count = std::min(
        config_.updatesPerAdvance, walkers_.size());
    for (std::size_t i = 0; i < count; ++i) {
        advanceWalker(walkers_[nextWalker_]);
        nextWalker_ = (nextWalker_ + 1) % walkers_.size();
    }
}

void Sampler::initializeWalkers()
{
    const double orbitalScale = static_cast<double>(state_.n * state_.n)
        / state_.nuclearCharge;
    std::normal_distribution<double> initial(0.0, 0.5 * orbitalScale);
    std::normal_distribution<double> step(
        0.0, 0.75 * state_.n / state_.nuclearCharge);
    std::uniform_real_distribution<double> unit(0.0, 1.0);

    quantum::PositionAu current{};
    do {
        current = {initial(random_), initial(random_), initial(random_)};
    } while (logDensity(current, state_) < -600.0);
    double currentLogDensity = logDensity(current, state_);

    walkers_.clear();
    walkers_.reserve(config_.walkerCount);
    constexpr int burnIn = 4000;
    constexpr int thinning = 6;
    int iteration = 0;
    while (walkers_.size() < config_.walkerCount) {
        const quantum::PositionAu candidate{
            current.x + step(random_),
            current.y + step(random_),
            current.z + step(random_),
        };
        const double candidateLogDensity = logDensity(candidate, state_);
        if (std::log(unit(random_))
            < std::min(0.0, candidateLogDensity - currentLogDensity)) {
            current = candidate;
            currentLogDensity = candidateLogDensity;
        }

        ++iteration;
        if (iteration > burnIn && iteration % thinning == 0) {
            walkers_.push_back({current, phaseAt(current, state_)});
        }
    }
}

} // namespace sampling
