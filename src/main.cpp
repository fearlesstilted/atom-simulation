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
    Vector3 morphStart;
    float opacity;
    double phase;
    double targetPhase;
    double morphStartPhase;
    MotionPhase motion;
};

constexpr std::size_t phaseBinCount = 12;
using PhaseBatches = std::array<std::vector<Matrix>, phaseBinCount>;

constexpr std::array realOrbitals{
    quantum::RealOrbital::Px,
    quantum::RealOrbital::Py,
    quantum::RealOrbital::Pz,
    quantum::RealOrbital::Dxy,
    quantum::RealOrbital::Dxz,
    quantum::RealOrbital::Dyz,
    quantum::RealOrbital::Dz2,
    quantum::RealOrbital::Dx2Y2,
};

int wrap(int value, int minimum, int maximum)
{
    if (value > maximum) return minimum;
    if (value < minimum) return maximum;
    return value;
}

quantum::RealOrbital nextRealOrbital(quantum::RealOrbital current,
                                     int direction)
{
    const auto found = std::find(realOrbitals.begin(), realOrbitals.end(), current);
    const int index = static_cast<int>(found - realOrbitals.begin());
    return realOrbitals[wrap(index + direction, 0,
                             static_cast<int>(realOrbitals.size()) - 1)];
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
        const Vector3 start = spawnOutside(target, i);
        particles.push_back({start, target, target, start, 0.0f,
                             walkers[i].phase, walkers[i].phase,
                             walkers[i].phase,
                             MotionPhase::Entering});
    }
    return particles;
}

void beginMorph(std::vector<Particle>& particles,
                const std::vector<sampling::Walker>& walkers)
{
    assert(particles.size() == walkers.size());
    for (std::size_t i = 0; i < particles.size(); ++i) {
        Particle& particle = particles[i];
        particle.morphStart = particle.position;
        particle.morphStartPhase = particle.phase;
        particle.target = displayPosition(walkers[i].position);
        particle.pendingTarget = particle.target;
        particle.targetPhase = walkers[i].phase;
        particle.opacity = 1.0f;
        particle.motion = MotionPhase::Moving;
    }
}

float smootherStep(float progress)
{
    const float t = std::clamp(progress, 0.0f, 1.0f);
    const float smooth = t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    return 0.12f * t + 0.88f * smooth;
}

void updateMorph(std::vector<Particle>& particles, float progress)
{
    constexpr double twoPi = 2.0 * 3.14159265358979323846;
    const float amount = smootherStep(progress);
    for (Particle& particle : particles) {
        particle.position = Vector3Lerp(
            particle.morphStart, particle.target, amount);
        particle.phase = particle.morphStartPhase + std::remainder(
            particle.targetPhase - particle.morphStartPhase, twoPi) * amount;
    }
}

void updateMorphTargets(std::vector<Particle>& particles,
                        const std::vector<sampling::Walker>& walkers)
{
    assert(particles.size() == walkers.size());
    for (std::size_t i = 0; i < particles.size(); ++i) {
        particles[i].target = displayPosition(walkers[i].position);
        particles[i].targetPhase = walkers[i].phase;
    }
}

