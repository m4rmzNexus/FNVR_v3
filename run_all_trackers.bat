@echo off
REM FNVR Tracker Selection Menu
REM This batch file lets you choose which tracker to run

echo ========================================
echo FNVR Tracker Selection Menu
echo ========================================
echo.

REM Check if venv exists
if not exist "venv\Scripts\activate.bat" (
    echo ERROR: Virtual environment not found!
    echo Please run setup_venv.bat first.
    echo.
    pause
    exit /b 1
)

REM Activate virtual environment
call venv\Scripts\activate.bat

:menu
cls
echo ========================================
echo FNVR Tracker Selection Menu
echo ========================================
echo.
echo 1. Run Calibrated Tracker (FNVR_calibrated_tracker.py) - RECOMMENDED
echo 2. Run Simple Tracker (FNVR_simple_tracker.py)
echo 3. Run Legacy Tracker (legacy_FNVR_Tracker.py)
echo 4. Exit
echo.
set /p choice="Select tracker to run (1-4): "

if "%choice%"=="1" goto calibrated
if "%choice%"=="2" goto simple
if "%choice%"=="3" goto legacy
if "%choice%"=="4" goto exit
echo Invalid choice. Please try again.
pause
goto menu

:calibrated
if not exist "FNVR_calibrated_tracker.py" (
    echo ERROR: FNVR_calibrated_tracker.py not found!
    pause
    goto menu
)
echo.
echo Starting Calibrated Tracker...
echo ========================================
echo Controls:
echo   C - Save current position as calibration center
echo   R - Reset calibration
echo   S - Save calibration settings
echo   Tab - Show debug information
echo ========================================
echo.
python FNVR_calibrated_tracker.py
echo.
echo Tracker stopped.
pause
goto menu

:simple
if not exist "FNVR_simple_tracker.py" (
    echo ERROR: FNVR_simple_tracker.py not found!
    pause
    goto menu
)
echo.
echo Starting Simple Tracker...
echo.
python FNVR_simple_tracker.py
echo.
echo Tracker stopped.
pause
goto menu

:legacy
if not exist "legacy_FNVR_Tracker.py" (
    echo ERROR: legacy_FNVR_Tracker.py not found!
    pause
    goto menu
)
echo.
echo Starting Legacy Tracker...
echo.
python legacy_FNVR_Tracker.py
echo.
echo Tracker stopped.
pause
goto menu

:exit
call deactivate
echo Goodbye!
exit /b 0