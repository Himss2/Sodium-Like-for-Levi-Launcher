#pragma once

namespace ParticleDisabler {

// Mod lifecycle entry points, mirroring the load()/enable() split used in
// ZoomRewrite (hooks are installed in enable(), not load(), to avoid the
// SIGSEGV class of bug we hit before — ModMenu/engine state isn't ready
// yet at load() time).
class Core {
public:
    static void Load();
    static void Enable();
    static void Disable();
};

} // namespace ParticleDisabler
