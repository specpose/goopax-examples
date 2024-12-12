option(GOOPAX_DEBUG "GOOPAX_DEBUG" OFF)


find_package(SDL3)
find_package(OpenGL)

if (TARGET SDL3::SDL3)

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

message("OpenGL: ${GOOPAX_DRAW_WITH_OPENGL}")
message("Metal: ${GOOPAX_DRAW_WITH_METAL}")


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

#target_include_directories(goopax_draw SYSTEM PRIVATE "ext/glatter/include")
target_link_libraries(goopax_draw PUBLIC SDL3::SDL3)
if (NOT APPLE)
  if (NOT WIN32)
    target_link_libraries(goopax_draw PUBLIC -lX11)
  endif()
  target_link_libraries(goopax_draw PUBLIC OpenGL::GLU)
endif()
target_link_libraries(goopax_draw PUBLIC goopax::goopax Eigen3::Eigen)

if (GOOPAX_DEBUG)
  target_compile_definitions(goopax_draw PUBLIC -DGOOPAX_DEBUG=1)
else()
  target_compile_definitions(goopax_draw PUBLIC -DGOOPAX_DEBUG=0)
endif()

endif()
