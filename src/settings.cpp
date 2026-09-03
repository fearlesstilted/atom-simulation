#include "settings.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <string>

namespace settings {
AppState load(const std::filesystem::path& path)
{
    AppState state;
    std::ifstream input(path);
    std::string magic;
    int version = 0;
    int demoMode = 0;
    int autoRotate = 0;

    if (!(input >> magic >> version
          >> state.orbital.n >> state.orbital.l >> state.orbital.m
          >> state.orbital.nuclearCharge
          >> state.cameraDistance >> state.cameraYaw >> state.cameraPitch
          >> demoMode >> autoRotate >> state.autoRotationSpeed)
        || magic != "ATOM_SETTINGS"
        || version != 1
        || !quantum::isValid(state.orbital)
        || state.orbital.n > 5
        || !std::isfinite(state.cameraDistance)
        || !std::isfinite(state.cameraYaw)
        || !std::isfinite(state.cameraPitch)
        || !std::isfinite(state.autoRotationSpeed)
        || (demoMode != 0 && demoMode != 1)
        || (autoRotate != 0 && autoRotate != 1)) {
        return {};
    }

    state.cameraDistance = std::clamp(state.cameraDistance, 4.0f, 40.0f);
    state.cameraPitch = std::clamp(state.cameraPitch, -1.45f, 1.45f);
    state.autoRotationSpeed = std::clamp(
        state.autoRotationSpeed, -1.0f, 1.0f);
    state.demoMode = demoMode == 1;
    state.autoRotate = autoRotate == 1;
    return state;
}

bool save(const std::filesystem::path& path, const AppState& state)
{
    std::ofstream output(path);
    if (!output) return false;

    output << std::setprecision(9)
           << "ATOM_SETTINGS 1 "
           << state.orbital.n << ' '
           << state.orbital.l << ' '
           << state.orbital.m << ' '
           << state.orbital.nuclearCharge << ' '
           << state.cameraDistance << ' '
           << state.cameraYaw << ' '
           << state.cameraPitch << ' '
           << static_cast<int>(state.demoMode) << ' '
           << static_cast<int>(state.autoRotate) << ' '
           << state.autoRotationSpeed << '\n';
    return output.good();
}

} // namespace settings
