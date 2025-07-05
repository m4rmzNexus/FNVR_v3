@echo off
echo Starting FNVR Raw Pose Tracker...
echo.
echo Make sure:
echo 1. Fallout New Vegas is running with NVSE
echo 2. FNVRGlobals.esp is active
echo 3. SteamVR is running
echo.
echo Press any key to start the tracker...
pause > nul

cd /d "%~dp0"
if exist venv\Scripts\activate.bat (
    echo Using virtual environment...
    call venv\Scripts\activate.bat
    python fnvr_pose_pipe.py
) else (
    echo No virtual environment found, using system Python...
    python fnvr_pose_pipe.py
)

echo.
echo Tracker stopped.
pause 