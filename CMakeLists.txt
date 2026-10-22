cmake_minimum_required(VERSION 3.24)
if(CMAKE_HOST_APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET AND NOT DEFINED ENV{MACOSX_DEPLOYMENT_TARGET})
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum supported macOS release")
endif()
project(OpenA2 VERSION 0.4.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
  # The release ZIP should not require a separately installed MSVC runtime.
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(A2_BUILD_GAME "Build the SDL desktop application" ON)
option(A2_USE_SYSTEM_SDL2 "Look for an installed SDL2 before downloading" ON)
option(A2_FETCH_SDL2 "Download pinned SDL2 if no installed package is selected" ON)
option(A2_MACOS_BUNDLE "Package the macOS executable in an .app bundle" OFF)
option(A2_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
set(A2_TEST_DATA "" CACHE PATH "Optional original CD directory/image for integration tests")

add_library(a2smacker STATIC third_party/libsmacker/smacker.c)
target_include_directories(a2smacker PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libsmacker")
set_target_properties(a2smacker PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
# Upstream uses plain char for negative status codes and signed bit-reader
# results; retain those semantics on ARM toolchains whose default char is unsigned.
target_compile_options(a2smacker PRIVATE "$<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-fsigned-char>")
add_library(a2core STATIC src/assets.cpp src/simulation.cpp src/renderer.cpp src/scene.cpp src/movie.cpp src/guardrails.cpp src/hud.cpp src/texture.cpp src/graphics.cpp src/render_workers.cpp src/profiles.cpp src/menu_art.cpp)
target_include_directories(a2core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(a2core PUBLIC cxx_std_20)
find_package(Threads REQUIRED)
target_link_libraries(a2core PRIVATE a2smacker Threads::Threads)
if(MSVC)
  target_compile_options(a2core PRIVATE /W4 /permissive-)
else()
  target_compile_options(a2core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(A2_SANITIZERS)
  if(MSVC)
    message(FATAL_ERROR "A2_SANITIZERS currently requires Clang or GCC")
  endif()
  target_compile_options(a2core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(a2core PUBLIC -fsanitize=address,undefined)
  target_compile_options(a2smacker PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
endif()

if(A2_BUILD_GAME)
  if(A2_USE_SYSTEM_SDL2)
    find_package(SDL2 2.0.18 CONFIG QUIET)
  endif()
  if(NOT TARGET SDL2::SDL2 AND NOT TARGET SDL2::SDL2-static)
    if(NOT A2_FETCH_SDL2)
      message(FATAL_ERROR "SDL2 was not found. Set SDL2_DIR/CMAKE_PREFIX_PATH or enable A2_FETCH_SDL2.")
    endif()
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL2
      URL https://www.libsdl.org/release/SDL2-2.32.10.tar.gz
      URL_HASH SHA256=5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165
      DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    )
    FetchContent_MakeAvailable(SDL2)
  endif()

  # Prefer a static dependency so the packaged application does not depend on a
  # build machine's Homebrew/vcpkg installation at runtime.
  if(TARGET SDL2::SDL2-static)
    set(A2_SDL_TARGET SDL2::SDL2-static)
    set(A2_SDL_STATIC TRUE)
  else()
    set(A2_SDL_TARGET SDL2::SDL2)
    set(A2_SDL_STATIC FALSE)
  endif()
  add_executable(opena2 src/main.cpp src/audio.cpp)
  target_link_libraries(opena2 PRIVATE a2core ${A2_SDL_TARGET})
  target_compile_definitions(opena2 PRIVATE SDL_MAIN_HANDLED)
  if(APPLE AND A2_MACOS_BUNDLE)
    set_target_properties(opena2 PROPERTIES
      MACOSX_BUNDLE TRUE
      MACOSX_BUNDLE_BUNDLE_NAME "OpenA2"
      MACOSX_BUNDLE_GUI_IDENTIFIER "org.opena2.game"
      MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
      MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
  endif()
  install(TARGETS opena2 RUNTIME DESTINATION . BUNDLE DESTINATION .)
  if(NOT A2_SDL_STATIC)
    if(WIN32)
      install(FILES $<TARGET_FILE:${A2_SDL_TARGET}> DESTINATION .)
    else()
      message(WARNING "Shared SDL2 selected. For a self-contained release use -DA2_USE_SYSTEM_SDL2=OFF, which builds pinned static SDL2.")
    endif()
  endif()
endif()

find_package(Python3 COMPONENTS Interpreter QUIET)
include(CTest)
if(BUILD_TESTING)
  foreach(A2_TEST_NAME IN ITEMS assets simulation renderer movie legacy_geometry guardrails hud graphics profiles menu_art race_details police race_camera finish)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/${A2_TEST_NAME}_test.cpp")
      add_executable(${A2_TEST_NAME}_test tests/${A2_TEST_NAME}_test.cpp)
      target_link_libraries(${A2_TEST_NAME}_test PRIVATE a2core)
      if((A2_TEST_NAME STREQUAL "assets" OR A2_TEST_NAME STREQUAL "movie" OR A2_TEST_NAME STREQUAL "hud" OR A2_TEST_NAME STREQUAL "graphics" OR A2_TEST_NAME STREQUAL "menu_art" OR A2_TEST_NAME STREQUAL "race_details") AND A2_TEST_DATA)
        add_test(NAME ${A2_TEST_NAME} COMMAND ${A2_TEST_NAME}_test "${A2_TEST_DATA}")
      else()
        add_test(NAME ${A2_TEST_NAME} COMMAND ${A2_TEST_NAME}_test)
      endif()
    endif()
  endforeach()
  if(A2_BUILD_GAME AND Python3_Interpreter_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_data.py")
    add_test(NAME generate_test_data COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_data.py" "${CMAKE_CURRENT_BINARY_DIR}/synthetic-data")
    set_tests_properties(generate_test_data PROPERTIES FIXTURES_SETUP a2_generated_data)
    add_test(NAME headless_game COMMAND opena2 --data "${CMAKE_CURRENT_BINARY_DIR}/synthetic-data"
      --headless --demo --frames 480 --screenshot "${CMAKE_CURRENT_BINARY_DIR}/synthetic-smoke.bmp")
    add_test(NAME headless_enhanced_game COMMAND opena2 --data "${CMAKE_CURRENT_BINARY_DIR}/synthetic-data"
      --headless --demo --enhanced --frames 420 --screenshot "${CMAKE_CURRENT_BINARY_DIR}/synthetic-enhanced.bmp")
    set_tests_properties(headless_enhanced_game PROPERTIES FIXTURES_REQUIRED a2_generated_data TIMEOUT 60)
    set_tests_properties(headless_game PROPERTIES FIXTURES_REQUIRED a2_generated_data TIMEOUT 30)
  endif()
endif()

install(FILES README.md LICENSE DESTINATION .)
install(FILES docs/BUILD.md docs/THIRD_PARTY.md docs/VERIFICATION.md docs/GRAPHICS.md docs/MENUS.md DESTINATION docs)
install(FILES docs/RECONSTRUCTION.md DESTINATION docs OPTIONAL)
install(FILES third_party/libsmacker/COPYING third_party/libsmacker/UPSTREAM.md DESTINATION docs/libsmacker)
install(FILES third_party/liberation/LICENSE third_party/liberation/UPSTREAM.md DESTINATION docs/liberation)
install(FILES third_party/stb/LICENSE third_party/stb/UPSTREAM.md DESTINATION docs/stb)
# LGPL-2.1 section 6(a): ship the application's complete corresponding source
# and build files with static-linked native packages, so users can modify and
# relink libsmacker. The source allowlist never includes original game data.
install(CODE "
  if(NOT EXISTS \"${Python3_EXECUTABLE}\")
    message(FATAL_ERROR \"Python 3 is required to package the LGPL relinking source archive.\")
  endif()
  execute_process(COMMAND \"${Python3_EXECUTABLE}\" \"${CMAKE_CURRENT_SOURCE_DIR}/tools/package_source.py\"
    --output \"${CMAKE_CURRENT_BINARY_DIR}/source/OpenA2-${PROJECT_VERSION}-source.zip\"
    RESULT_VARIABLE a2_source_result)
  if(NOT a2_source_result EQUAL 0)
    message(FATAL_ERROR \"Could not create corresponding source archive\")
  endif()
")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/source/OpenA2-${PROJECT_VERSION}-source.zip" DESTINATION docs/source)
set(CPACK_PACKAGE_NAME "OpenA2")
set(CPACK_PACKAGE_VENDOR "OpenA2 contributors")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "OpenA2-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/packages")
set(CPACK_GENERATOR ZIP)
include(CPack)
