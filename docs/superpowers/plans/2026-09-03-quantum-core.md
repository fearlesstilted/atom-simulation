# Quantum Core Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extract a raylib-independent hydrogenic wavefunction module, verify its scientific invariants, and reconnect the existing visualizer without changing its visible controls.

**Architecture:** The pure `quantum` module owns states, validation, complex amplitudes, and probability density. The raylib application consumes only its small public interface; numerical verification lives in a standalone CTest executable.

**Tech Stack:** C++17 standard library, CMake/CTest, raylib 5.5 for the application only.

**Spec:** `docs/superpowers/specs/2026-09-03-hydrogen-visualizer-design.md`

## Global Constraints

- Model only non-relativistic one-electron hydrogenic systems in atomic units.
- Support complex eigenstates through `1 <= n <= 8` and `Z >= 1`.
- Add real chemistry orbitals in the next plan, after this complex basis is verified.
- Keep the quantum module independent of raylib and rendering scale.
- Preserve the current executable and controls after migration.
- Add no testing framework; one CTest executable and standard-library checks are enough.
- Do not commit from the dirty parent repository. Repository setup is a separate owner decision.

---

### Task 1: Establish A Reproducible Build

**Files:**
- Create: `CMakeLists.txt`
- Move: `main.cpp` to `src/main.cpp`

**Interfaces:**
- Consumes: system raylib 5.5 discovered by CMake.
- Produces: `atom` executable and a configured CTest project.

- [x] **Step 1: Install the missing build tool**

Run:

```bash
sudo dnf install cmake
```

Verify:

```bash
cmake --version
```

- [x] **Step 2: Move the existing application without editing its behavior**

```bash
mkdir -p src tests build
mv main.cpp src/main.cpp
```

- [x] **Step 3: Create the baseline CMake project**

```cmake
cmake_minimum_required(VERSION 3.20)
project(hydrogen_visualizer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(raylib 5.5 REQUIRED)

add_executable(atom src/main.cpp)
target_link_libraries(atom PRIVATE raylib)
target_compile_options(atom PRIVATE -Wall -Wextra -pedantic)

include(CTest)
```

- [x] **Step 4: Build and smoke-test the unchanged application**

Run:

```bash
cmake -S . -B build
cmake --build build
timeout 3 ./build/atom
```

Expected: compilation succeeds without warnings; raylib initializes; `timeout`
ends the interactive program with status 124.

---

### Task 2: Define And Validate Complex Eigenstates

**Files:**
- Create: `src/quantum.hpp`
- Create: `src/quantum.cpp`
- Create: `tests/quantum_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: atomic-unit positions and quantum numbers.
- Produces:

```cpp
namespace quantum {
struct PositionAu { double x; double y; double z; };
struct ComplexState { int n; int l; int m; int nuclearCharge; };
bool isValid(const ComplexState& state);
}
```

- [x] **Step 1: Write validation tests before the implementation**

Create a tiny test runner with a `check(bool, const char*)` helper that prints
the message and returns a nonzero process status on failure. Cover exactly:

```cpp
check(isValid({1, 0, 0, 1}), "hydrogen 1s is valid");
check(isValid({8, 7, -7, 2}), "upper supported state is valid");
check(!isValid({0, 0, 0, 1}), "n starts at one");
check(!isValid({9, 0, 0, 1}), "n stops at eight");
check(!isValid({2, 2, 0, 1}), "l must be below n");
check(!isValid({2, 1, 2, 1}), "absolute m cannot exceed l");
check(!isValid({1, 0, 0, 0}), "nuclear charge is positive");
```

- [x] **Step 2: Register the failing test**

Add:

```cmake
add_library(quantum src/quantum.cpp)
target_include_directories(quantum PUBLIC src)
target_compile_options(quantum PRIVATE -Wall -Wextra -pedantic)

add_executable(quantum_test tests/quantum_test.cpp)
target_link_libraries(quantum_test PRIVATE quantum)
add_test(NAME quantum_test COMMAND quantum_test)
```

Run `cmake --build build && ctest --test-dir build --output-on-failure`.
Expected: build fails because the declared interface has no implementation.

- [x] **Step 3: Implement only state validation**

```cpp
bool isValid(const ComplexState& state)
{
    return state.n >= 1 && state.n <= 8
        && state.l >= 0 && state.l < state.n
        && std::abs(state.m) <= state.l
        && state.nuclearCharge >= 1;
}
```

- [x] **Step 4: Verify the validation contract**

Run:

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: `quantum_test` passes.

---

### Task 3: Implement The Complex Hydrogenic Wavefunction

**Files:**
- Modify: `src/quantum.hpp`
- Modify: `src/quantum.cpp`
- Modify: `tests/quantum_test.cpp`

**Interfaces:**
- Consumes: valid `ComplexState` and `PositionAu`.
- Produces:

```cpp
std::complex<double> wavefunction(PositionAu position,
                                  const ComplexState& state);
double probabilityDensity(PositionAu position,
                          const ComplexState& state);
```

- [x] **Step 1: Add failing closed-form checks**

```cpp
constexpr double pi = 3.14159265358979323846;
checkNear(probabilityDensity({0, 0, 0}, {1, 0, 0, 1}),
          1.0 / pi, 1e-12, "1s density at origin");
checkNear(probabilityDensity({2, 0, 0}, {2, 0, 0, 1}),
          0.0, 1e-12, "2s radial node");
