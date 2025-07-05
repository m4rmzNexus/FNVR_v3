@echo off
echo Activating virtual environment and starting the v2 calibrated tracker...

REM Execute the python script using the venv interpreter
.\\venv\\Scripts\\python.exe v2_calibrated_tracker.py

echo.
echo Tracker script has been terminated. Press any key to exit.
pause >nul 