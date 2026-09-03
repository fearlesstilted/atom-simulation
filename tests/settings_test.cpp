#include "settings.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

int main()
{
    const auto path = std::filesystem::temp_directory_path()
        / "atom-settings-test.txt";
    const settings::AppState expected{
        {4, 2, -1, 1}, 31.5f, 1.2f, -0.4f, false, true, -0.12f};

    if (!settings::save(path, expected)) {
        std::cerr << "FAIL: settings save\n";
        return 1;
    }
    const auto actual = settings::load(path);
    if (actual.orbital.n != expected.orbital.n
        || actual.orbital.l != expected.orbital.l
        || actual.orbital.m != expected.orbital.m
        || std::abs(actual.cameraDistance - expected.cameraDistance) > 1e-6f
        || std::abs(actual.cameraYaw - expected.cameraYaw) > 1e-6f
        || std::abs(actual.cameraPitch - expected.cameraPitch) > 1e-6f
        || actual.demoMode != expected.demoMode
        || actual.autoRotate != expected.autoRotate
        || actual.autoRotationSpeed != expected.autoRotationSpeed) {
        std::cerr << "FAIL: settings round trip\n";
        return 1;
    }

    std::ofstream(path) << "broken data\n";
    const auto fallback = settings::load(path);
    std::filesystem::remove(path);
    if (fallback.orbital.n != 1 || fallback.cameraDistance != 24.0f
        || !fallback.demoMode) {
        std::cerr << "FAIL: malformed settings fallback\n";
        return 1;
    }
    return 0;
}
