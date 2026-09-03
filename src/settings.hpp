#pragma once

#include "quantum.hpp"

#include <filesystem>

namespace settings {

struct AppState {
    quantum::ComplexState orbital{1, 0, 0, 1};
    float cameraDistance = 24.0f;
    float cameraYaw = 0.8f;
    float cameraPitch = 0.45f;
    bool demoMode = true;
    bool autoRotate = false;
    float autoRotationSpeed = 0.12f;
};

AppState load(const std::filesystem::path& path);
bool save(const std::filesystem::path& path, const AppState& state);

} // namespace settings
