# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

FNVR v3 is a mod that brings VR capabilities to Fallout: New Vegas. The project consists of:

1. **NVSE Plugin** (`/fnvr_plugin`) - C++ plugin that runs inside the game, receives VR tracking data via named pipe and manipulates game objects/camera/skeleton
2. **Python Tracker Scripts** - Use OpenVR to read VR headset/controller data and send it to the NVSE plugin

## Common Development Commands

### Building the NVSE Plugin

```bash
# From project root, build the plugin:
.\build.bat

# The compiled FNVR.dll will be in: fnvr_plugin/build/Release/
```

### Installing the Plugin

```bash
# Install to game (requires admin):
.\install_fnvr.bat

# Or manually copy:
copy fnvr_plugin\build\Release\FNVR.dll "D:\SteamLibrary\steamapps\common\Fallout New Vegas\Data\NVSE\Plugins\"
```

### Running the VR System

```bash
# Start both tracker and game:
.\start_fnvr.bat

# Or run components separately:
python fnvr_pose_pipe.py  # Start VR tracker
# Then launch game with nvse_loader.exe
```

### Alternative Build Methods

```bash
# Using Developer Command Prompt (x86):
cd fnvr_plugin\build
msbuild FNVRPlugin.sln /p:Configuration=Release /p:Platform=Win32

# Clean build:
msbuild FNVRPlugin.sln /t:Clean /p:Configuration=Release /p:Platform=Win32
```

### Python Dependencies

```bash
# Create virtual environment
python -m venv venv
venv\Scripts\activate.bat

# Install dependencies
pip install pyopenvr pywin32
```

## Architecture Overview

### Plugin Architecture (C++)

The NVSE plugin follows a specific loading pattern:

1. **Entry Points**: `NVSEPlugin_Query()` and `NVSEPlugin_Load()` are called by NVSE
2. **Named Pipe Communication**: Receives VR data via `\\.\pipe\FNVRTracker`
3. **Per-Frame Updates**: Hooks into game's main loop via NVSE messaging system
4. **Skeleton Manipulation**: Updates NiNode transforms for VR hand/head tracking

Key components:
- `PluginMain.cpp` - Entry point and NVSE integration
- `PipeClient.cpp/h` - Named pipe client for receiving VR data
- `VRDataPacket.h` - Struct defining VR data format (32 floats for positions/rotations/buttons)
- `VRSystem.cpp/h` - VR data processing and game integration
- `NVCSSkeleton.cpp/h` - Skeleton manipulation for hand tracking
- `FirstPersonBodyFix.cpp/h` - Handles first-person body visibility and alignment

### Python Tracker Architecture

The tracker (`fnvr_pose_pipe.py`) uses OpenVR to:
1. Initialize VR system and get device poses
2. Convert pose matrices to quaternions and positions
3. Send raw data via named pipe to the NVSE plugin
4. No game-specific transformations (handled by plugin)

### Data Flow

```
VR Headset/Controllers → OpenVR → Python Tracker → Named Pipe → NVSE Plugin → Game Engine
```

## Important Technical Notes

### SDK Compatibility (from FNVR_porting_notes.md)

- **Platform**: Must build for Win32 (x86), not x64
- **SDK Choice**: Uses JIP-LN-NVSE SDK (header-only, don't include source files)
- **Key Differences**:
  - `g_thePlayer` is `PlayerCharacter*` (not `PlayerCharacter**`)
  - Use `node1stPerson` instead of `firstPersonSkeleton`
  - Use `GetDead()` instead of `IsDead()`
  - NiAVObject flags offset is `0x30` (not `0x14`)

### Plugin Export Pattern

The plugin must export functions correctly for NVSE:
- Use `.def` file for exports (fnvr_plugin.def)
- Avoid mixing `extern "C"` with `__declspec(dllexport)`
- Follow NVSE example plugin patterns for function signatures

### Named Pipe Protocol

- Pipe name: `\\.\pipe\FNVRTracker`
- Data format: Binary struct with timestamps, quaternions, and positions
- Python sends 80-88 bytes: `struct.pack('<I4f3f4f3f3fd', ...)`
- C++ expects `VRDataPacket` struct (check size alignment)

### Known Issues and Limitations

1. **Missing Files**:
   - `VRDataPacketV2.h` referenced but not present
   - `Core/Logging.cpp` exists but other Core files missing
   - `ConvertV2ToFull()` function called but not defined

2. **Path Dependencies**:
   - Game path hardcoded in install scripts and CMake
   - SDK paths hardcoded in CMakeLists.txt

3. **Build Path Mismatch**:
   - Build outputs to `fnvr_plugin/build/Release/`
   - Install script looks for `fnvr_plugin/installer/FNVR.dll`

4. **Coordinate System Conversion**:
   - OpenVR uses different coordinate system than Gamebryo
   - Quaternion to Euler conversion may need adjustment
   - Position scaling and offsets configured via INI

## Configuration

### fnvr_config.ini Settings

```ini
[Position]
PositionScale = 50.0          # Meters to game units conversion
PositionOffsetX = 15.0        # Right/left offset
PositionOffsetY = -10.0       # Forward/back offset
PositionOffsetZ = 0.0         # Up/down offset

[Rotation]
RotationScalePitch = -120.0   # Pitch multiplier
RotationScaleYaw = 1.0        # Yaw multiplier
RotationScaleRoll = 120.0     # Roll multiplier
```

Note: These settings are referenced but not currently loaded by the plugin code.

## Debugging

- Check logs in game directory:
  - `FNVR_Complete.log` - Main plugin log
  - `nvse.log` - NVSE framework log
  - `fnvr_debug.txt` - Debug output (if enabled)

- Enable debug output by defining `DEBUG_OUTPUT` in plugin code
- Python tracker prints connection status and packet info to console

## Development Tips

1. **Build Issues**: Ensure using x86 Developer Command Prompt, not x64
2. **Testing**: Check logs in Fallout directory for errors
3. **Calibration**: Adjust values in `fnvr_config.ini` (note: not currently loaded by plugin)
4. **VorpX Integration**: Special INI files handle VorpX compatibility mode
5. **Memory Safety**: Plugin uses manual memory management - check for leaks
6. **Thread Safety**: Named pipe operations should use critical sections