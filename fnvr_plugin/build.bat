@echo off
echo Building FNVR Plugin with CMake (Win32 Release)...

REM Clean build directory
if exist build rmdir /s /q build
mkdir build
cd build

REM Configure CMake for Win32 Release
echo Configuring CMake...
cmake -G "Visual Studio 17 2022" -A Win32 -DCMAKE_BUILD_TYPE=Release ..

if errorlevel 1 (
    echo CMake configuration failed!
    cd ..
    pause
    exit /b 1
)

REM Build the project
echo Building...
cmake --build . --config Release

if errorlevel 1 (
    echo Build failed!
    cd ..
    pause
    exit /b 1
)

REM Copy DLL to parent directory
if exist Release\FNVR.dll (
    copy /Y Release\FNVR.dll ..
    echo Build successful! FNVR.dll created.
) else (
    echo Error: FNVR.dll not found!
    cd ..
    pause
    exit /b 1
)

cd ..

REM Copy to game directory if specified
if not "%1"=="" (
    echo.
    echo Copying to %1\Data\NVSE\Plugins\
    if not exist "%1\Data\NVSE\Plugins" mkdir "%1\Data\NVSE\Plugins"
    copy /Y FNVR.dll "%1\Data\NVSE\Plugins\"
    echo Copy complete!
) else (
    echo.
    echo To auto-install, run: build_cmake.bat "C:\Games\Fallout New Vegas"
)

echo Done!