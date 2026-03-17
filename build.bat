@echo off
setlocal

set BUILD_DIR=build
set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Debug

cmake -B %BUILD_DIR% -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
cmake --build %BUILD_DIR%

echo.
echo Build complete: %BUILD_DIR%\szip.exe
