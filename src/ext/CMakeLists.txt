add_custom_target(build_external_libraries)
include(ExternalProject)


if (NOT TARGET SDL3::SDL3)
  ExternalProject_Add(
    build_sdl3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG main
    EXCLUDE_FROM_ALL 1
    #INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/sdl3"
    CMAKE_ARGS "${MY_CMAKE_ARGS}"
    "-DBUILD_SHARED_LIBS=0"
    "-DSDL_SHARED=0"
    "-DSDL_STATIC=1"
    "-DSDL_ASSEMBLY=0"
    "--install-prefix ${CMAKE_CURRENT_BINARY_DIR}/sdl3"
  )
  ExternalProject_Get_Property(build_sdl3 INSTALL_DIR)
  message("INSTALL_DIR=${INSTALL_DIR}")
  add_dependencies(build_external_libraries build_sdl3)
endif()

if (NOT TARGET opencv)
  ExternalProject_Add(
    build_opencv
    #DEPENDS Eigen3::Eigen
    GIT_REPOSITORY https://github.com/opencv/opencv.git
    GIT_TAG 4.10.0
    GIT_SUBMODULES_RECURSE 1
    EXCLUDE_FROM_ALL 1
    #INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/opencv"
    CMAKE_ARGS "${MY_CMAKE_ARGS}"
    "-DBUILD_SHARED_LIBS=0"
    "-DBUILD_ITT=0"
    "-DBUILD_PERF_TESTS=0"
    "-DBUILD_TESTS=0"
    "-DBUILD_opencv_apps=0"
    "-DWITH_PROTOBUF=0"
    "-DWITH_QUIRC=1"
    "-DWITH_LAPACK=0"
    "-DWITH_CUDA=0"
    "-DBUILD_opencv_dnn=0"
    "-DWITH_IPP=0"
    "-DWITH_OPENEXR=0"
    "-DWITH_JPEG=0"
    "-DWITH_OPENJPEG=0"
    "-DWITH_TIFF=0"
    "-DWITH_FFMPEG=0"
    "--install-prefix ${CMAKE_CURRENT_BINARY_DIR}/opencv"
  )
  add_dependencies(build_external_libraries build_opencv)
endif()



if (NOT TARGET Boost::system)
  ExternalProject_Add(
    build_boost
    GIT_REPOSITORY https://github.com/boostorg/boost.git
    GIT_TAG boost-1.86.0
    GIT_SUBMODULES_RECURSE 1
    EXCLUDE_FROM_ALL 1
    INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/boost"
    CMAKE_ARGS "-DBUILD_SHARED_LIBS=0"
    "--install-prefix ${CMAKE_CURRENT_BINARY_DIR}/boost"
  )
  add_dependencies(build_external_libraries build_boost)
endif()


if (NOT TARGET Eigen3::Eigen)
  ExternalProject_Add(
    build_eigen
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG 3.4.0
    EXCLUDE_FROM_ALL 1
    #INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/eigen"
    CMAKE_ARGS "${MY_CMAKE_ARGS}"
    "-DBUILD_SHARED_LIBS=0"
    "--install-prefix ${CMAKE_CURRENT_BINARY_DIR}/eigen"
    INSTALL_COMMAND ""
  )
  add_dependencies(build_external_libraries build_eigen)
endif()
