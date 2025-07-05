@echo off
REM FNVR Calibrated Tracker Launcher
REM This batch file activates the virtual environment and runs the calibrated tracker

echo ========================================
echo FNVR Calibrated Tracker Launcher
echo ========================================
echo.

REM Check if venv exists
if not exist "venv\Scripts\activate.bat" (
    echo ERROR: Virtual environment not found!
    echo Please create a virtual environment first with: python -m venv venv
    echo.
    pause
    exit /b 1
)

REM Activate virtual environment
echo Activating virtual environment...
call venv\Scripts\activate.bat

REM Check if activation was successful
if errorlevel 1 (
    echo ERROR: Failed to activate virtual environment!
    pause
    exit /b 1
)

echo Virtual environment activated.
echo.

REM Check if required packages are installed
echo Checking for required packages...
python -c "import openvr" 2>nul
if errorlevel 1 (
    echo WARNING: openvr package not found!
    echo Installing required packages...
    pip install pyopenvr keyboard numpy
    echo.
)

REM Check if calibrated tracker exists
if not exist "FNVR_calibrated_tracker.py" (
    echo ERROR: FNVR_calibrated_tracker.py not found!
    echo Please ensure the tracker script is in the current directory.
    pause
    exit /b 1
)

REM Run the calibrated tracker
echo Starting FNVR Calibrated Tracker...
echo.
echo ========================================
echo Controls:
echo   C - Save current position as calibration center
echo   R - Reset calibration
echo   S - Save calibration settings to file
echo   Tab - Show debug information
echo   Ctrl+C - Exit tracker
echo ========================================
echo.

REM Execute the python script using the venv interpreter
.\\venv\\Scripts\\python.exe FNVR_calibrated_tracker.py

REM Check if tracker exited with error
if errorlevel 1 (
    echo.
    echo ERROR: Tracker exited with an error!
    echo Check the error message above.
    pause
)

REM Deactivate virtual environment
call deactivate

echo.
echo Tracker stopped.
pause