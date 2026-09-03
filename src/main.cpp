#include "raylib.h"
#include "raymath.h"
#include "quantum.hpp"
#include "sampler.hpp"
#include "settings.hpp"
#include "state_sequence.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

enum class MotionPhase {
    Moving,
    FadingOut,
    Entering,
};

struct Particle {
    Vector3 position;
    Vector3 target;
    Vector3 pendingTarget;
    float opacity;
    double phase;
    double targetPhase;
    MotionPhase motion;
};

constexpr std::size_t phaseBinCount = 12;
using PhaseBatches = std::array<std::vector<Matrix>, phaseBinCount>;

int wrap(int value, int minimum, int maximum)
{
    if (value > maximum) return minimum;
    if (value < minimum) return maximum;
    return value;
}

void changeOrbital(quantum::ComplexState& orbital, char quantumNumber,
                   int direction)
{
    constexpr int maximumN = 5;

    if (quantumNumber == 'n') {
        orbital.n = wrap(orbital.n + direction, 1, maximumN);
        orbital.l = std::min(orbital.l, orbital.n - 1);
        orbital.m = std::clamp(orbital.m, -orbital.l, orbital.l);
    } else if (quantumNumber == 'l') {
        orbital.l = wrap(orbital.l + direction, 0, orbital.n - 1);
        orbital.m = std::clamp(orbital.m, -orbital.l, orbital.l);
    } else if (quantumNumber == 'm') {
        orbital.m = wrap(orbital.m + direction, -orbital.l, orbital.l);
    }
}

Vector3 displayPosition(quantum::PositionAu position)
{
    constexpr float scale = 0.38f;
    return {
        static_cast<float>(position.x) * scale,
        static_cast<float>(position.y) * scale,
        static_cast<float>(position.z) * scale,
    };
}

Vector3 spawnOutside(Vector3 target, std::size_t index)
{
    Vector3 direction = Vector3Normalize(target);
    if (Vector3LengthSqr(direction) < 0.001f) {
        const float angle = static_cast<float>(index) * 2.399963f;
        direction = {std::cos(angle), std::sin(angle), 0.35f};
        direction = Vector3Normalize(direction);
    }
    const float extra = 2.5f + static_cast<float>(index % 17) * 0.08f;
    return Vector3Add(target, Vector3Scale(direction, extra));
}

std::vector<Particle> makeParticles(
    const std::vector<sampling::Walker>& walkers)
{
    std::vector<Particle> particles;
    particles.reserve(walkers.size());
    for (std::size_t i = 0; i < walkers.size(); ++i) {
        const Vector3 target = displayPosition(walkers[i].position);
        particles.push_back({spawnOutside(target, i), target, target, 0.0f,
                             walkers[i].phase, walkers[i].phase,
                             MotionPhase::Entering});
    }
    return particles;
}

void retargetParticles(std::vector<Particle>& particles,
                       const std::vector<sampling::Walker>& walkers,
                       bool forceRespawn)
{
    assert(particles.size() == walkers.size());
    constexpr float longJump = 0.8f;
    for (std::size_t i = 0; i < particles.size(); ++i) {
        Particle& particle = particles[i];
        const Vector3 next = displayPosition(walkers[i].position);
        particle.targetPhase = walkers[i].phase;

        if (forceRespawn) {
            particle.pendingTarget = next;
            particle.motion = MotionPhase::FadingOut;
        } else if (particle.motion == MotionPhase::FadingOut) {
            particle.pendingTarget = next;
        } else if (Vector3Distance(particle.target, next) > longJump) {
            particle.pendingTarget = next;
            particle.motion = MotionPhase::FadingOut;
        } else {
            particle.target = next;
            particle.pendingTarget = next;
        }
    }
}

