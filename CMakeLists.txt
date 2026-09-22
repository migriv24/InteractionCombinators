cmake_minimum_required(VERSION 3.20)
project(interaction_combinators CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# one version for every build (the APK script reads the same file)
# re-run CMake when VERSION changes, so a build never carries a stale version
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" IC_VERSION LIMIT_COUNT 1)
add_compile_definitions(IC_VERSION="${IC_VERSION}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Void Maiz (which itself locates Void Core via its VOIDCORE_ROOT).
set(VOIDMAIZ_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../VoidMaiz" CACHE PATH
    "Path to the Void Maiz repo")
add_subdirectory("${VOIDMAIZ_ROOT}" voidmaiz)

add_executable(interaction_combinators src/app.cpp src/main_desktop.cpp)
target_link_libraries(interaction_combinators PRIVATE
  voidmaiz_view    # canvas, inspector, widgets, faces (brings voidmaiz + imgui)
  voidmaiz_reduce  # the interaction-net executor (app-side, per the compute boundary)
)
if(NOT MSVC)
  target_compile_options(interaction_combinators PRIVATE -Wall -Wextra)
endif()

# Collaboration: only when Void Maiz built voidmaiz_net (it needs ../VoidPalabra).
# Without it the app is exactly the solo app it always was.
if(TARGET voidmaiz_net)
  target_link_libraries(interaction_combinators PRIVATE voidmaiz_net)
  target_compile_definitions(interaction_combinators PRIVATE IC_NET)

  # The duo bench: two windows, one process, a simulated link between them
  # (VoidMaiz okf/testing/collaborative-canvas-user-tests.md).
  add_executable(interaction_combinators_duo src/app.cpp src/main_duo.cpp)
  target_link_libraries(interaction_combinators_duo PRIVATE
    voidmaiz_view voidmaiz_reduce voidmaiz_net)
  target_compile_definitions(interaction_combinators_duo PRIVATE IC_NET)
  if(NOT MSVC)
    target_compile_options(interaction_combinators_duo PRIVATE -Wall -Wextra)
  endif()
else()
  message(STATUS "voidmaiz_net not available: Interaction Combinators builds solo only")
endif()
