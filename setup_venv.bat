@echo off
REM FNVR Virtual Environment Setup
REM This batch file creates and sets up the virtual environment

echo ========================================
echo FNVR Virtual Environment Setup
echo ========================================
echo.

REM Check if Python is installed
python --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python is not installed or not in PATH!
    echo Please install Python 3.7 or higher.
    pause
    exit /b 1
)

echo Python found:
python --version
echo.

REM Check if venv already exists
if exist "venv" (
    echo Virtual environment already exists.
    set /p overwrite="Do you want to recreate it? (y/n): "
    if /i not "%overwrite%"=="y" (
        echo Setup cancelled.
        pause
        exit /b 0
    )
    echo Removing existing virtual environment...
    rmdir /s /q venv
    echo.
)

REM Create virtual environment
echo Creating virtual environment...
python -m venv venv

if errorlevel 1 (
    echo ERROR: Failed to create virtual environment!
    pause
    exit /b 1
)

echo Virtual environment created successfully.
echo.

REM Activate virtual environment
echo Activating virtual environment...
call venv\Scripts\activate.bat

REM Upgrade pip
echo Upgrading pip...
python -m pip install --upgrade pip

REM Install required packages
echo.
echo Installing required packages...
pip install pyopenvr keyboard numpy

echo.
echo ========================================
echo Setup completed successfully!
echo ========================================
echo.
echo To run the tracker, use: run_calibrated_tracker.bat
echo.

REM Deactivate virtual environment
call deactivate

pause