void updateParticles(std::vector<Particle>& particles, float deltaTime)
{
    const float easing = 1.0f - std::exp(-7.0f * deltaTime);
    for (std::size_t i = 0; i < particles.size(); ++i) {
        Particle& particle = particles[i];
        if (particle.motion == MotionPhase::FadingOut) {
            particle.opacity = std::max(0.0f, particle.opacity - 3.0f * deltaTime);
            if (particle.opacity == 0.0f) {
                particle.target = particle.pendingTarget;
                particle.position = spawnOutside(particle.target, i);
                particle.phase = particle.targetPhase;
                particle.motion = MotionPhase::Entering;
            }
            continue;
        }

        particle.position = Vector3Lerp(
            particle.position, particle.target, easing);
        particle.phase += std::remainder(
            particle.targetPhase - particle.phase,
            2.0 * 3.14159265358979323846
        ) * easing;
        if (particle.motion == MotionPhase::Entering) {
            particle.opacity = std::min(1.0f, particle.opacity + 1.8f * deltaTime);
            if (particle.opacity == 1.0f) {
                particle.motion = MotionPhase::Moving;
            }
        }
    }
}

void updatePhaseBatches(
    PhaseBatches& batches,
    const std::vector<Particle>& particles,
    float sphereRadius
)
{
    constexpr double pi = 3.14159265358979323846;
    for (auto& batch : batches) {
        batch.clear();
        batch.reserve(particles.size() / phaseBinCount + 1);
    }

    for (const Particle& particle : particles) {
        const float radius = sphereRadius * particle.opacity;
        const Matrix scale = MatrixScale(radius, radius, radius);
        const Vector3& p = particle.position;
        const Matrix translation = MatrixTranslate(p.x, p.y, p.z);
        const double normalizedPhase = (particle.phase + pi) / (2.0 * pi);
        const auto bin = static_cast<std::size_t>(
            std::floor(normalizedPhase * phaseBinCount)) % phaseBinCount;
        batches[bin].push_back(MatrixMultiply(scale, translation));
    }
}

void configureShader(Shader& shader)
{
    shader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(shader, "mvp");
    shader.locs[SHADER_LOC_MATRIX_MODEL] =
        GetShaderLocationAttrib(shader, "instanceTransform");
    shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "viewPos");
}

struct ShaderWatcher {
    std::filesystem::path vertexPath;
    std::filesystem::path fragmentPath;
    std::filesystem::file_time_type vertexStamp;
    std::filesystem::file_time_type fragmentStamp;
};

bool reloadShaderIfChanged(ShaderWatcher& watcher, Shader& shader,
                           Material& material, int& phaseColorLocation)
{
    const auto vertexStamp = std::filesystem::last_write_time(watcher.vertexPath);
    const auto fragmentStamp = std::filesystem::last_write_time(watcher.fragmentPath);
    if (vertexStamp == watcher.vertexStamp
        && fragmentStamp == watcher.fragmentStamp) {
        return false;
    }
    watcher.vertexStamp = vertexStamp;
    watcher.fragmentStamp = fragmentStamp;

    Shader replacement = LoadShader(
        watcher.vertexPath.c_str(), watcher.fragmentPath.c_str());
    if (!IsShaderValid(replacement)) return false;

    const Shader previous = shader;
    shader = replacement;
    configureShader(shader);
    phaseColorLocation = GetShaderLocation(shader, "phaseColor");
    material.shader = shader;
    UnloadShader(previous);
    return true;
}

