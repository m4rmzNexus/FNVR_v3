@echo off
echo ====================================
echo  FNVR (Fallout New Vegas VR) Installer
echo  Version 1.0
echo ====================================
echo.

REM Check if running as admin
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Please run this installer as Administrator!
    echo Right-click and select "Run as administrator"
    pause
    exit /b 1
)

REM Set paths
set GAME_DIR=D:\SteamLibrary\steamapps\common\Fallout New Vegas
set NVSE_DIR=%GAME_DIR%\Data\NVSE\Plugins
set PLUGIN_FILE=fnvr_plugin\installer\FNVR.dll

REM Check if game exists
if not exist "%GAME_DIR%\FalloutNV.exe" (
    echo ERROR: Fallout New Vegas not found at:
    echo %GAME_DIR%
    echo.
    echo Please edit this script to set the correct game path.
    pause
    exit /b 1
)

REM Check if NVSE exists
if not exist "%NVSE_DIR%" (
    echo ERROR: NVSE not installed!
    echo Expected directory: %NVSE_DIR%
    echo.
    echo Please install NVSE first from: https://github.com/xNVSE/NVSE/releases
    pause
    exit /b 1
)

REM Check if plugin file exists
if not exist "%PLUGIN_FILE%" (
    echo ERROR: FNVR.dll not found!
    echo Expected at: %PLUGIN_FILE%
    echo.
    echo Please run build_complete.bat first.
    pause
    exit /b 1
)

REM Stop game if running
echo Checking if game is running...
tasklist /FI "IMAGENAME eq FalloutNV.exe" 2>NUL | find /I /N "FalloutNV.exe">NUL
if "%ERRORLEVEL%"=="0" (
    echo Game is running! Closing it...
    taskkill /F /IM FalloutNV.exe
    timeout /t 3 >nul
)

REM Backup old version if exists
if exist "%NVSE_DIR%\FNVR.dll" (
    echo Backing up existing FNVR.dll...
    move /Y "%NVSE_DIR%\FNVR.dll" "%NVSE_DIR%\FNVR.dll.backup"
)

REM Copy new version
echo Installing FNVR.dll...
copy /Y "%PLUGIN_FILE%" "%NVSE_DIR%\"
if %errorlevel% neq 0 (
    echo ERROR: Failed to copy FNVR.dll!
    pause
    exit /b 1
)

REM Create Python virtual environment if needed
if not exist "venv" (
    echo.
    echo Creating Python virtual environment...
    python -m venv venv
    if %errorlevel% neq 0 (
        echo WARNING: Failed to create virtual environment
        echo You'll need to install Python dependencies manually
    ) else (
        echo Activating virtual environment...
        call venv\Scripts\activate.bat
        
        echo Installing Python dependencies...
        pip install pyopenvr pywin32
    )
)

REM Create launch script
echo.
echo Creating launch script...
(
echo @echo off
echo cd /d "%~dp0"
echo echo Starting FNVR...
echo echo.
echo echo Step 1: Starting VR tracker...
echo if exist "venv\Scripts\activate.bat" call venv\Scripts\activate.bat
echo start "FNVR Tracker" python fnvr_pose_pipe.py
echo timeout /t 3 ^>nul
echo.
echo echo Step 2: Launching Fallout New Vegas with NVSE...
echo cd /d "%GAME_DIR%"
echo start "" nvse_loader.exe
echo.
echo echo.
echo echo FNVR is running! 
echo echo - Make sure VorpX is running
echo echo - The game should start with VR hand tracking
echo echo.
echo echo Close this window to stop the tracker.
echo pause
) > start_fnvr.bat

echo.
echo ====================================
echo  Installation Complete!
echo ====================================
echo.
echo FNVR has been installed successfully!
echo.
echo To use FNVR:
echo 1. Make sure VorpX is running
echo 2. Run start_fnvr.bat
echo 3. The game will launch with VR hand tracking
echo.
echo Check these log files for troubleshooting:
echo - %GAME_DIR%\FNVR_Complete.log
echo - %GAME_DIR%\nvse.log
echo.
pause