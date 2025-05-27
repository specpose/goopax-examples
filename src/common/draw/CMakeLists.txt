option(GOOPAX_DEBUG "GOOPAX_DEBUG" OFF)


find_package(SDL3 PATHS "${CMAKE_BINARY_DIR}/ext/sdl3")
find_package(Eigen3 3.3 NO_MODULE PATHS "${CMAKE_BINARY_DIR}/ext/eigen")
find_package(OpenGL)

add_library(goopax_typedefs INTERFACE)
if (GOOPAX_DEBUG)
  target_compile_definitions(goopax_typedefs INTERFACE -DGOOPAX_DEBUG=1)
else()
  target_compile_definitions(goopax_typedefs INTERFACE -DGOOPAX_DEBUG=0)
endif()
target_include_directories(goopax_typedefs SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/..")



if (TARGET SDL3::SDL3 AND TARGET Eigen3::Eigen)

find_path(Vulkan_INCLUDE_DIRS "vulkan/vulkan.h" HINTS "${CMAKE_BINARY_DIR}/ext/build_sdl3-prefix/src/build_sdl3/src/video/khronos")

if (TARGET OpenGL::GL)
    option(GOOPAX_DRAW_WITH_OPENGL "with OpenGL support" 1)
else()
    option(GOOPAX_DRAW_WITH_OPENGL "with OpenGL support" 0)
endif()

if (APPLE)
    option(GOOPAX_DRAW_WITH_METAL "with Metal support" 1)
    option(GOOPAX_DRAW_WITH_VULKAN "with Vulkan support" 0)
else()
    option(GOOPAX_DRAW_WITH_METAL "with Metal support" 0)
    option(GOOPAX_DRAW_WITH_VULKAN "with Vulkan support" 1)
endif()

if (NOT Vulkan_INCLUDE_DIRS)
    set(GOOPAX_DRAW_WITH_VULKAN 0)
endif()

if  (GOOPAX_DEBUG)
    set(GOOPAX_DRAW_WITH_METAL 0)
    set(GOOPAX_DRAW_WITH_OPENGL 0)
    set(GOOPAX_DRAW_WITH_VULKAN 0)
endif()

message("OpenGL: ${GOOPAX_DRAW_WITH_OPENGL}")
message("Vulkan: ${GOOPAX_DRAW_WITH_VULKAN}")
message("Metal: ${GOOPAX_DRAW_WITH_METAL}")


set (FILES window_sdl.cpp window_sdl.h window_plain.h window_plain.cpp)
if (GOOPAX_DRAW_WITH_METAL)
    set (FILES ${FILES} window_metal.h window_metal.mm)
endif()
if (GOOPAX_DRAW_WITH_OPENGL)
    set(FILES ${FILES} window_gl.h window_gl.cpp)
endif()
if (GOOPAX_DRAW_WITH_VULKAN)
    set(FILES ${FILES} window_vulkan.h window_vulkan.cpp)
endif()

add_library(goopax_draw ${FILES})



if (GOOPAX_DRAW_WITH_OPENGL)
    target_compile_definitions(goopax_draw PUBLIC -DWITH_OPENGL=1)
    target_link_libraries(goopax_draw PRIVATE OpenGL::GL)
else()
    target_compile_definitions(goopax_draw PUBLIC -DWITH_OPENGL=0)
endif()

if (GOOPAX_DRAW_WITH_VULKAN)
    target_compile_definitions(goopax_draw PUBLIC -DWITH_VULKAN=1)
    target_include_directories(goopax_draw SYSTEM PUBLIC ${Vulkan_INCLUDE_DIRS})
else()
    target_compile_definitions(goopax_draw PUBLIC -DWITH_VULKAN=0)
endif()

if (GOOPAX_DRAW_WITH_METAL)
    target_compile_definitions(goopax_draw PUBLIC -DWITH_METAL=1)
else()
    target_compile_definitions(goopax_draw PUBLIC -DWITH_METAL=0)
endif()

target_link_libraries(goopax_draw PUBLIC SDL3::SDL3)
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
