#include "raylib.h"
#include "raymath.h"
#include "quantum.hpp"
#include "sampler.hpp"
#include "state_sequence.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <fstream>
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

float loadZoom(const std::string& path)
{
    constexpr float defaultZoom = 24.0f;
    std::ifstream input(path);
    float value = defaultZoom;
    if (!(input >> value) || !std::isfinite(value)) return defaultZoom;
    return std::clamp(value, 4.0f, 40.0f);
}

void saveZoom(const std::string& path, float value)
{
    std::ofstream(path) << std::clamp(value, 4.0f, 40.0f) << '\n';
}

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

const char* vertexShaderCode = R"glsl(
#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
in mat4 instanceTransform;
uniform mat4 mvp;
out vec3 fragPosition;
out vec3 fragNormal;

void main()
{
    vec4 worldPosition = instanceTransform * vec4(vertexPosition, 1.0);
    fragPosition = worldPosition.xyz;
    fragNormal = normalize(mat3(instanceTransform) * vertexNormal);
    gl_Position = mvp * worldPosition;
}
)glsl";

const char* fragmentShaderCode = R"glsl(
#version 330
in vec3 fragPosition;
in vec3 fragNormal;
uniform vec3 viewPos;
uniform vec3 phaseColor;
out vec4 finalColor;

void main()
{
    vec3 normal = normalize(fragNormal);
    vec3 lightDirection = normalize(vec3(0.4, 0.8, 0.3));
    vec3 viewDirection = normalize(viewPos - fragPosition);

    float diffuse = max(dot(normal, lightDirection), 0.0);
    float specular = pow(max(dot(reflect(-lightDirection, normal),
                                 viewDirection), 0.0), 24.0);
    vec3 color = phaseColor * (0.35 + 1.15 * diffuse);
    color += vec3(specular * 1.5);
    finalColor = vec4(color, 1.0);
}
)glsl";

int main()
{
    constexpr int screenWidth = 1600;
    constexpr int screenHeight = 900;
    quantum::ComplexState orbital{1, 0, 0, 1};

    quantum::ComplexState controlTest{1, 0, 0, 1};
    changeOrbital(controlTest, 'n', -1);
    assert(controlTest.n == 5);

    sampling::Sampler sampler(orbital);
    std::vector<Particle> particles = makeParticles(sampler.walkers());

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "Atomic Orbital Lab");
    SetTargetFPS(60);
    const std::string settingsPath =
        std::string(GetApplicationDirectory()) + "atom.settings";

    Mesh sphere = GenMeshSphere(1.0f, 6, 8);
    Shader shader = LoadShaderFromMemory(vertexShaderCode, fragmentShaderCode);
    shader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(shader, "mvp");
    shader.locs[SHADER_LOC_MATRIX_MODEL] =
        GetShaderLocationAttrib(shader, "instanceTransform");
    shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "viewPos");
    const int phaseColorLocation = GetShaderLocation(shader, "phaseColor");

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

    float cameraYaw = 0.8f;
    float cameraPitch = 0.45f;
    float cameraDistance = loadZoom(settingsPath);
    bool autoRotate = false;
    float autoRotationSpeed = 0.12f;
    bool demoMode = true;
    float demoElapsed = 0.0f;
    constexpr float demoInterval = 6.0f;

    while (!WindowShouldClose()) {
        const float deltaTime = GetFrameTime();
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
        bool orbitalChanged = false;

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
                orbital = sequence::nextState(orbital);
                demoElapsed = 0.0f;
                orbitalChanged = true;
            }
        }

        if (orbitalChanged) {
            assert(quantum::isValid(orbital));
            sampler.reset(orbital);
            retargetParticles(particles, sampler.walkers(), true);
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
                "n=%d  l=%d  m=%d  mode=%s  rotation=%s  MALA=%.1f%%",
                orbital.n,
                orbital.l,
                orbital.m,
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
        DrawText("D demo  |  N/L/M state  |  shift reverses",
                 16, 70, 10, DARKGRAY);
        DrawFPS(screenWidth - 100, 22);

        EndDrawing();
    }

    saveZoom(settingsPath, cameraDistance);
    UnloadMesh(sphere);
    UnloadMaterial(material);
    CloseWindow();
    return 0;
}
