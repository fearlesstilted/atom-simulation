#include "sampler.hpp"

#include <cmath>
#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

double radius(quantum::PositionAu position)
{
    return std::sqrt(position.x * position.x
        + position.y * position.y
        + position.z * position.z);
}

} // namespace

int main()
{
    const sampling::SamplerConfig config{2000, 2000, 1234, 0.60, 0.35};
    sampling::Sampler first(quantum::ComplexState{1, 0, 0, 1}, config);
    sampling::Sampler second(quantum::ComplexState{1, 0, 0, 1}, config);

    check(first.walkers().size() == config.walkerCount,
          "sampler creates requested walker count");
    check(first.walkers().front().position.x
              == second.walkers().front().position.x,
          "fixed seed is repeatable");

    const auto before = first.walkers().front().position;
    for (int i = 0; i < 20; ++i) {
        first.advance();
        second.advance();
    }
    const auto after = first.walkers().front().position;
    check(radius({after.x - before.x, after.y - before.y,
                  after.z - before.z}) > 0.0,
          "walkers move after advances");
    check(after.x == second.walkers().front().position.x
              && after.y == second.walkers().front().position.y
              && after.z == second.walkers().front().position.z,
          "fixed seed remains repeatable after advances");

    double radiusSum = 0.0;
    for (const auto& walker : first.walkers()) {
        check(std::isfinite(walker.position.x)
                  && std::isfinite(walker.position.y)
                  && std::isfinite(walker.position.z)
                  && std::isfinite(walker.phase),
              "walker values remain finite");
        radiusSum += radius(walker.position);
    }

    const auto diagnostics = first.diagnostics();
    check(diagnostics.attempted == 20 * config.updatesPerAdvance,
          "diagnostics count attempted moves");
    check(diagnostics.acceptanceRate() > 0.1
              && diagnostics.acceptanceRate() < 0.95,
          "acceptance rate is informative");
    check(std::abs(radiusSum / first.walkers().size() - 1.5) < 0.2,
          "sampled 1s mean radius matches expectation");

    const sampling::SamplerConfig flowConfig{
        1000, 1000, 777, 1e-8, 1.0};
    sampling::Sampler positiveFlow(quantum::ComplexState{2, 1, 1, 1}, flowConfig);
    sampling::Sampler negativeFlow(quantum::ComplexState{2, 1, -1, 1}, flowConfig);
    const auto beforeFlow = positiveFlow.walkers();
    positiveFlow.advance();
    negativeFlow.advance();

    double positiveTurn = 0.0;
    double negativeTurn = 0.0;
    for (std::size_t i = 0; i < beforeFlow.size(); ++i) {
        const auto beforePosition = beforeFlow[i].position;
        const auto positivePosition = positiveFlow.walkers()[i].position;
        const auto negativePosition = negativeFlow.walkers()[i].position;
        positiveTurn += beforePosition.x * positivePosition.y
            - beforePosition.y * positivePosition.x;
        negativeTurn += beforePosition.x * negativePosition.y
            - beforePosition.y * negativePosition.x;
        check(std::abs(radius(beforePosition) - radius(positivePosition)) < 1e-6,
              "probability flow preserves radius");
    }
    check(positiveTurn > 0.0, "positive m flows counterclockwise");
    check(negativeTurn < 0.0, "negative m reverses probability flow");

    const auto superposition = quantum::equalSuperposition(
        {1, 0, 0, 1}, {2, 1, 0, 1});
    sampling::Sampler evolving(superposition, config);
    const double phaseBefore = evolving.walkers().front().phase;
    evolving.setTime(8.0);
    evolving.advance();
    check(std::isfinite(evolving.walkers().front().phase)
              && std::abs(evolving.walkers().front().phase - phaseBefore) > 1e-6,
          "superposition phase evolves with time");

    sampling::Sampler realOrbital(
        quantum::RealState{2, quantum::RealOrbital::Px, 1}, config);
    realOrbital.advance();
    check(std::isfinite(realOrbital.walkers().front().position.x)
              && std::isfinite(realOrbital.walkers().front().phase),
          "real orbital sampler remains finite");

    return failures == 0 ? 0 : 1;
}
