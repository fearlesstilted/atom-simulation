# Domain Glossary

## Hydrogenic system

A one-electron atom or ion with nuclear charge `Z`. Hydrogen has `Z = 1`.
The project does not model electron-electron interactions.

## Quantum state

A normalized wavefunction describing one electron. A stationary hydrogenic
eigenstate is identified by the quantum numbers `n`, `l`, and `m`.

## Complex eigenstate

The state whose angular part is the complex spherical harmonic `Y_l^m`.
Its probability density is `|psi|^2`. Color may represent complex phase, but
phase does not change probability density for a stationary state.

## Real orbital

A real-valued linear combination of complex eigenstates, named using chemistry
notation such as `p_x`, `d_xy`, or `d_x2-y2`. A real orbital must not be labeled
as though it were one pure nonzero-`m` eigenstate.

## Probability density

`|psi(x, y, z)|^2`, measured per unit volume. Its integral over all space is
one for a normalized state.

## Probability sample

One position drawn from the probability density. A rendered sphere represents
a sample, not a physical electron or the physical size of an electron.

## Probability current velocity

`Im(conj(psi) * grad(psi)) / |psi|^2` in atomic units. It describes the
flow of probability, not an electron trajectory. It is zero where density is
numerically indistinguishable from zero.

## Node

A surface or radius where the wavefunction and probability density are zero.
A hydrogenic state has `n - l - 1` radial nodes.

## Atomic units

The internal unit system. Distances are expressed in Bohr radii `a0`; display
scaling is a rendering concern and does not alter physical coordinates.
