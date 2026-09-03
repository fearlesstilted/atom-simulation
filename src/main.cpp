#include "raylib.h"
#include "raymath.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <random>
#include <vector>

struct Particle {
    Vector3 position;
};

struct Orbital {
    int n;
    int l;
    int m;
};

bool isValid(const Orbital& orbital)
{
    return orbital.n >= 1
        && orbital.l >= 0
        && orbital.l < orbital.n
        && std::abs(orbital.m) <= orbital.l;
}

int wrap(int value, int minimum, int maximum)
{
    if (value > maximum) return minimum;
    if (value < minimum) return maximum;
    return value;
}

void changeOrbital(Orbital& orbital, char quantumNumber, int direction)
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

float orbitalDensity(Vector3 p, const Orbital& orbital)
{
    constexpr double pi = 3.14159265358979323846;
    constexpr double rootTwo = 1.41421356237309504880;

    const double radius = std::sqrt(
        static_cast<double>(p.x) * p.x
        + static_cast<double>(p.y) * p.y
        + static_cast<double>(p.z) * p.z
    );
    const double rho = 2.0 * radius / orbital.n;
    const int radialDegree = orbital.n - orbital.l - 1;
    const int absM = std::abs(orbital.m);

    const double radialNorm = std::sqrt(
        std::pow(2.0 / orbital.n, 3)
        * std::tgamma(radialDegree + 1.0)
        / (2.0 * orbital.n * std::tgamma(orbital.n + orbital.l + 1.0))
    );
    const double radial = radialNorm
        * std::exp(-rho / 2.0)
        * std::pow(rho, orbital.l)
        * std::assoc_laguerre(radialDegree, 2 * orbital.l + 1, rho);

    const double cosTheta = radius > 0.0 ? p.z / radius : 1.0;
    const double phi = std::atan2(p.y, p.x);
    const double angularNorm = std::sqrt(
        (2.0 * orbital.l + 1.0) / (4.0 * pi)
        * std::tgamma(orbital.l - absM + 1.0)
        / std::tgamma(orbital.l + absM + 1.0)
    );
    const double legendre = std::assoc_legendre(orbital.l, absM, cosTheta);

    double angular = angularNorm * legendre;
    if (orbital.m > 0) angular *= rootTwo * std::cos(absM * phi);
    if (orbital.m < 0) angular *= rootTwo * std::sin(absM * phi);

    const double amplitude = radial * angular;
    return static_cast<float>(amplitude * amplitude);
}

std::vector<Particle> makeOrbitalCloud(
    std::size_t count,
    const Orbital& orbital
)
{
    assert(isValid(orbital));

    std::mt19937 random(42);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::normal_distribution<float> initial(
        0.0f,
        orbital.n * orbital.n * 0.5f
    );
    std::normal_distribution<float> step(0.0f, orbital.n * 0.75f);

    std::vector<Particle> particles;
    particles.reserve(count);

    Vector3 current{};
    float currentDensity = 0.0f;
    while (currentDensity < 0.000000000001f) {
        current = {initial(random), initial(random), initial(random)};
        currentDensity = orbitalDensity(current, orbital);
    }

    constexpr int burnIn = 4000;
    constexpr int thinning = 6;
    int iteration = 0;

    while (particles.size() < count) {
        const Vector3 candidate{
            current.x + step(random),
            current.y + step(random),
            current.z + step(random),
        };

        const float candidateDensity = orbitalDensity(candidate, orbital);
        const float acceptance = std::min(1.0f, candidateDensity / currentDensity);
        if (unit(random) < acceptance) {
            current = candidate;
            currentDensity = candidateDensity;
        }

        ++iteration;
        if (iteration <= burnIn || iteration % thinning != 0) {
            continue;
        }

        constexpr float displayScale = 0.38f;
        const Vector3 displayPosition{
            current.x * displayScale,
            current.y * displayScale,
            current.z * displayScale,
        };
        particles.push_back({displayPosition});
    }

    return particles;
}

