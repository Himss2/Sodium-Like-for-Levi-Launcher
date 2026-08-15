# Reverse Engineering Notes — ParticleSystemEngine::render

Target: `libminecraftpe.so`, arm64-v8a

This document tracks the hook target across Minecraft Bedrock versions.
Update this file every time a new game version requires re-scanning.

## Known builds

| MC Version | Build ID (sha1) | Status |
|---|---|---|
| 1.26.40.5 | `5893edc8d56c93cbdb50e0f9436320236b78c89d` | RE'd by community (ChatGPT-assisted), verified by us |
| 1.26.44   | `b480c79a54f33d6e4f0d63a131673e3daf749911` | RE'd by us directly, cross-validated |

## Function chain
LevelRenderer lambda (particle draw call site)
-> ParticleSystemEngine::render(ScreenContext&, Vec3 cameraTargetPos, Vec3 cameraPos, ParticleRenderData const&)
[inlined] ParticleRenderer high-level body (this->mParticleRenderer @ +0x78)
-> bucket-prep worker(ParticleRenderer*, ParticleRenderData const&)
-> _renderBuckets worker(ParticleRenderer*, ScreenContext&, Vec3, Vec3, ParticleRenderData const&)
`mParticleRenderer` member offset inside `ParticleSystemEngine`: **+0x78** (stable across both builds).

## Addresses per version

### 1.26.40.5 (from community RE doc)

| Symbol | RVA | Size |
|---|---|---|
| `ParticleSystemEngine::render` | `0x0A76898C` | `0xD8` |
| inlined wrapper body (start) | `0x0A7689F0` | — |
| bucket-prep worker | `0x0A764240` | `0x64C` |
| `_renderBuckets` worker | `0x0A76488C` | `0x1DF4` |
| caller (LevelRenderer lambda) | `0x0AE605C4` | — |

### 1.26.44 (RE'd directly by us, verified twice)

| Symbol | RVA | Size |
|---|---|---|
| `ParticleSystemEngine::render` | `0x0A769C4C` | `0xD8` (identical) |
| inlined wrapper body (start) | `0x0A769CB0` | — |
| bucket-prep worker | `0x0A765500` | `0x64C` (identical) |
| `_renderBuckets` worker | `0x0A765B4C` | `0x1DF4` (identical) |
| caller (LevelRenderer lambda) | `0x0AE61C38` | — |
| unique caller count | 1 | verified via full `.text` BL scan |

All three function sizes are byte-identical between 1.26.40.5 and 1.26.44 — the
particle render path was not touched in this update. Only the surrounding
code shifted, moving the absolute RVAs by a constant-ish delta.

## Signatures used for scanning

These are used by `ParticleHook::FindTarget()` at runtime instead of hardcoding RVAs,
so the mod can (in principle) re-resolve itself on future game updates without a
manual RE pass — as long as the compiler output for this function doesn't change.

**`_renderBuckets` worker prefix (exact, 48 bytes, no wildcards):**
EF 3B B6 6D ED 33 01 6D EB 2B 02 6D E9 23 03 6D
FD 7B 04 A9 FC 6F 05 A9 FA 67 06 A9 F8 5F 07 A9
F6 57 08 A9 F4 4F 09 A9 FD 03 01 91 FF 43 17 D1
Matched exactly once in both 1.26.40.5 and 1.26.44 binaries.

**Inlined wrapper site (wildcarded BL targets, 36 bytes):**
60 E2 01 91 E1 03 15 AA ?? ?? ?? ?? 60 E2 01 91
E1 03 17 AA E2 03 14 AA E3 03 16 AA E4 03 15 AA
?? ?? ?? ??
This is `ADD X0,X19,#0x78 / MOV X1,X21 / BL <bucket-prep> / ADD X0,X19,#0x78 /
MOV X1,X23 / MOV X2,X20 / MOV X3,X22 / MOV X4,X21 / BL <_renderBuckets>`.
Matched exactly once in both builds.

## Chosen hook point

**`ParticleSystemEngine::render` (function start, e.g. `0x0A769C4C` on 1.26.44).**

Rationale: smallest function in the chain (216 bytes), has exactly one caller,
and requires no understanding of `ParticleRenderData`/bucket-map internals.
When the toggle is enabled, the hook returns immediately before doing any
work; when disabled, it calls through to the original function unchanged.

We deliberately did NOT hook `_renderBuckets` or the bucket-prep worker —
those operate on internal bucket-map state that would need much deeper RE
to touch safely, and we don't need it for a full particle disable.

## How to re-resolve on a new game version

1. Get the new `libminecraftpe.so`, note its Build ID (`readelf -n`).
2. Search for the `_renderBuckets` 48-byte exact prefix signature above.
   If it matches uniquely, that address is `_renderBuckets` for the new build.
3. Use the eh_frame FDE table to get that function's start/end (confirms size
   is still `0x1DF4`; if size changed, the function was modified — stop and
   re-RE manually).
4. Search for the wildcarded inline-wrapper signature; decode the two BL
   immediates. The second BL target should equal the address found in step 2
   (cross-validation).
5. Use eh_frame again on the wrapper's parent FDE to get
   `ParticleSystemEngine::render`'s start/end — that start address is the
   new hook target.
6. Update the table above and bump `MC_VERSION_TESTED` in `ParticleHook.cpp`.