void retargetParticles(std::vector<Particle>& particles,
                       const std::vector<sampling::Walker>& walkers)
{
    assert(particles.size() == walkers.size());
    constexpr float longJump = 0.8f;
    for (std::size_t i = 0; i < particles.size(); ++i) {
        Particle& particle = particles[i];
        const Vector3 next = displayPosition(walkers[i].position);
        particle.targetPhase = walkers[i].phase;

        if (particle.motion == MotionPhase::FadingOut) {
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

struct DensitySlice {
    static constexpr int resolution = 240;
    Texture2D texture{};
    std::vector<Color> pixels;
    std::vector<std::complex<double>> amplitudes;
    int plane = 0;
};

const char* slicePlaneName(int plane)
{
    if (plane == 1) return "z = 0";
    if (plane == 2) return "y = 0";
    if (plane == 3) return "x = 0";
    return "off";
}

template <typename Evaluate>
void updateDensitySlice(DensitySlice& slice, double extent,
                        Evaluate evaluate)
{
    constexpr double pi = 3.14159265358979323846;
    double maximumDensity = 0.0;
    for (int row = 0; row < DensitySlice::resolution; ++row) {
        for (int column = 0; column < DensitySlice::resolution; ++column) {
            const double horizontal = extent
                * (2.0 * column / (DensitySlice::resolution - 1.0) - 1.0);
            const double vertical = extent
                * (1.0 - 2.0 * row / (DensitySlice::resolution - 1.0));
            quantum::PositionAu position{};
            if (slice.plane == 1) position = {horizontal, vertical, 0.0};
            if (slice.plane == 2) position = {horizontal, 0.0, vertical};
            if (slice.plane == 3) position = {0.0, horizontal, vertical};
            const std::size_t index = static_cast<std::size_t>(row)
                * DensitySlice::resolution + column;
            slice.amplitudes[index] = evaluate(position);
            maximumDensity = std::max(
                maximumDensity, std::norm(slice.amplitudes[index]));
        }
    }

    for (std::size_t i = 0; i < slice.amplitudes.size(); ++i) {
        const double relativeDensity = maximumDensity == 0.0
            ? 0.0 : std::norm(slice.amplitudes[i]) / maximumDensity;
        const float value = static_cast<float>(
            std::pow(relativeDensity, 0.22));
        const float hue = static_cast<float>(
            (std::arg(slice.amplitudes[i]) + pi) * 180.0 / pi);
        Color color = ColorFromHSV(hue, 0.68f, value);
        color.a = 245;
        slice.pixels[i] = color;
    }
    UpdateTexture(slice.texture, slice.pixels.data());
}

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

bool reloadBloomIfChanged(const std::filesystem::path& path,
                          std::filesystem::file_time_type& stamp,
                          Shader& shader, Vector2 resolution)
{
    const auto nextStamp = std::filesystem::last_write_time(path);
    if (nextStamp == stamp) return false;
    stamp = nextStamp;
    Shader replacement = LoadShader(nullptr, path.c_str());
    if (!IsShaderValid(replacement)) return false;
    const Shader previous = shader;
    shader = replacement;
    const int location = GetShaderLocation(shader, "resolution");
    SetShaderValue(shader, location, &resolution, SHADER_UNIFORM_VEC2);
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
    assert(smootherStep(0.0f) == 0.0f
        && smootherStep(0.5f) == 0.5f
        && smootherStep(1.0f) == 1.0f);

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "Atomic Orbital Lab");
    SetTargetFPS(60);
    Font hudFont = GetFontDefault();
    bool unloadHudFont = false;
    constexpr std::array fontPaths{
        "/usr/share/fonts/google-noto-vf/NotoSans[wght].ttf",
        "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    };
    for (const char* path : fontPaths) {
        if (!FileExists(path)) continue;
        hudFont = LoadFontEx(path, 18, nullptr, 0);
        unloadHudFont = true;
        break;
    }
    const std::string settingsPath =
        std::string(GetApplicationDirectory()) + "atom.settings";
    const settings::AppState saved = settings::load(settingsPath);
    quantum::ComplexState orbital = saved.orbital;
    quantum::ComplexState secondaryOrbital = sequence::nextState(orbital);
    bool superpositionMode = saved.superpositionMode;
    double quantumTimeAu = saved.quantumTimeAu;
    bool realMode = saved.realMode;
    quantum::RealOrbital realOrbital = saved.realOrbital;
    sampling::Sampler sampler(orbital);
    if (realMode) {
        sampler.reset({orbital.n, realOrbital, orbital.nuclearCharge});
    } else if (superpositionMode) {
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
    const std::filesystem::path bloomPath =
        std::filesystem::path(ATOM_SHADER_DIR) / "bloom.frag";
    auto bloomStamp = std::filesystem::last_write_time(bloomPath);
    Shader bloomShader = LoadShader(nullptr, bloomPath.c_str());
    const Vector2 renderResolution{
        static_cast<float>(screenWidth), static_cast<float>(screenHeight)};
    const int bloomResolutionLocation =
        GetShaderLocation(bloomShader, "resolution");
    SetShaderValue(bloomShader, bloomResolutionLocation, &renderResolution,
                   SHADER_UNIFORM_VEC2);
    RenderTexture2D sceneTarget = LoadRenderTexture(screenWidth, screenHeight);

    DensitySlice densitySlice;
    densitySlice.pixels.resize(
        DensitySlice::resolution * DensitySlice::resolution, BLACK);
    densitySlice.amplitudes.resize(densitySlice.pixels.size());
    Image sliceImage = GenImageColor(
        DensitySlice::resolution, DensitySlice::resolution, BLACK);
    densitySlice.texture = LoadTextureFromImage(sliceImage);
    UnloadImage(sliceImage);
    SetTextureFilter(densitySlice.texture, TEXTURE_FILTER_BILINEAR);
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
    constexpr float demoInterval = 12.0f;
    constexpr float morphDuration = 5.0f;
    float morphElapsed = morphDuration;
    float shaderCheckElapsed = 0.0f;
    float sliceElapsed = 0.0f;
    bool sliceDirty = false;

    while (!WindowShouldClose()) {
        const float deltaTime = GetFrameTime();
        shaderCheckElapsed += deltaTime;
        if (shaderCheckElapsed >= 0.25f) {
            reloadShaderIfChanged(shaderWatcher, shader, material,
                                  phaseColorLocation);
            reloadBloomIfChanged(
                bloomPath, bloomStamp, bloomShader, renderResolution);
            shaderCheckElapsed = 0.0f;
        }
        const bool shiftHeld = IsKeyDown(KEY_LEFT_SHIFT)
            || IsKeyDown(KEY_RIGHT_SHIFT);
        bool orbitalChanged = false;

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
            if (demoMode && realMode) {
                realMode = false;
                superpositionMode = false;
                orbitalChanged = true;
            }
        }
        if (IsKeyPressed(KEY_S)) {
            superpositionMode = !superpositionMode;
            realMode = false;
            quantumTimeAu = 0.0;
            demoMode = false;
            orbitalChanged = true;
        }
        if (IsKeyPressed(KEY_B)) {
            realMode = !realMode;
            superpositionMode = false;
            demoMode = false;
            while (realMode && !quantum::isValid({
                    orbital.n, realOrbital, orbital.nuclearCharge})) {
                orbital.n = wrap(orbital.n + 1, 1, 5);
            }
            orbitalChanged = true;
        }
        if (IsKeyPressed(KEY_O)) {
            realMode = true;
            superpositionMode = false;
            demoMode = false;
            realOrbital = nextRealOrbital(
                realOrbital, shiftHeld ? -1 : 1);
            while (!quantum::isValid({
                    orbital.n, realOrbital, orbital.nuclearCharge})) {
                orbital.n = wrap(orbital.n + 1, 1, 5);
            }
            orbitalChanged = true;
        }
        if (IsKeyPressed(KEY_X)) {
            densitySlice.plane = (densitySlice.plane + 1) % 4;
            sliceDirty = densitySlice.plane != 0;
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
            if (realMode) {
                do {
                    orbital.n = wrap(orbital.n + direction, 1, 5);
                } while (!quantum::isValid({
                    orbital.n, realOrbital, orbital.nuclearCharge}));
            } else {
                changeOrbital(orbital, 'n', direction);
            }
            orbitalChanged = true;
            demoMode = false;
        }
        if (IsKeyPressed(KEY_L)) {
            realMode = false;
            changeOrbital(orbital, 'l', direction);
            orbitalChanged = true;
            demoMode = false;
        }
        if (IsKeyPressed(KEY_M)) {
            realMode = false;
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
            if (realMode) {
                sampler.reset({
                    orbital.n, realOrbital, orbital.nuclearCharge});
            } else if (superpositionMode) {
                sampler.reset(quantum::equalSuperposition(
                    orbital, secondaryOrbital));
            } else {
                sampler.reset(orbital);
            }
            beginMorph(particles, sampler.walkers());
            morphElapsed = 0.0f;
            sliceDirty = densitySlice.plane != 0;
        }

        if (superpositionMode) {
            constexpr double quantumTimePerSecond = 4.0;
            quantumTimeAu += quantumTimePerSecond * deltaTime;
            sampler.setTime(quantumTimeAu);
        }
        sampler.advance();

        if (morphElapsed < morphDuration) {
            morphElapsed = std::min(morphElapsed + deltaTime, morphDuration);
            updateMorphTargets(particles, sampler.walkers());
            updateMorph(particles, morphElapsed / morphDuration);
        } else {
            retargetParticles(particles, sampler.walkers());
            updateParticles(particles, deltaTime);
        }
        updatePhaseBatches(phaseBatches, particles, 0.055f);

        sliceElapsed += deltaTime;
        if (densitySlice.plane != 0
            && (sliceDirty || (superpositionMode && sliceElapsed >= 0.15f))) {
            const double extent = 1.65 * orbital.n * orbital.n
                / orbital.nuclearCharge;
            if (realMode) {
                const quantum::RealState state{
                    orbital.n, realOrbital, orbital.nuclearCharge};
                updateDensitySlice(densitySlice, extent,
                    [&](quantum::PositionAu position) {
                        return quantum::wavefunction(position, state);
                    });
            } else if (superpositionMode) {
                const auto state = quantum::equalSuperposition(
                    orbital, secondaryOrbital);
                updateDensitySlice(densitySlice, extent,
                    [&](quantum::PositionAu position) {
                        return quantum::wavefunction(
                            position, state, quantumTimeAu);
                    });
            } else {
                updateDensitySlice(densitySlice, extent,
                    [&](quantum::PositionAu position) {
                        return quantum::wavefunction(position, orbital);
                    });
            }
            sliceDirty = false;
            sliceElapsed = 0.0f;
        }

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

        BeginTextureMode(sceneTarget);
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
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);
        BeginShaderMode(bloomShader);
        DrawTextureRec(sceneTarget.texture,
                       {0.0f, 0.0f, static_cast<float>(screenWidth),
                        -static_cast<float>(screenHeight)},
                       {0.0f, 0.0f}, WHITE);
        EndShaderMode();

        if (densitySlice.plane != 0) {
            constexpr float size = 240.0f;
            const Vector2 position{
                screenWidth - size - 18.0f, screenHeight - size - 18.0f};
            DrawRectangle(static_cast<int>(position.x) - 1,
                          static_cast<int>(position.y) - 23,
                          static_cast<int>(size) + 2,
                          static_cast<int>(size) + 24,
                          {3, 4, 10, 225});
            DrawTexturePro(densitySlice.texture,
                           {0.0f, 0.0f,
                            static_cast<float>(DensitySlice::resolution),
                            static_cast<float>(DensitySlice::resolution)},
                           {position.x, position.y, size, size},
                           {0.0f, 0.0f}, 0.0f, WHITE);
            DrawTextEx(hudFont,
                       TextFormat("density slice  %s", slicePlaneName(
                           densitySlice.plane)),
                       {position.x, position.y - 20.0f}, 14.0f, 0.0f,
                       {170, 178, 192, 220});
        }

        std::string stateLabel;
        double energy = quantum::energyHartree(orbital);
        if (realMode) {
            stateLabel = std::to_string(orbital.n) + quantum::name(realOrbital);
        } else if (superpositionMode) {
            stateLabel = "|" + std::to_string(orbital.n) + ","
                + std::to_string(orbital.l) + ","
                + std::to_string(orbital.m) + "> + |"
                + std::to_string(secondaryOrbital.n) + ","
                + std::to_string(secondaryOrbital.l) + ","
                + std::to_string(secondaryOrbital.m) + ">";
            energy = 0.5 * (energy
                + quantum::energyHartree(secondaryOrbital));
        } else {
            stateLabel = "|" + std::to_string(orbital.n) + ","
                + std::to_string(orbital.l) + ","
                + std::to_string(orbital.m) + ">";
        }
        DrawTextEx(hudFont, TextFormat("%s   E %.3f Ha / %.2f eV",
                       stateLabel.c_str(), energy, energy * 27.211386),
                   {18.0f, 16.0f}, 16.0f, 0.0f, {190, 198, 210, 220});
        DrawTextEx(hudFont, TextFormat("30,000 samples   MALA %.0f%%   %d fps",
                       sampler.diagnostics().acceptanceRate() * 100.0,
                       GetFPS()),
                   {18.0f, 38.0f}, 14.0f, 0.0f, {115, 124, 140, 210});

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
        realMode,
        realOrbital,
    });
    UnloadMesh(sphere);
    UnloadRenderTexture(sceneTarget);
    UnloadTexture(densitySlice.texture);
    UnloadShader(bloomShader);
    UnloadMaterial(material);
    if (unloadHudFont) UnloadFont(hudFont);
    CloseWindow();
    return 0;
}