std::vector<Matrix> makeTransforms(
    const std::vector<Particle>& particles,
    float sphereRadius
)
{
    std::vector<Matrix> transforms;
    transforms.reserve(particles.size());

    const Matrix scale = MatrixScale(sphereRadius, sphereRadius, sphereRadius);
    for (const Particle& particle : particles) {
        const Vector3& p = particle.position;
        const Matrix translation = MatrixTranslate(p.x, p.y, p.z);
        transforms.push_back(MatrixMultiply(scale, translation));
    }

    return transforms;
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
out vec4 finalColor;

void main()
{
    vec3 normal = normalize(fragNormal);
    vec3 lightDirection = normalize(vec3(0.4, 0.8, 0.3));
    vec3 viewDirection = normalize(viewPos - fragPosition);

    float diffuse = max(dot(normal, lightDirection), 0.0);
    float specular = pow(max(dot(reflect(-lightDirection, normal),
                                 viewDirection), 0.0), 24.0);
    float edge = smoothstep(1.0, 8.0, length(fragPosition));
    vec3 innerColor = vec3(1.0, 0.65, 0.2);
    vec3 outerColor = vec3(0.45, 0.12, 0.9);
    vec3 color = mix(innerColor, outerColor, edge);

    color *= 0.45 + 0.85 * diffuse;
    color += vec3(specular * 1.25);
    finalColor = vec4(color, 1.0);
}
)glsl";

int main()
{
    constexpr int screenWidth = 1600;
    constexpr int screenHeight = 900;
    Orbital orbital{4, 2, 2};

    assert(isValid(orbital));
    assert(!isValid({2, 2, 0}));
    assert(orbitalDensity({1.0f, 0.0f, 0.0f}, {2, 1, 0}) < 0.0001f);
    assert(orbitalDensity({0.0f, 0.0f, 1.0f}, {2, 1, 0}) > 0.0f);
    assert(orbitalDensity({2.0f, 0.0f, 0.0f}, {2, 0, 0}) < 0.000001f);

    Orbital controlTest{1, 0, 0};
    changeOrbital(controlTest, 'n', -1);
    assert(controlTest.n == 5);

    std::vector<Particle> particles =
        makeOrbitalCloud(30000, orbital);

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "Atomic Orbital Lab");
    SetTargetFPS(60);

    Mesh sphere = GenMeshSphere(1.0f, 6, 8);
    Shader shader = LoadShaderFromMemory(vertexShaderCode, fragmentShaderCode);
    shader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(shader, "mvp");
    shader.locs[SHADER_LOC_MATRIX_MODEL] =
        GetShaderLocationAttrib(shader, "instanceTransform");
    shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "viewPos");

    Material material = LoadMaterialDefault();
    material.shader = shader;
    std::vector<Matrix> transforms = makeTransforms(particles, 0.055f);

    Camera3D camera{};
    camera.position = {11.0f, 7.0f, 11.0f};
    camera.target = {0.0f, 0.0f, 0.0f};
    camera.up = {0.0f, 1.0f, 0.0f};
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    float cameraYaw = 0.8f;
    float cameraPitch = 0.45f;
    float cameraDistance = 16.0f;
    bool autoRotate = false;
    float autoRotationSpeed = 0.12f;

    while (!WindowShouldClose()) {
        const bool shiftHeld = IsKeyDown(KEY_LEFT_SHIFT)
            || IsKeyDown(KEY_RIGHT_SHIFT);

        if (IsKeyPressed(KEY_R)) {
            if (shiftHeld) {
                autoRotationSpeed = -autoRotationSpeed;
            } else {
                autoRotate = !autoRotate;
            }
        }

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            const Vector2 movement = GetMouseDelta();
            cameraYaw -= movement.x * 0.005f;
            cameraPitch += movement.y * 0.005f;
            cameraPitch = std::clamp(cameraPitch, -1.45f, 1.45f);
        } else if (autoRotate) {
            cameraYaw += autoRotationSpeed * GetFrameTime();
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
        }
        if (IsKeyPressed(KEY_L)) {
            changeOrbital(orbital, 'l', direction);
            orbitalChanged = true;
        }
        if (IsKeyPressed(KEY_M)) {
            changeOrbital(orbital, 'm', direction);
            orbitalChanged = true;
        }

        if (orbitalChanged) {
            particles = makeOrbitalCloud(30000, orbital);
            transforms = makeTransforms(particles, 0.055f);
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

        BeginDrawing();
        ClearBackground({3, 4, 10, 255});

        BeginMode3D(camera);
        DrawMeshInstanced(
            sphere,
            material,
            transforms.data(),
            static_cast<int>(transforms.size())
        );
        DrawSphere({0.0f, 0.0f, 0.0f}, 0.12f, GOLD);
        EndMode3D();

        DrawText(
            TextFormat(
                "n=%d  l=%d  m=%d  rotation=%s",
                orbital.n,
                orbital.l,
                orbital.m,
                autoRotate ? "auto" : "manual"
            ),
            16,
            16,
            10,
            DARKGRAY
        );
        DrawFPS(screenWidth - 100, 22);

        EndDrawing();
    }

    UnloadMesh(sphere);
    UnloadMaterial(material);
    CloseWindow();
    return 0;
}
