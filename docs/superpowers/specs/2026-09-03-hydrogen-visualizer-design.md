# Hydrogenic Orbital Visualizer Design

## Purpose

Turn the current single-file visual prototype into a scientifically testable
hydrogenic orbital visualizer. It must distinguish complex `|n,l,m>`
eigenstates from real chemistry orbitals, render reproducible probability
samples interactively, and remain responsive at 1080p.

Success means exceeding the reference video in three ways:

1. Correct labels and a documented scientific model.
2. Reproducible numerical checks rather than visually plausible shapes alone.
3. Depth-correct GPU rendering with phase-aware color and restrained bloom.

## Scope

The first production milestone covers non-relativistic, one-electron,
hydrogenic systems in atomic units:

- nuclear charge `Z >= 1`;
- stationary states with `1 <= n <= 8`, `0 <= l < n`, and `-l <= m <= l`;
- complex spherical-harmonic eigenstates;
- named real orbitals formed from valid linear combinations;
- probability sampling and static visualization.

The following are explicitly deferred:

- multi-electron atoms and electron-electron interactions;
- spin, fine structure, external fields, and relativistic corrections;
- molecular orbitals;
- time evolution and superpositions of different energies.

## Scientific Model

For a complex stationary state:

```text
psi_nlm(r, theta, phi) = R_nl(r; Z) * Y_lm(theta, phi)
density = |psi_nlm|^2
```

The evaluator returns a complex amplitude. Probability density is derived from
that amplitude rather than calculated through a separate approximation.

Real orbitals are separate domain values. For example, `d_x2-y2` is represented
as a normalized real combination of the `m = +2` and `m = -2` complex states.
The UI displays the real-orbital name, not a misleading single `m` value.

Physical positions remain in Bohr radii. A renderer-owned scale converts them
to world coordinates. Rendered sphere radius, lighting, and bloom carry no
physical meaning.

## Modules And Interfaces

### Quantum module

Owns quantum numbers, basis selection, validation, orbital naming, and the
wavefunction implementation. It has no raylib dependency.

```cpp
bool isValid(const QuantumState& state);
std::complex<double> wavefunction(PositionAu position,
                                  const QuantumState& state);
double probabilityDensity(PositionAu position,
                          const QuantumState& state);
```

This is the scientific seam. Tests use the same interface as the sampler.

### Sampler module

Draws positions from `probabilityDensity()` and reports diagnostics.

```cpp
SampleSet sample(const QuantumState& state, const SamplingConfig& config);
```

Each probability sample contains its physical position and wavefunction phase.
`SampleSet` also contains seed, chain count, attempted moves, accepted moves,
acceptance rate, burn-in, and thinning. The initial adapter is multi-chain
Metropolis-Hastings. A different sampler may replace it only if measurements
show a correctness or performance problem.

### Renderer module

Owns raylib resources, conversion from atomic to world coordinates, sphere
mesh instancing, shaders, lighting, and post-processing.

```cpp
void setSamples(const SampleSet& samples);
void draw(const CameraState& camera, const RenderSettings& settings);
```

The renderer never evaluates a wavefunction. It receives finished samples and
cannot change their scientific distribution.

### Application module

Owns the selected state, input mapping, camera, regeneration decisions, and
the frame loop. It requests new samples only after scientific state or sampler
settings change. Camera and visual-setting changes do not regenerate samples.

## Data Flow

```text
keyboard changes QuantumState
        -> validate state
        -> Sampler calls Quantum probabilityDensity
        -> SampleSet plus diagnostics
        -> Renderer uploads instance transforms
        -> camera/render loop reuses GPU data every frame
```

This separates expensive scientific work from per-frame rendering.

## Scientific Verification

The quantum module must pass deterministic checks before visual work continues:

- allowed and forbidden `n/l/m` combinations;
- `|psi_100(0)|^2 = Z^3/pi` in atomic units;
- numerical normalization integral approximately equals one;
- radial node count equals `n - l - 1`;
- known nodal planes for selected `p` and `d` real orbitals;
- real-orbital combinations preserve normalization;
- probability density is non-negative and finite.

