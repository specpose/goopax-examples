option(GOOPAX_DEBUG "GOOPAX_DEBUG" OFF)

find_package(SDL3 PATHS "${CMAKE_BINARY_DIR}/ext/sdl")
find_package(SDL2 PATHS "${CMAKE_BINARY_DIR}/ext/sdl")
find_package(Eigen3 3.3 NO_MODULE PATHS "${CMAKE_BINARY_DIR}/ext/eigen")
find_package(OpenGL)

add_library(goopax_typedefs INTERFACE)
if (GOOPAX_DEBUG)
  target_compile_definitions(goopax_typedefs INTERFACE -DGOOPAX_DEBUG=1)
else()
  target_compile_definitions(goopax_typedefs INTERFACE -DGOOPAX_DEBUG=0)
endif()


if ((TARGET SDL3::SDL3 OR TARGET SDL2::SDL2) AND TARGET Eigen3::Eigen)

if (TARGET OpenGL::GL)
    option(GOOPAX_DRAW_WITH_OPENGL "with OpenGL support" 1)
else()
    option(GOOPAX_DRAW_WITH_OPENGL "with OpenGL support" 0)
endif()

if (APPLE)
    option(GOOPAX_DRAW_WITH_METAL "with Metal support" 1)
else()
    option(GOOPAX_DRAW_WITH_METAL "with Metal support" 0)
endif()

if  (GOOPAX_DEBUG)
set(GOOPAX_DRAW_WITH_METAL 0)
set(GOOPAX_DRAW_WITH_OPENGL 0)
endif()

if (TARGET SDL3::SDL3)
    option(GOOPAX_DRAW_USE_SDL2 "Backport to SDL2" 0)
else()
    option(GOOPAX_DRAW_USE_SDL2 "Backport to SDL2" 1)
endif()

message("OpenGL: ${GOOPAX_DRAW_WITH_OPENGL}")
message("Metal: ${GOOPAX_DRAW_WITH_METAL}")
message("SDL2: ${GOOPAX_DRAW_USE_SDL2}")

set (FILES window_sdl.cpp window_sdl.h window_plain.h window_plain.cpp)
if (GOOPAX_DRAW_WITH_METAL)
    set (FILES ${FILES} window_metal.h window_metal.mm)
endif()
if (GOOPAX_DRAW_WITH_OPENGL)
    set(FILES ${FILES} window_gl.h window_gl.cpp)
endif()

add_library(goopax_draw ${FILES})



if (GOOPAX_DRAW_WITH_OPENGL)
    target_compile_definitions(goopax_draw PUBLIC -DWITH_OPENGL=1)
    target_link_libraries(goopax_draw PRIVATE OpenGL::GL)
else()
    target_compile_definitions(goopax_draw PUBLIC -DWITH_OPENGL=0)
endif()

if (GOOPAX_DRAW_WITH_METAL)
    target_compile_definitions(goopax_draw PUBLIC -DWITH_METAL=1)
else()
    target_compile_definitions(goopax_draw PUBLIC -DWITH_METAL=0)
endif()

if (NOT GOOPAX_DRAW_USE_SDL2)
  target_compile_definitions(goopax_draw PUBLIC -DUSE_SDL2=0)
  target_link_libraries(goopax_draw PUBLIC SDL3::SDL3)
else()
  target_compile_definitions(goopax_draw PUBLIC -DUSE_SDL2=1)
  target_link_libraries(goopax_draw PUBLIC SDL2::SDL2)
endif()
if (NOT APPLE)
  if (NOT WIN32 AND NOT ANDROID)
    target_link_libraries(goopax_draw PUBLIC -lX11)
  endif()
  if (NOT ANDROID)
  target_link_libraries(goopax_draw PUBLIC OpenGL::GLU)
  endif()
endif()
target_link_libraries(goopax_draw PUBLIC goopax::goopax Eigen3::Eigen goopax_typedefs)

endif()
