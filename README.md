# Atomic Orbital Lab

Interactive C++/raylib visualizer for hydrogenic wavefunctions. It evaluates
the analytical state

```text
psi_nlm(r, theta, phi) = R_nl(r) Y_lm(theta, phi)
```

and uses a Metropolis-adjusted Langevin sampler to draw 30,000 positions from
the Born probability density `|psi|^2`. Color represents complex phase. The
project supports stationary eigenstates, time-dependent two-state
superpositions, probability-current motion, and named real `p`/`d` orbitals.

Rendered points are probability samples, not electrons or electron paths.
Particle size, transition speed, and glow are visualization choices rather
than physical observables. The model omits spin, relativistic corrections,
external fields, and electron-electron interactions.

## Build

Requires a C++17 compiler, CMake 3.20+, and raylib 5.5.

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/atom
```

Use `./tools/dev-run.sh` for automatic rebuild, test, and state-preserving
restart while editing. GLSL shaders reload inside the running process.

## Controls

| Input | Action |
|---|---|
| Touchpad drag | Orbit camera |
| Two-finger scroll | Zoom |
| `N`, `L`, `M` | Change quantum numbers |
| `Shift` + control | Reverse direction |
| `S` | Toggle time-dependent superposition |
| `B` | Toggle complex/real basis |
| `O` | Cycle named real orbital |
| `D` | Toggle automatic state sequence |
| `R` | Toggle camera rotation |
| `Shift+R` | Reverse camera rotation |
