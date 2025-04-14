add_custom_target(build_external_libraries)
include(ExternalProject)

set(MY_OPENCV_ARGS "")
set(MY_BOOST_ARGS "")
set(MY_SDL_GIT_TAG release-3.2.4)

list(APPEND MY_CMAKE_ARGS -DBUILD_SHARED_LIBS=0)

if (WIN32)
  list(APPEND MY_OPENCV_ARGS "-DOpenCV_STATIC=ON")
  set(OpenCV_STATIC ON CACHE "BOOL" "")
elseif (ANDROID)
  list(APPEND MY_BOOST_ARGS "-DBOOST_EXCLUDE_LIBRARIES=process")
elseif (IOS)
  list(APPEND MY_CMAKE_ARGS
    -DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}
    -G Xcode
    -DCMAKE_XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET=${CMAKE_XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET})
  list(APPEND MY_OPENCV_ARGS
    -DWITH_PNG=0
    -DWITH_WEBP=0
    -DBUILD_WEBP=0
    -DWITH_JASPER=0
    -DBUILD_JASPER=0)
  list(APPEND MY_BOOST_ARGS "-DBOOST_EXCLUDE_LIBRARIES=process|context|coroutine|fiber|asio|log")
  set(MY_SDL_GIT_TAG 8f9a562969761ec626d298079c99341fd25b4644)
endif()


if (NOT ANDROID)
  ExternalProject_Add(
    build_sdl3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG ${MY_SDL_GIT_TAG}
    EXCLUDE_FROM_ALL 1
    CMAKE_ARGS "${MY_CMAKE_ARGS}"
    "-DSDL_SHARED=0"
    "-DSDL_STATIC=1"
    "-DSDL_ASSEMBLY=0"
    "-DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/sdl3"
  )
endif()
if (NOT TARGET SDL3::SDL3 AND NOT ANDROID)
  add_dependencies(build_external_libraries build_sdl3)
endif()

if (NOT ANDROID)
  ExternalProject_Add(
    build_opencv
    URL https://github.com/opencv/opencv/archive/refs/tags/4.10.0.zip
    URL_HASH SHA256=3810bca2b1d1c572912df0ac3888126341f3762dfd28e91068c805fb656d0e51
    EXCLUDE_FROM_ALL 1
    CMAKE_ARGS "${MY_CMAKE_ARGS}"
    "${MY_OPENCV_ARGS}"
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
    "-DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/opencv"
  )
endif()
if (NOT OpenCV_FOUND AND NOT ANDROID)
  add_dependencies(build_external_libraries build_opencv)
endif()

  ExternalProject_Add(
    build_boost
    GIT_REPOSITORY https://github.com/boostorg/boost.git
    GIT_TAG boost-1.86.0
    GIT_SUBMODULES_RECURSE 1
    EXCLUDE_FROM_ALL 1
    INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/boost"
    LIST_SEPARATOR |   # Workaround to prevent MY_BOOST_ARGS from being treated as a list.
    CMAKE_ARGS ${MY_CMAKE_ARGS} "${MY_BOOST_ARGS}"
    "-DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/boost"
  )
if (NOT TARGET Boost::system)
  add_dependencies(build_external_libraries build_boost)
endif()


  ExternalProject_Add(
    build_eigen
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG 3.4.0
    EXCLUDE_FROM_ALL 1
    CMAKE_ARGS "${MY_CMAKE_ARGS}"
    "-DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/eigen"
    #INSTALL_COMMAND ""
  )
if (NOT TARGET Eigen3::Eigen)
  add_dependencies(build_external_libraries build_eigen)
endif()

  ExternalProject_Add(
    build_glatter
    GIT_REPOSITORY https://github.com/imakris/glatter.git
    GIT_TAG 886f9765c6553ecb0934b5ec187db7d57ab0cfbc
    EXCLUDE_FROM_ALL 1
    CONFIGURE_COMMAND ""
    INSTALL_COMMAND ""
    BUILD_COMMAND ""
  )
if (NOT GLATTER_INCLUDE_DIR)
  add_dependencies(build_external_libraries build_glatter)
endif()
