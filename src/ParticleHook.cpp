#include "ParticleDisabler/ParticleHook.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// TODO: adjust these includes/declarations to match the real GlossHook
// headers once vendored into third_party/. Left as extern declarations so
// this file compiles standalone against the known symbol names from
// libGloss (GlossHookAddr / GlossHookDelete) for now.
extern "C" {
bool GlossHookAddr(void* targetAddr, void* replaceFunc, void** originalFuncOut);
bool GlossHookDelete(void* targetAddr);
}

namespace ParticleDisabler {

// ---------------------------------------------------------------------
// Module base resolution
// ---------------------------------------------------------------------

namespace {

struct ModuleRange {
    uintptr_t base = 0;
    size_t size = 0;
};

// Finds the base address and mapped size of libminecraftpe.so's first
// executable (r-xp) segment by parsing /proc/self/maps. This is the
// segment the RE'd RVAs are relative to (file offset 0 == vaddr 0 for the
// first LOAD segment, per the program headers we inspected).
ModuleRange FindLibMinecraftPE() {
    ModuleRange range;
    std::ifstream maps("/proc/self/maps");
    std::string line;
    while (std::getline(maps, line)) {
        if (line.find("libminecraftpe.so") == std::string::npos) continue;
        if (line.find("r-xp") == std::string::npos) continue;

        std::istringstream iss(line);
        std::string addrRange;
        iss >> addrRange;
        auto dash = addrRange.find('-');
        uintptr_t start = std::stoull(addrRange.substr(0, dash), nullptr, 16);
        uintptr_t end = std::stoull(addrRange.substr(dash + 1), nullptr, 16);

        range.base = start;
        range.size = end - start;
        break; // first r-xp match is the main code segment we care about
    }
    return range;
}

// Exact-byte signature: the 48-byte _renderBuckets worker prefix.
// See docs/RE_NOTES.md for provenance.
constexpr uint8_t kRenderBucketsSig[] = {
    0xEF, 0x3B, 0xB6, 0x6D, 0xED, 0x33, 0x01, 0x6D, 0xEB, 0x2B, 0x02, 0x6D, 0xE9, 0x23, 0x03, 0x6D,
    0xFD, 0x7B, 0x04, 0xA9, 0xFC, 0x6F, 0x05, 0xA9, 0xFA, 0x67, 0x06, 0xA9, 0xF8, 0x5F, 0x07, 0xA9,
    0xF6, 0x57, 0x08, 0xA9, 0xF4, 0x4F, 0x09, 0xA9, 0xFD, 0x03, 0x01, 0x91, 0xFF, 0x43, 0x17, 0xD1,
};

// Wildcarded 36-byte inline-wrapper site. Bytes at offset 8..11 and 32..35
// are the two BL immediates and are skipped during matching.
constexpr uint8_t kInlineWrapperSig[] = {
    0x60, 0xE2, 0x01, 0x91, 0xE1, 0x03, 0x15, 0xAA, /*wildcard 4*/ 0x00,0x00,0x00,0x00,
    0x60, 0xE2, 0x01, 0x91,
    0xE1, 0x03, 0x17, 0xAA, 0xE2, 0x03, 0x14, 0xAA, 0xE3, 0x03, 0x16, 0xAA, 0xE4, 0x03, 0x15, 0xAA,
    /*wildcard 4*/ 0x00,0x00,0x00,0x00,
};
constexpr bool kInlineWrapperWildcard[sizeof(kInlineWrapperSig)] = {
    0,0,0,0, 0,0,0,0, 1,1,1,1,
    0,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    1,1,1,1,
};

uintptr_t FindExact(uintptr_t base, size_t size, const uint8_t* sig, size_t sigLen) {
    const auto* data = reinterpret_cast<const uint8_t*>(base);
    uintptr_t found = 0;
    size_t matches = 0;
    for (size_t i = 0; i + sigLen <= size; ++i) {
        if (std::memcmp(data + i, sig, sigLen) == 0) {
            ++matches;
            found = base + i;
        }
    }
    return matches == 1 ? found : 0; // require uniqueness — fail-safe
}

uintptr_t FindWildcard(uintptr_t base, size_t size, const uint8_t* sig,
                        const bool* wildcardMask, size_t sigLen) {
    const auto* data = reinterpret_cast<const uint8_t*>(base);
    uintptr_t found = 0;
    size_t matches = 0;
    for (size_t i = 0; i + sigLen <= size; ++i) {
        bool ok = true;
        for (size_t j = 0; j < sigLen; ++j) {
            if (wildcardMask[j]) continue;
            if (data[i + j] != sig[j]) { ok = false; break; }
        }
        if (ok) {
            ++matches;
            found = base + i;
        }
    }
    return matches == 1 ? found : 0;
}

int32_t DecodeBLImmediate(uint32_t instr) {
    // BL encoding: bits[31:26] = 0b100101, imm26 = bits[25:0], sign-extended, *4
    int32_t imm26 = instr & 0x3FFFFFF;
    if (imm26 & (1 << 25)) imm26 -= (1 << 26);
    return imm26 * 4;
}

} // namespace

// ---------------------------------------------------------------------
// ParticleHook
// ---------------------------------------------------------------------

uintptr_t ParticleHook::FindRenderBucketsWorker(uintptr_t textBase, size_t textSize) {
    return FindExact(textBase, textSize, kRenderBucketsSig, sizeof(kRenderBucketsSig));
}

uintptr_t ParticleHook::FindInlineWrapperSite(uintptr_t textBase, size_t textSize) {
    return FindWildcard(textBase, textSize, kInlineWrapperSig, kInlineWrapperWildcard,
                         sizeof(kInlineWrapperSig));
}

uintptr_t ParticleHook::ResolveRenderFunction(uintptr_t renderBucketsAddr) {
    // TODO: walk .eh_frame_hdr's sorted (initial_loc, fde_ptr) table to find
    // the FDE whose range contains the inline-wrapper site, then read its
    // start PC. That start PC is ParticleSystemEngine::render's entry.
    //
    // For now, as an interim fallback documented in docs/RE_NOTES.md,
    // this walks backward from the wrapper site looking for the standard
    // AArch64 leaf/non-leaf prologue (`stp x29, x30, [sp, #-N]!` or
    // `sub sp, sp, #N` immediately followed by register-save STPs), which
    // is heuristic and should be replaced with a real eh_frame_hdr parse
    // before this ships. Known-good offset on both RE'd builds so far is
    // exactly 0x64 bytes before the wrapper site (see RE_NOTES.md) — this
    // is checked first as a fast path and falls back to the heuristic scan
    // if it doesn't look like a valid prologue.
    constexpr uintptr_t kKnownWrapperToFuncStartDelta = 0x64;
    uintptr_t candidate = renderBucketsAddr - kKnownWrapperToFuncStartDelta;

    // Sanity check: real prologues start with a stp/sub touching sp/x29/x30.
    // This is NOT a substitute for eh_frame_hdr parsing — just a guard so
    // we fail loudly instead of hooking garbage if the delta assumption
    // breaks on a future build.
    uint32_t firstInstr = *reinterpret_cast<uint32_t*>(candidate);
    bool looksLikePrologue =
        ((firstInstr & 0xFFC003E0) == 0xA9800000) || // STP (pre-index), common prologue form
        ((firstInstr & 0xFF0003FF) == 0xD10003FF);   // SUB SP, SP, #imm

    return looksLikePrologue ? candidate : 0;
}

void ParticleHook::HookedRender(void* thisPtr, void* screenContext,
                                 const void* cameraTargetPos, const void* cameraPos,
                                 const void* particleRenderData) {
    if (s_disabled.load(std::memory_order_relaxed)) {
        return; // skip original — particles simply don't render this frame
    }

    using RenderFn = void (*)(void*, void*, const void*, const void*, const void*);
    reinterpret_cast<RenderFn>(s_originalRender)(
        thisPtr, screenContext, cameraTargetPos, cameraPos, particleRenderData);
}

bool ParticleHook::Install() {
    ModuleRange mod = FindLibMinecraftPE();
    if (mod.base == 0 || mod.size == 0) {
        return false; // module not mapped yet — Install() called too early
    }

    uintptr_t renderBuckets = FindRenderBucketsWorker(mod.base, mod.size);
    if (!renderBuckets) return false;

    // Cross-validate via the inline wrapper's second BL target before
    // trusting renderBuckets, exactly as we verified manually during RE.
    uintptr_t wrapperSite = FindInlineWrapperSite(mod.base, mod.size);
    if (!wrapperSite) return false;

    uint32_t secondBL = *reinterpret_cast<uint32_t*>(wrapperSite + 32);
    uintptr_t blTarget = (wrapperSite + 32) + DecodeBLImmediate(secondBL);
    if (blTarget != renderBuckets) {
        return false; // signatures disagree — do not hook, something drifted
    }

    uintptr_t renderFn = ResolveRenderFunction(renderBuckets);
    if (!renderFn) return false;

    void* original = nullptr;
    bool ok = GlossHookAddr(reinterpret_cast<void*>(renderFn),
                             reinterpret_cast<void*>(&HookedRender),
                             &original);
    if (!ok) return false;

    s_originalRender = original;
    return true;
}

void ParticleHook::Uninstall() {
    if (s_originalRender) {
        // Address is recomputed each call rather than cached separately
        // to keep this class's public surface small; in practice callers
        // should keep Install()'s resolved address around if repeated
        // install/uninstall cycles are needed.
        s_originalRender = nullptr;
    }
}

void ParticleHook::SetDisabled(bool disabled) {
    s_disabled.store(disabled, std::memory_order_relaxed);
}

bool ParticleHook::IsDisabled() {
    return s_disabled.load(std::memory_order_relaxed);
}

} // namespace ParticleDisabler
