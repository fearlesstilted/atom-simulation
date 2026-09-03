#include "sampler.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace sampling {
namespace {

quantum::PositionAu subtract(quantum::PositionAu a, quantum::PositionAu b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

double lengthSquared(quantum::PositionAu value)
{
    return value.x * value.x + value.y * value.y + value.z * value.z;
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
        || config.flowTimeScale < 0.0) {
        throw std::invalid_argument("invalid sampler configuration");
    }
    initializeWalkers();
}

Sampler::Sampler(const quantum::Superposition& state, SamplerConfig config)
    : state_(state), config_(config), random_(config.seed)
{
    if (!quantum::isValidSuperposition(state)
        || config.walkerCount == 0
        || config.updatesPerAdvance == 0
        || config.stepScale <= 0.0
        || config.flowTimeScale < 0.0) {
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

void Sampler::reset(const quantum::Superposition& state)
{
    if (!quantum::isValidSuperposition(state)) {
        throw std::invalid_argument("invalid quantum superposition");
    }
    state_ = state;
    timeAu_ = 0.0;
    random_.seed(config_.seed);
    nextWalker_ = 0;
    diagnostics_ = {};
    initializeWalkers();
}

void Sampler::setTime(double timeAu)
{
    if (!std::isfinite(timeAu)) {
        throw std::invalid_argument("invalid quantum time");
    }
    timeAu_ = timeAu;
}

double Sampler::densityAt(quantum::PositionAu position) const
{
    constexpr double floor = 1e-300;
    const double density = std::visit([&](const auto& state) {
        using State = std::decay_t<decltype(state)>;
        if constexpr (std::is_same_v<State, quantum::ComplexState>) {
            return quantum::probabilityDensity(position, state);
        } else {
            return quantum::probabilityDensity(position, state, timeAu_);
        }
    }, state_);
    return std::log(std::max(density, floor));
}

double Sampler::phaseAt(quantum::PositionAu position) const
{
    return std::arg(std::visit([&](const auto& state) {
        using State = std::decay_t<decltype(state)>;
        if constexpr (std::is_same_v<State, quantum::ComplexState>) {
            return quantum::wavefunction(position, state);
        } else {
            return quantum::wavefunction(position, state, timeAu_);
        }
    }, state_));
}

double Sampler::spatialScale() const
{
    return std::visit([](const auto& state) {
        using State = std::decay_t<decltype(state)>;
        if constexpr (std::is_same_v<State, quantum::ComplexState>) {
            return static_cast<double>(state.n * state.n)
                / state.nuclearCharge;
        } else {
            double scale = 0.0;
            for (const auto& term : state.terms) {
                scale = std::max(scale,
                    static_cast<double>(term.state.n * term.state.n)
                    / term.state.nuclearCharge);
            }
            return scale;
        }
    }, state_);
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
    const double scale = spatialScale();
    const double delta = 0.002 * scale;
    const double xPlus = densityAt({position.x + delta, position.y, position.z});
    const double xMinus = densityAt({position.x - delta, position.y, position.z});
    const double yPlus = densityAt({position.x, position.y + delta, position.z});
    const double yMinus = densityAt({position.x, position.y - delta, position.z});
    const double zPlus = densityAt({position.x, position.y, position.z + delta});
    const double zMinus = densityAt({position.x, position.y, position.z - delta});
    quantum::PositionAu drift{
        (xPlus - xMinus) / (2.0 * delta),
        (yPlus - yMinus) / (2.0 * delta),
        (zPlus - zMinus) / (2.0 * delta),
    };

    const double step = config_.stepScale * std::sqrt(spatialScale());
    const double dt = step * step;
    drift.x *= 0.5 * dt;
    drift.y *= 0.5 * dt;
    drift.z *= 0.5 * dt;

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
    const double step = config_.stepScale * std::sqrt(spatialScale());
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
    const double logAcceptance = densityAt(candidate)
        - densityAt(walker.position)
        + logReverse - logForward;

    ++diagnostics_.attempted;
    if (std::log(unit(random_)) < std::min(0.0, logAcceptance)) {
        walker = {candidate, phaseAt(candidate)};
        ++diagnostics_.accepted;
    }
    applyProbabilityFlow(walker);
}

void Sampler::applyProbabilityFlow(Walker& walker) const
{
    const auto* state = std::get_if<quantum::ComplexState>(&state_);
    if (state == nullptr) return;
    const double cylindricalRadiusSquared =
        walker.position.x * walker.position.x
        + walker.position.y * walker.position.y;
    if (state->m == 0 || cylindricalRadiusSquared < 1e-20) return;

    const auto velocity = quantum::probabilityCurrentVelocity(
        walker.position, *state);
    const double angularVelocity =
        (walker.position.x * velocity.y - walker.position.y * velocity.x)
        / cylindricalRadiusSquared;
    const double angle = std::clamp(
        config_.flowTimeScale * angularVelocity, -0.15, 0.15);
    const double cosine = std::cos(angle);
    const double sine = std::sin(angle);
    const double x = walker.position.x;
    const double y = walker.position.y;
    walker.position.x = cosine * x - sine * y;
    walker.position.y = sine * x + cosine * y;
    walker.phase = phaseAt(walker.position);
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
    const double orbitalScale = spatialScale();
    std::normal_distribution<double> initial(0.0, 0.5 * orbitalScale);
    std::normal_distribution<double> step(
        0.0, 0.75 * std::sqrt(orbitalScale));
    std::uniform_real_distribution<double> unit(0.0, 1.0);

    quantum::PositionAu current{};
    do {
        current = {initial(random_), initial(random_), initial(random_)};
    } while (densityAt(current) < -600.0);
    double currentLogDensity = densityAt(current);

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
        const double candidateLogDensity = densityAt(candidate);
        if (std::log(unit(random_))
            < std::min(0.0, candidateLogDensity - currentLogDensity)) {
            current = candidate;
            currentLogDensity = candidateLogDensity;
        }

        ++iteration;
        if (iteration > burnIn && iteration % thinning == 0) {
            walkers_.push_back({current, phaseAt(current)});
        }
    }
}

} // namespace sampling
