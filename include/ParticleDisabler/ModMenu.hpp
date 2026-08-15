#pragma once

namespace ParticleDisabler {

// Registers the "Particle Disabler" toggle in LeviLaunchroid's ModMenu.
// TODO: swap the body of these functions for the real ModMenu API once
// this is wired up against the actual SDK headers (kept generic here so
// this compiles as a skeleton without the full LeviLaunchroid tree).
class ModMenu {
public:
    static void Register();

private:
    static void OnToggleChanged(bool enabled);
};

} // namespace ParticleDisabler
