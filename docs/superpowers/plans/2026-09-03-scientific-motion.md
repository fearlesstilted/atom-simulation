# Scientific Motion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Animate a fixed hydrogenic state with verified MALA walkers, probability-current drift, phase color, and honest visual transitions.

**Architecture:** `quantum` owns wavefunction-derived quantities. A raylib-independent `sampler` owns Markov states and diagnostics. `main` owns only input, camera, easing, fading, and drawing.

**Tech Stack:** C++17 standard library, CMake/CTest, raylib 5.5.

**Spec:** `docs/superpowers/specs/2026-09-03-hydrogen-visualizer-design.md`

## Global Constraints

- Keep `n/l/m/Z` fixed until explicit input.
- Label moving points as probability samples, never electron trajectories.
- Use fixed seeds in tests and no new dependencies.
- Update walkers in bounded per-frame batches.
- Keep quantum and sampler modules independent of raylib.

---

### Task 1: Probability Current

**Files:** Modify `src/quantum.hpp`, `src/quantum.cpp`, and `tests/quantum_test.cpp`.

- [ ] Add tests proving zero current for `m=0`, tangential flow, and reversal when `m` changes sign.
- [ ] Add `probabilityCurrentVelocity(PositionAu, const ComplexState&)` in atomic units.
- [ ] Build and run CTest.
- [ ] Commit the verified quantum change.

### Task 2: MALA Sampler

**Files:** Create `src/sampler.hpp`, `src/sampler.cpp`, and `tests/sampler_test.cpp`; modify `CMakeLists.txt`.

- [ ] Define `SamplerConfig`, `Walker`, `Diagnostics`, and `Sampler` with `reset`, `advance`, `walkers`, and `diagnostics`.
- [ ] Add deterministic tests for repeatability, finite positions, acceptance bounds, movement, and sampled mean radius.
- [ ] Implement central-difference `grad(log rho)`, asymmetric proposal correction, and batched updates.
- [ ] Build and run all CTest tests.
- [ ] Commit the verified sampler.

### Task 3: Animated Presentation

**Files:** Modify `src/main.cpp` and `CMakeLists.txt`.

- [ ] Replace static cloud generation with `Sampler` and one visual particle per walker.
- [ ] Ease nearby accepted samples; fade long jumps out, respawn outside, and fade in.
- [ ] Preserve manual state and camera controls; reset only after `n/l/m` changes.
- [ ] Display state, sample meaning, acceptance rate, and motion status.
- [ ] Build, run CTest, and smoke-test the window.
- [ ] Commit the integrated motion.

### Task 4: Phase-Aware Rendering

**Files:** Modify `src/main.cpp`.

- [ ] Compute `arg(psi)` for each displayed sample and group transforms into twelve hue bins.
- [ ] Render bounded instanced batches with an emissive phase-color uniform.
- [ ] Verify stable 60 FPS, readable labels, clean shutdown, and all tests.
- [ ] Commit and push the completed milestone.
