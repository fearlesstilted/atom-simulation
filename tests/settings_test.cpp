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
        {4, 2, -1, 1}, 31.5f, 1.2f, -0.4f, false, true, -0.12f,
        false, 7.25, true, quantum::RealOrbital::Dx2Y2};

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
        || actual.autoRotationSpeed != expected.autoRotationSpeed
        || actual.superpositionMode != expected.superpositionMode
        || std::abs(actual.quantumTimeAu - expected.quantumTimeAu) > 1e-9
        || actual.realMode != expected.realMode
        || actual.realOrbital != expected.realOrbital) {
        std::cerr << "FAIL: settings round trip\n";
        return 1;
    }

    std::ofstream(path)
        << "ATOM_SETTINGS 1 3 2 1 1 18 0.5 -0.25 0 1 0.2\n";
    const auto legacy = settings::load(path);
    if (legacy.orbital.n != 3 || legacy.cameraDistance != 18.0f
        || legacy.demoMode || !legacy.autoRotate
        || legacy.superpositionMode || legacy.quantumTimeAu != 0.0) {
        std::cerr << "FAIL: legacy settings load\n";
        return 1;
    }

    std::ofstream(path)
        << "ATOM_SETTINGS 2 3 2 1 1 18 0.5 -0.25 0 1 0.2 1 4.5\n";
    const auto versionTwo = settings::load(path);
    if (!versionTwo.superpositionMode || versionTwo.quantumTimeAu != 4.5
        || versionTwo.realMode) {
        std::cerr << "FAIL: version two settings load\n";
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
