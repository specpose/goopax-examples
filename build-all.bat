@echo off
setlocal EnableDelayedExpansion

set CMAKE_FLAGS=%CMAKE_FLAGS% -DCMAKE_CONFIGURATION_TYPES=Debug;Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDebug

set CMAKE_BUILD_PARALLEL_LEVEL=%NUMBER_OF_PROCESSORS%

cmake %* %CMAKE_FLAGS% -B build src
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

cmake --build build --target build_external_libraries
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

cmake build
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

cmake --build build --config Release
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

endlocal