checkNear(probabilityDensity({1, 0, 0}, {2, 1, 0, 1}),
          0.0, 1e-12, "2p m0 angular node");
check(std::isfinite(probabilityDensity({3, -2, 1}, {4, 2, 2, 1})),
      "density remains finite");
const auto yPositive = wavefunction({1, 2, 3}, {3, 2, 1, 1});
const auto yNegative = wavefunction({1, 2, 3}, {3, 2, -1, 1});
checkNearComplex(yNegative, -std::conj(yPositive), 1e-12,
                 "negative m conjugation relation");
```

Run the test. Expected: compilation fails because both functions are missing.

- [x] **Step 2: Implement the radial amplitude**

Use `rho = 2 Z r / n`, `std::assoc_laguerre`, and `std::tgamma`:

```cpp
const double norm = std::sqrt(
    std::pow(2.0 * z / n, 3)
    * std::tgamma(n - l)
    / (2.0 * n * std::tgamma(n + l + 1.0))
);
const double radial = norm * std::exp(-rho / 2.0)
    * std::pow(rho, l)
    * std::assoc_laguerre(n - l - 1, 2 * l + 1, rho);
```

- [x] **Step 3: Implement the complex angular amplitude**

Use the normalized associated Legendre function and preserve `exp(i*m*phi)`:

```cpp
const int absM = std::abs(m);
const double norm = std::sqrt(
    (2.0 * l + 1.0) / (4.0 * pi)
    * std::tgamma(l - absM + 1.0)
    / std::tgamma(l + absM + 1.0)
);
const double magnitude = norm * std::assoc_legendre(l, absM, cosTheta);
const std::complex<double> phase = std::polar(1.0, m * phi);
```

`std::assoc_legendre` omits the Condon-Shortley phase. Restore it and construct
negative `m` from the standard conjugation relation:

```cpp
const double condonShortley = absM % 2 == 0 ? 1.0 : -1.0;
const auto positive = condonShortley * magnitude
    * std::polar(1.0, absM * phi);
const auto angular = m >= 0
    ? positive
    : condonShortley * std::conj(positive);
```

Return `radial * angular`; derive density only with
`std::norm(wavefunction)`.

- [x] **Step 4: Run the closed-form checks**

Run `cmake --build build && ctest --test-dir build --output-on-failure`.
Expected: all checks pass with no raylib linkage in `quantum_test`.

---

### Task 4: Verify Normalization, Nodes, And Mean Radius Numerically

**Files:**
- Modify: `tests/quantum_test.cpp`

**Interfaces:**
- Consumes: `probabilityDensity()` only.
- Produces: deterministic scientific regression checks; no production types.

- [x] **Step 1: Add a deterministic spherical quadrature helper in the test**

Integrate with midpoint bins over `r`, `cos(theta)`, and `phi`; include the
Jacobian `r*r`. Use `rMax = 8.0 * n * n / Z`, 320 radial bins, 48 cosine bins,
and 64 azimuth bins. Return both normalization and density-weighted radius.

- [x] **Step 2: Add normalization and expectation checks**

For states `1s`, `2s`, `2p(m=0)`, and `4d(m=2)`:

```cpp
checkNear(integral.normalization, 1.0, 0.02, "state is normalized");
const double expectedRadius =
    (3.0 * n * n - l * (l + 1.0)) / (2.0 * nuclearCharge);
checkNear(integral.meanRadius, expectedRadius, 0.05 * expectedRadius,
          "mean radius matches hydrogenic expectation");
```

- [x] **Step 3: Verify the test fails for an intentional normalization error**

Temporarily multiply the production radial normalization by `1.1`, run CTest,
and confirm the normalization check fails. Revert that one-line mutation.

- [x] **Step 4: Run the restored scientific suite**

Run:

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: all states pass normalization and mean-radius tolerances.

---

### Task 5: Reconnect The Visualizer To The Quantum Module

**Files:**
- Modify: `src/main.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `quantum::ComplexState`, `quantum::PositionAu`,
  `quantum::isValid()`, and `quantum::probabilityDensity()`.
- Produces: unchanged `atom` user experience backed by the tested module.

- [ ] **Step 1: Link the application to the quantum library**

```cmake
target_link_libraries(atom PRIVATE raylib quantum)
```

- [ ] **Step 2: Replace duplicate domain declarations**

Delete `Orbital`, `isValid()`, and `orbitalDensity()` from `src/main.cpp`.
Include `quantum.hpp`, use `quantum::ComplexState`, and convert the sampler's
candidate coordinates to `quantum::PositionAu` at its one call site.

- [ ] **Step 3: Preserve control invariants**

Keep the existing `N/L/M` behavior, but call `quantum::isValid()` after each
change. Keep camera-only changes independent from sample regeneration.

- [ ] **Step 4: Run all verification**

```bash
cmake --build build
ctest --test-dir build --output-on-failure
timeout 3 ./build/atom
```

Expected: scientific tests pass, shaders compile, the window opens, and no
compiler warnings are emitted.

- [ ] **Step 5: Confirm the deletion test**

Search:

```bash
rg "assoc_laguerre|assoc_legendre|probabilityDensity" src
```

Expected: special-function implementation appears only in `quantum.cpp`;
`main.cpp` contains only the call to `probabilityDensity()`.
