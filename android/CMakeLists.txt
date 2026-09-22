# Android build of Interaction Combinators: one shared library loaded by
# NativeActivity. Configure with the NDK toolchain file (see build_apk.ps1).
# Void Maiz's own CMake handles the substrate: on ANDROID it compiles Void
# Core from source (static), swaps GLFW for the android imgui backend, and
# skips desktop examples/tests.
cmake_minimum_required(VERSION 3.22)
project(interaction_combinators_android CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# one version for every build (the APK script reads the same file)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../VERSION" IC_VERSION LIMIT_COUNT 1)
add_compile_definitions(IC_VERSION="${IC_VERSION}")

set(VOIDMAIZ_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../VoidMaiz" CACHE PATH
    "Path to the Void Maiz repo")
add_subdirectory("${VOIDMAIZ_ROOT}" voidmaiz)

add_library(interaction_combinators SHARED
  ../src/app.cpp
  src/main_android.cpp
  "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c"
)
target_include_directories(interaction_combinators PRIVATE
  ../src
  "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue")
target_link_libraries(interaction_combinators PRIVATE
  voidmaiz_view    # canvas, inspector, widgets (brings voidmaiz + imgui/GLES3)
  voidmaiz_reduce  # the interaction-net executor
  android log EGL GLESv3)
# collaboration: Void Palabra's sync + Void Maiz's LAN layer (when ../VoidPalabra exists)
if(TARGET voidmaiz_net)
  target_link_libraries(interaction_combinators PRIVATE voidmaiz_net)
  target_compile_definitions(interaction_combinators PRIVATE IC_NET)
endif()
# the glue's entry point is only referenced by the framework — keep it
target_link_options(interaction_combinators PRIVATE -u ANativeActivity_onCreate)
if(NOT MSVC)
  target_compile_options(interaction_combinators PRIVATE -Wall -Wextra)
endif()
