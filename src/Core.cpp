#include "ParticleDisabler/Core.h"
#include "ParticleDisabler/ModMenu.h"
#include "ParticleDisabler/ParticleHook.h"

#include <android/log.h>

#define LOG_TAG "ParticleDisabler"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace ParticleDisabler {

void Core::Load() {
    // Intentionally empty — see ZoomRewrite's CameraHook::Install() note:
    // installing hooks here (before the engine/ModMenu is fully up) caused
    // a SIGSEGV previously. Hooks go in Enable() instead.
    LOGI("ParticleDisabler loaded");
}

void Core::Enable() {
    if (!ParticleHook::Install()) {
        LOGE("Failed to install ParticleHook - signatures did not resolve "
             "uniquely or cross-validation failed. See docs/RE_NOTES.md for "
             "how to re-resolve on this game version. Mod will stay inert.");
        return;
    }
    ModMenu::Register();
    LOGI("ParticleDisabler enabled");
}

void Core::Disable() {
    ParticleHook::Uninstall();
    LOGI("ParticleDisabler disabled");
}

} // namespace ParticleDisabler