int main()
{
    constexpr int screenWidth = 1600;
    constexpr int screenHeight = 900;

    quantum::ComplexState controlTest{1, 0, 0, 1};
    changeOrbital(controlTest, 'n', -1);
    assert(controlTest.n == 5);

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "Atomic Orbital Lab");
    SetTargetFPS(60);
    const std::string settingsPath =
        std::string(GetApplicationDirectory()) + "atom.settings";
    const settings::AppState saved = settings::load(settingsPath);
    quantum::ComplexState orbital = saved.orbital;
    quantum::ComplexState secondaryOrbital = sequence::nextState(orbital);
    bool superpositionMode = saved.superpositionMode;
    double quantumTimeAu = saved.quantumTimeAu;
    sampling::Sampler sampler(orbital);
    if (superpositionMode) {
        sampler.reset(quantum::equalSuperposition(orbital, secondaryOrbital));
        sampler.setTime(quantumTimeAu);
    }
    std::vector<Particle> particles = makeParticles(sampler.walkers());

    Mesh sphere = GenMeshSphere(1.0f, 6, 8);
    ShaderWatcher shaderWatcher{
        std::filesystem::path(ATOM_SHADER_DIR) / "orbital.vert",
        std::filesystem::path(ATOM_SHADER_DIR) / "orbital.frag",
        {},
        {},
    };
    shaderWatcher.vertexStamp =
        std::filesystem::last_write_time(shaderWatcher.vertexPath);
    shaderWatcher.fragmentStamp =
        std::filesystem::last_write_time(shaderWatcher.fragmentPath);
    Shader shader = LoadShader(shaderWatcher.vertexPath.c_str(),
                               shaderWatcher.fragmentPath.c_str());
    configureShader(shader);
    int phaseColorLocation = GetShaderLocation(shader, "phaseColor");

    Material material = LoadMaterialDefault();
    material.shader = shader;
    PhaseBatches phaseBatches;
    updatePhaseBatches(phaseBatches, particles, 0.055f);

    Camera3D camera{};
    camera.position = {11.0f, 7.0f, 11.0f};
    camera.target = {0.0f, 0.0f, 0.0f};
    camera.up = {0.0f, 1.0f, 0.0f};
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    float cameraYaw = saved.cameraYaw;
    float cameraPitch = saved.cameraPitch;
    float cameraDistance = saved.cameraDistance;
    bool autoRotate = saved.autoRotate;
    float autoRotationSpeed = saved.autoRotationSpeed;
    bool demoMode = saved.demoMode;
    float demoElapsed = 0.0f;
    constexpr float demoInterval = 6.0f;
    float shaderCheckElapsed = 0.0f;

    while (!WindowShouldClose()) {
        const float deltaTime = GetFrameTime();
        shaderCheckElapsed += deltaTime;
        if (shaderCheckElapsed >= 0.25f) {
            reloadShaderIfChanged(shaderWatcher, shader, material,
                                  phaseColorLocation);
            shaderCheckElapsed = 0.0f;
        }
        const bool shiftHeld = IsKeyDown(KEY_LEFT_SHIFT)
            || IsKeyDown(KEY_RIGHT_SHIFT);

        if (IsKeyPressed(KEY_R)) {
            if (shiftHeld) {
                autoRotationSpeed = -autoRotationSpeed;
            } else {
                autoRotate = !autoRotate;
            }
        }
        if (IsKeyPressed(KEY_D)) {
            demoMode = !demoMode;
            demoElapsed = 0.0f;
        }
        bool orbitalChanged = false;
        if (IsKeyPressed(KEY_S)) {
            superpositionMode = !superpositionMode;
            quantumTimeAu = 0.0;
            demoMode = false;
            orbitalChanged = true;
        }

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            const Vector2 movement = GetMouseDelta();
            cameraYaw -= movement.x * 0.005f;
            cameraPitch += movement.y * 0.005f;
            cameraPitch = std::clamp(cameraPitch, -1.45f, 1.45f);
        } else if (autoRotate) {
            cameraYaw += autoRotationSpeed * deltaTime;
        }

        cameraDistance -= GetMouseWheelMove() * 1.2f;
        cameraDistance = std::clamp(cameraDistance, 4.0f, 40.0f);
        camera.position = {
            cameraDistance * std::cos(cameraPitch) * std::cos(cameraYaw),
            cameraDistance * std::sin(cameraPitch),
            cameraDistance * std::cos(cameraPitch) * std::sin(cameraYaw),
        };

        const int direction = shiftHeld ? -1 : 1;
        if (IsKeyPressed(KEY_N)) {
            changeOrbital(orbital, 'n', direction);
            orbitalChanged = true;
            demoMode = false;
        }
        if (IsKeyPressed(KEY_L)) {
            changeOrbital(orbital, 'l', direction);
            orbitalChanged = true;
            demoMode = false;
        }
        if (IsKeyPressed(KEY_M)) {
            changeOrbital(orbital, 'm', direction);
            orbitalChanged = true;
            demoMode = false;
        }

        if (demoMode) {
            demoElapsed += deltaTime;
            if (demoElapsed >= demoInterval) {
                if (superpositionMode) {
                    orbital = secondaryOrbital;
                    superpositionMode = false;
                } else {
                    superpositionMode = true;
                }
                quantumTimeAu = 0.0;
                demoElapsed = 0.0f;
                orbitalChanged = true;
            }
        }

        if (orbitalChanged) {
            assert(quantum::isValid(orbital));
            secondaryOrbital = sequence::nextState(orbital);
            if (superpositionMode) {
                sampler.reset(quantum::equalSuperposition(
                    orbital, secondaryOrbital));
            } else {
                sampler.reset(orbital);
            }
            retargetParticles(particles, sampler.walkers(), true);
        }

        if (superpositionMode) {
            constexpr double quantumTimePerSecond = 4.0;
            quantumTimeAu += quantumTimePerSecond * deltaTime;
            sampler.setTime(quantumTimeAu);
        }
        sampler.advance();
        retargetParticles(particles, sampler.walkers(), false);
        updateParticles(particles, deltaTime);
        updatePhaseBatches(phaseBatches, particles, 0.055f);

        const float cameraPosition[] = {
            camera.position.x,
            camera.position.y,
            camera.position.z,
        };
        SetShaderValue(
            shader,
            shader.locs[SHADER_LOC_VECTOR_VIEW],
            cameraPosition,
            SHADER_UNIFORM_VEC3
        );

        BeginDrawing();
        ClearBackground({3, 4, 10, 255});

        BeginMode3D(camera);
        BeginBlendMode(BLEND_ADDITIVE);
        for (std::size_t bin = 0; bin < phaseBatches.size(); ++bin) {
            auto& transforms = phaseBatches[bin];
            if (transforms.empty()) continue;

            const Color color = ColorFromHSV(
                static_cast<float>(bin) * 360.0f / phaseBinCount,
                0.72f,
                1.0f
            );
            const float phaseColor[] = {
                color.r / 255.0f,
                color.g / 255.0f,
                color.b / 255.0f,
            };
            SetShaderValue(shader, phaseColorLocation, phaseColor,
                           SHADER_UNIFORM_VEC3);
            DrawMeshInstanced(
                sphere,
                material,
                transforms.data(),
                static_cast<int>(transforms.size())
            );
        }
        EndBlendMode();
        DrawSphere({0.0f, 0.0f, 0.0f}, 0.12f, GOLD);
        EndMode3D();

        DrawText(
            TextFormat(
                "n=%d l=%d m=%d  quantum=%s  mode=%s  rotation=%s  MALA=%.1f%%",
                orbital.n,
                orbital.l,
                orbital.m,
                superpositionMode ? "superposition" : "eigenstate",
                demoMode ? "demo" : "manual",
                autoRotate ? "auto" : "manual",
                sampler.diagnostics().acceptanceRate() * 100.0
            ),
            16,
            16,
            10,
            DARKGRAY
        );
        DrawText("moving points: probability samples, not electron paths",
                 16, 34, 10, DARKGRAY);
        DrawText("color: complex phase arg(psi)", 16, 52, 10, DARKGRAY);
        DrawText("D demo  |  S superposition  |  N/L/M state  |  shift reverses",
                 16, 70, 10, DARKGRAY);
        DrawFPS(screenWidth - 100, 22);

        EndDrawing();
    }

    settings::save(settingsPath, {
        orbital,
        cameraDistance,
        cameraYaw,
        cameraPitch,
        demoMode,
        autoRotate,
        autoRotationSpeed,
        superpositionMode,
        quantumTimeAu,
    });
    UnloadMesh(sphere);
    UnloadMaterial(material);
    CloseWindow();
    return 0;
}
