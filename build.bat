@echo off
setlocal

REM =================================================================
REM FNVR NVSE Plugin Build Script
REM =================================================================
REM
REM This script builds the FNVR NVSE plugin.
REM
REM Prerequisites:
REM 1. Visual Studio (2019 or later) with C++ workload.
REM 2. CMake (https://cmake.org/download/) installed and in your PATH.
REM
REM =================================================================

REM --- Configuration ---
set PLUGIN_DIR=fnvr_plugin
set BUILD_DIR=%PLUGIN_DIR%/build

echo.
echo [FNVR Build Script]
echo.

REM --- Directory Check ---
if not exist "%PLUGIN_DIR%" (
    echo [ERROR] Plugin source directory '%PLUGIN_DIR%' not found.
    echo Please run this script from the root of the repository.
    exit /b 1
)

REM --- Build Directory Setup ---
if not exist "%BUILD_DIR%" (
    echo [INFO] Creating build directory: '%BUILD_DIR%'
    mkdir "%BUILD_DIR%"
)

REM --- CMake Configuration ---
echo [INFO] Configuring project with CMake...
cd %PLUGIN_DIR%
REM Specify Win32 architecture for the 32-bit Fallout New Vegas executable
cmake -S . -B build -A Win32
if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed.
    cd ..
    exit /b 1
)
cd ..

REM --- Build ---
echo [INFO] Building the project (Release mode)...
cmake --build %BUILD_DIR% --config Release
if %errorlevel% neq 0 (
    echo [ERROR] Build failed.
    exit /b 1
)

echo.
echo [SUCCESS] Build completed successfully!
echo.
echo  --> FNVR.dll can be found in: %BUILD_DIR%/Release/
echo.

endlocal 