
# FNVR Improvement Plan

This document outlines the prioritized improvements for the FNVR project, based on the detailed code analysis. The focus is on creating a Proof of Concept (PoC) for right-hand tracking integration with NVCS (New Vegas Character Skeleton), keeping left-hand support in the background as per instructions. Improvements are categorized and phased for systematic implementation.

## Guiding Principles
- **PoC Focus**: Ensure right-hand tracking works stably on NVCS first.
- **Modder Perspective**: Emphasize compatibility, performance, and modularity for New Vegas modding community.
- **Integration with Existing Plans**: Build on FNVR_UNIFIED_PLAN.md (unified VRDataPacket) and FNVR_DETAILED_ANALYSIS_REPORT.md (roadmap to FRIK level).
- **Prioritization**: Start with critical fixes for functionality, then optimizations, and finally advanced features.

## Phase 1: Critical Fixes (1-2 Days) - Get PoC Working
These ensure the basic right-hand tracking PoC runs without crashes.

1. **Resolve Data Packet Incompatibilities**
   - Implement the unified VRDataPacket structure from FNVR_UNIFIED_PLAN.md in VRDataPacket.h.
   - Update PipeClient.cpp to handle flags-based reading, mirroring left controller if absent (but focus on right).
   - Add conversion functions if needed for legacy code.

2. **Fix Configuration Loading**
   - Load all INI values at plugin load (NVSEPlugin_Load) and cache them globally.
   - Remove per-update INI reads in Globals.cpp and NVCSSkeleton.cpp.

3. **Ensure Right-Hand NVCS Integration**
   - In NVCSSkeleton.cpp, refine right-hand mapping with proper offsets from INI.
   - Add basic 2-bone IK for right arm, testing on NVCS bones (e.g., Bip01 R Hand, Weapon).

4. **Add Null Checks and Safe Access**
   - Add null pointer checks in all NiNode accesses (e.g., PluginMain.cpp's FindBone).

## Phase 2: Performance Optimizations (3-5 Days) - Stabilize PoC
Improve efficiency to avoid stutters in VR.

1. **Optimize Logging**
   - Make logging buffered: Open file once at load, flush periodically, close at unload.

2. **Cache Bone Lookups**
   - Implement a persistent bone cache in PluginMain.cpp, rebuild only on skeleton changes.

3. **Adjust Update Rates**
   - Sync Python update rate (fnvr_pose_pipe.py) to 60 Hz for PoC.
   - Use multi-threading for VR data processing as per FNVR_DETAILED_ANALYSIS_REPORT.md.

## Phase 3: Compatibility and Mod Integration (1 Week)
Ensure works with other mods.

1. **VorpX and Mod Conflict Detection**
   - Add mod list scanning at init to detect conflicts (e.g., VorpX mode auto-toggle).

2. **NVSE/JIP-LN Enhancements**
   - Use JohnnyGuitar NVSE for right-hand animations (e.g., grip blending).

3. **Broadcast VR Data**
   - Implement messaging system to share right-hand data with other plugins.

## Phase 4: User Experience and Code Quality (1 Week)
Make it modder-friendly.

1. **Dynamic Configuration**
   - Add MCM support for right-hand offsets/scales.

2. **Improved Debugging**
   - Add debug mode with bone position logging.

3. **Code Refactoring**
   - Add comments, modularize functions (e.g., separate right-hand IK solver).

## Phase 5: Advanced Features (2-3 Weeks) - Towards FRIK Level
Build on PoC for full features.

1. **Enhanced Right-Hand IK**
   - Implement FABRIK solver for right arm (pole vectors, elbow targeting).

2. **Gesture Recognition**
   - Add basic right-hand poses (grip, point) using controller inputs.

3. **Full Skeleton Expansion**
   - Expand to 94 bones as per FRIKSkeleton.h, starting with right arm chain.

## Risks and Mitigations
- **Risk**: Engine limitations cause crashes. **Mitigation**: Use JIP-LN safe functions.
- **Risk**: Performance issues in VR. **Mitigation**: Profile with tools like NVSE debugger.
- **Testing**: Test on vanilla New Vegas + essential mods (NVSE, JIP-LN).

This plan will be implemented step-by-step, starting with Phase 1. 

## Progress Tracking
- **Completed Phase 1.1 (Data Packet Incompatibilities)**: Unified VRDataPacket implemented in VRDataPacket.h; PipeClient.cpp updated for flags-based reading.
- **Completed Phase 1.2 (Configuration Loading)**: INI loading added to PluginMain.cpp; per-update reads removed from Globals.cpp and NVCSSkeleton.cpp.
- **In Progress**: Phase 1.3 (Right-Hand NVCS Integration). 