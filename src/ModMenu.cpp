#include "ParticleDisabler/ModMenu.h"
#include "ParticleDisabler/ParticleHook.h"

// TODO: include the real ModMenu registration headers, e.g.:
// #include <ModMenu/ModMenu.h>

namespace ParticleDisabler {

void ModMenu::Register() {
    // TODO: replace with actual ModMenu registration call, mirroring how
    // ZoomRewrite registers its toggles/sliders. Sketch of intended shape:
    //
    // ModMenu::AddCategory("Particle Disabler");
    // ModMenu::AddToggle("Disable Particles", /*default=*/false, OnToggleChanged);
}

void ModMenu::OnToggleChanged(bool enabled) {
    ParticleHook::SetDisabled(enabled);
}

} // namespace ParticleDisabler
