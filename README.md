# FNVR v3 - Fallout: New Vegas VR Mod

This repository contains the source code for FNVR v3, a mod aiming to bring enhanced, modern VR capabilities to Fallout: New Vegas.

The project consists of two main parts:
1.  **NVSE Plugin (`/fnvr_plugin`)**: A C++ plugin for the New Vegas Script Extender (NVSE) that runs inside the game. It receives VR tracking data and manipulates game objects, the camera, and the player skeleton.
2.  **Python Tracker Scripts**: A collection of Python scripts that use OpenVR to read data from a VR headset and controllers, and send it to the NVSE plugin via a named pipe.

## Building the Plugin (`FNVR.dll`)

A build script is provided to automate the compilation of the C++ NVSE plugin.

### Prerequisites
*   **Visual Studio 2019 (or newer)**: Make sure you have the "Desktop development with C++" workload installed. The free Community edition is sufficient.
*   **CMake**: Download from [cmake.org](https://cmake.org/download/) and ensure it is added to your system's PATH during installation.

### Build Steps
From the root directory of the project, simply run the `build.bat` script:

```shell
.\build.bat
```

This script will automatically:
1.  Create a `build` directory inside `fnvr_plugin`.
2.  Run CMake to generate the Visual Studio project files for a 32-bit build (required for Fallout: New Vegas).
3.  Compile the project in `Release` mode.

If the build is successful, the compiled `FNVR.dll` will be located in the `fnvr_plugin/build/Release/` directory.

## How to Use

1.  **Build the DLL** using the steps above.
2.  **Copy the DLL**: Take the newly compiled `FNVR.dll` and place it in your game's NVSE plugin directory: `.../Fallout New Vegas/Data/NVSE/Plugins/`.
3.  **Run the Tracker**: Execute the appropriate Python tracker script (e.g., `fnvr_pose_pipe.py`) to begin sending VR data.
4.  **Launch the Game**: Start Fallout: New Vegas using `nvse_loader.exe`. The plugin will automatically connect to the tracker script.

---
*This project is under active development.* 