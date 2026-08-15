# ParticleDisabler

Standalone LeviLaunchroid mod for Minecraft Bedrock (arm64-v8a) that disables
particle rendering entirely. Built as a standalone module first; once proven
working on-device, it's meant to be folded into
[BedrockTools-custom](../BedrockTools-custom) as an additional module
alongside `entityculling`, `particleoptimizer`, and `outlinergb`.

## How it works

Rather than touching particle *data* (spawn rates, bucket contents, etc.),
this hooks `ParticleSystemEngine::render` directly and skips the call
entirely while the toggle is on. It's the smallest, lowest-risk function in
the particle render chain — one caller, no internal state to understand.

The hook target isn't hardcoded: it's resolved at runtime via a byte-pattern
scan of `libminecraftpe.so`, cross-validated against a second, independent
signature before installing. If either signature fails to resolve uniquely,
the mod stays inert instead of hooking a possibly-wrong address.

Full RE trail, byte signatures, and the procedure for re-resolving the hook
target after a game update: see [`docs/RE_NOTES.md`](docs/RE_NOTES.md).

## Status

- [x] Hook target identified and RE'd for MC 1.26.44 (cross-validated against
      1.26.40.5 community findings)
- [ ] Signature scan + GlossHook install wired up and tested on-device
- [ ] `eh_frame_hdr`-based function boundary resolution (currently a
      fixed-offset fast path with a prologue sanity check — see TODO in
      `ParticleHook.cpp`)
- [ ] ModMenu toggle wired to real API
- [ ] Verified stable across a play session
- [ ] Ported into BedrockTools-custom as a module

## Building

Requires Android NDK (r27c tested) and CMake ≥ 3.22.
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake 
-DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24
cmake --build build
CI builds automatically on push via `.github/workflows/build.yml`.

## Layout
include/ParticleDisabler/   public headers
src/                         implementation
docs/RE_NOTES.md             RE findings, signatures, re-resolution steps