Sampler verification uses fixed seeds and checks:

- repeatability for the same seed;
- all coordinates and diagnostics are finite;
- multiple chains contribute samples;
- sampled mean radius agrees, within Monte Carlo tolerance, with
  `<r> = (3n^2 - l(l+1)) / (2Z)` in atomic units.

Tests check properties and observables, not exact random point sequences beyond
the explicit repeatability test.

## Rendering

The base renderer keeps the current instanced low-poly spheres and depth
buffer. The next visual stages are:

1. Carry complex phase or real-wavefunction sign with every sample.
2. Group transforms into a bounded number of hue bins and render one instanced
   batch per bin; sample density continues to represent probability.
3. Render the scene to a texture and apply a restrained bloom pass.
4. Add a node/slice view without changing the underlying samples.
5. Measure 1080p frame time before raising mesh detail or sample count.

The renderer must not encode probability simultaneously through arbitrary
sphere size and sample density. Sphere size remains a legibility setting.

## Input

- touchpad drag: orbit camera;
- two-finger scroll: zoom;
- `R`: toggle slow automatic rotation;
- `Shift+R`: reverse automatic rotation;
- `N`, `L`, `M`: cycle quantum numbers;
- basis toggle: switch between complex eigenstates and named real orbitals.

Invalid combinations are corrected before sampling. Regeneration may stay
synchronous while measured latency remains below 100 ms. Background generation
is added only if the measured latency exceeds that threshold.

## File Layout

```text
src/
  main.cpp
  quantum.hpp
  quantum.cpp
  sampler.hpp
  sampler.cpp
  renderer.hpp
  renderer.cpp
shaders/
  orbital.vert
  orbital.frag
tests/
  quantum_test.cpp
  sampler_test.cpp
CMakeLists.txt
```

No framework or dependency is added beyond the C++ standard library, raylib,
and CMake/CTest.

## Migration Order

1. Add CMake and extract the pure quantum module with scientific tests.
2. Return complex amplitudes and distinguish complex versus real bases.
3. Extract the sampler, add multiple chains and diagnostics, and test mean
   radius.
4. Extract the renderer and move GLSL into shader files.
5. Add per-instance phase/sign data and phase-aware color.
6. Add measured bloom and node/slice visualization.
7. Design time-dependent superpositions as a separate milestone.

## Scientific Motion

The selected `n/l/m/Z` state remains fixed until explicit keyboard input.
Motion represents Monte Carlo walkers sampling the stationary density, not
electron trajectories. Each walker advances with a Metropolis-adjusted
Langevin proposal targeting `rho = |psi|^2`:

```text
y = x + 0.5 h^2 grad(log(rho(x))) + h N(0, I)
```

The Metropolis-Hastings correction uses both forward and reverse proposal
densities, so finite step size does not change the target distribution.
The logarithmic gradient is evaluated by deterministic central differences.

Complex eigenstates also expose the probability-current velocity. In atomic
units, the `exp(i m phi)` phase gives:

```text
v = j / rho = m (-y, x, 0) / (x^2 + y^2)
```

It is regularized to zero on the axis. Probability flow is applied after each
MALA update as an exact azimuthal rotation. This operator-split step preserves
`r`, `theta`, volume, and therefore `rho`, while retaining directed current;
putting it inside ordinary Metropolis-Hastings would incorrectly restore
detailed balance and erase the net flow.

The sampler updates a bounded batch of walkers per frame. The presentation
layer eases rendered positions toward accepted sample positions. Long jumps
fade out and re-enter from outside the cloud instead of drawing misleading
paths through nodal regions. Opacity transitions are explicitly visual and do
not alter sampler statistics.

The default mode continuously advances walkers. `n/l/m` never cycle
automatically. Manual state changes reseed all chains. Diagnostics show the
proposal acceptance rate, and phase-aware color displays `arg(psi)` without
encoding probability a second time.
