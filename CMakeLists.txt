cmake_minimum_required(VERSION 3.22)
project(ParticleDisabler CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT ANDROID_ABI)
    set(ANDROID_ABI "arm64-v8a")
endif()

# Kept as a plain STATIC lib target so this can later be dropped straight
# into BedrockTools-custom's build as a module, rather than a standalone
# .so — but also buildable on its own as a shared object for testing.
option(PARTICLE_DISABLER_STANDALONE "Build as its own loadable .so" ON)

file(GLOB_RECURSE PD_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

if(PARTICLE_DISABLER_STANDALONE)
    add_library(ParticleDisabler SHARED ${PD_SOURCES})
else()
    add_library(ParticleDisabler STATIC ${PD_SOURCES})
endif()

target_include_directories(ParticleDisabler PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# TODO: link against GlossHook and the LeviLaunchroid SDK once vendored,
# e.g.:
# target_link_libraries(ParticleDisabler PRIVATE Gloss LeviLaunchroidSDK log)
target_link_libraries(ParticleDisabler PRIVATE log)

target_compile_options(ParticleDisabler PRIVATE
    -Wall -Wextra -fvisibility=hidden
)
