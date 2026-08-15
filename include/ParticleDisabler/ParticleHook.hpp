#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace ParticleDisabler {

// Hooks ParticleSystemEngine::render and skips it entirely while disabled.
// See docs/RE_NOTES.md for the full RE trail and how to re-resolve the
// target address on future game versions.
class ParticleHook {
public:
    // Locates the hook target at runtime via signature scan (does not use
    // a hardcoded RVA) and installs the GlossHook trampoline.
    // Returns false (and does nothing further) if any signature fails to
    // resolve uniquely — fail-safe rather than hooking a wrong address.
    static bool Install();

    static void Uninstall();

    // Runtime toggle, flipped from ModMenu.
    static void SetDisabled(bool disabled);
    static bool IsDisabled();

private:
    // Finds the unique 48-byte exact prefix of the `_renderBuckets` worker.
    // Returns nullptr if not found or not unique.
    static uintptr_t FindRenderBucketsWorker(uintptr_t textBase, size_t textSize);

    // Finds the inlined-wrapper call site (wildcarded BL targets) and
    // decodes its second BL immediate. Used only to cross-validate
    // FindRenderBucketsWorker() before trusting the result.
    static uintptr_t FindInlineWrapperSite(uintptr_t textBase, size_t textSize);

    // Given the validated _renderBuckets address, walks the eh_frame FDE
    // table to find the enclosing ParticleSystemEngine::render function
    // (the actual hook target).
    static uintptr_t ResolveRenderFunction(uintptr_t renderBucketsAddr);

    // The trampoline installed via GlossHook. Skips straight to
    // s_originalRender when disabled is false; when true, returns
    // immediately without calling through.
    static void HookedRender(void* thisPtr, void* screenContext,
                              const void* cameraTargetPos, const void* cameraPos,
                              const void* particleRenderData);

    static inline std::atomic<bool> s_disabled{false};
    static inline void* s_originalRender = nullptr;
    static inline void* s_hookHandle = nullptr;
};

} // namespace ParticleDisabler
