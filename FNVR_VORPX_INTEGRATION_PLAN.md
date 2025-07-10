
# FNVR VorpX Integration Plan

This document expands on Path 1 from FNVR_ADVANCEMENT_PATHS.md, chosen for its simplicity and leverage of VorpX for VR rendering. The plan assumes VorpX is always in use, focusing on refactoring the codebase to optimize for this scenario. It includes further analysis, a detailed roadmap, and specific refactoring steps to achieve a stable PoC (right-hand NVCS tracking). Left-hand support remains in the background.

## Further Analysis
### Why VorpX Path?
VorpX provides out-of-the-box stereo rendering and head tracking for FNV, reducing the need for custom DX hooks. This aligns with FNV modding realities: Gamebryo's renderer is outdated, making custom VR risky (e.g., crashes, low FPS). By assuming VorpX, we can focus on hand/skeleton integration via NVSE, making PoC faster (2-4 weeks vs. 6-12).

### Potential Issues and Mitigations
- **Interference with Custom Tracking:** VorpX's head tracking may conflict with custom HMD data. Mitigation: Disable custom HMD in code (set to zero, let VorpX handle).
- **Performance Overhead:** VorpX adds latency. Mitigation: Sync updates to VorpX frame rate, cache data.
- **Dependency Risks:** VorpX updates could break. Mitigation: Pin versions, add detection.
- **Mod Conflicts:** Other mods (e.g., ENBs) may clash with VorpX. Mitigation: Auto-detect and toggle modes.
- **Feasibility Boost:** With VorpX, PoC success rate rises to 85% – community has VorpX FNV guides.

### Assumptions for Refactoring
- VorpX is required and detected at runtime.
- Code paths for non-VorpX (e.g., custom HMD tracking) are simplified or #ifdef'd out.
- Focus: Optimize right-hand IK/weapon integration for VorpX's rendering.

## Detailed Implementation Plan (Timeline: 2-4 Weeks)
The plan is phased, with micro-steps. Each phase includes refactoring specifics (files, changes).

### Phase 1: Setup and Integration (Week 1)
**Goal:** Establish VorpX baseline and auto-detection.

1. **Install and Test VorpX (1 Day)**
   - Setup VorpX with FNV, configure for stereo/head tracking.
   - Verify basic gameplay in VR.

2. **Add VorpX Detection (1 Day)**
   - Refactor Globals.cpp: In InitGlobals, scan modList for VorpX ESP/DLL (set global g_vorpxMode = true if found).
   - Change: Make g_vorpxMode default true; add log warning if not detected.
   - Files: Globals.cpp (add detection logic, lines ~99-163).

3. **Sync Tracking with VorpX (2 Days)**
   - Refactor NVCSSkeleton.cpp: In Update, if g_vorpxMode, zero HMD globals and rely on VorpX (disable custom head mapping).
   - Add: Hook for VorpX events if API available (fallback: memory scan for VorpX DLL).
   - Files: NVCSSkeleton.cpp (UpdateVorpXMode: expand lines 299-325); PluginMain.cpp (add VorpX-specific thread sync).
   - Test: Right-hand moves correctly without overriding VorpX head.

### Phase 2: Right-Hand PoC Development (Week 2)
**Goal:** Stable right-hand tracking in VorpX.

1. **Refine Right-Hand Mapping (1 Day)**
   - Refactor NVCSSkeleton.cpp: Use INI offsets exclusively (remove hardcoded, assume VorpX scales).
   - Change: In MapControllerToHand, add VorpX-specific adjustments (e.g., scale factor from VorpX FOV).
   - Files: NVCSSkeleton.cpp (MapControllerToHand: lines 70-95).

2. **Improve IK for Right Arm (2 Days)**
   - Refactor CalculateArmIK: Add pole vectors, test with VorpX poses.
   - Change: Optimize for VorpX latency (e.g., predict next frame).
   - Files: NVCSSkeleton.cpp (CalculateArmIK: lines 119-149).
   - Test: Weapon grip/aim smooth in VorpX view.

3. **Basic Gesture Integration (1 Day)**
   - Add simple right-hand gestures (e.g., trigger press -> grip anim) using VRDataPacket inputs.
   - Files: Globals.cpp (UpdateGlobals: add input checks); use JohnnyGuitar for anim blending.
   - Test: Gesture triggers in VorpX without lag.

### Phase 3: Optimizations (Week 3)
**Goal:** Ensure performance in VorpX.

1. **Caching and Sync (2 Days)**
   - Refactor PluginMain.cpp: Cache VorpX-related data (e.g., head pos from memory if accessible).
   - Change: Sync update thread to VorpX refresh rate (query via hook).
   - Files: PluginMain.cpp (UpdateThreadFunc: lines 276-292).

2. **Config Tweaks for VorpX (1 Day)**
   - Expand LoadConfig: Add VorpX-specific params (e.g., latency_offset).
   - Files: PluginMain.cpp (LoadConfig: add sections).
   - Test: Adjust params without restart.

3. **Profiling (1 Day)**
   - Use NVSE tools to profile FPS/latency in VorpX.
   - Optimize hotspots (e.g., reduce bone updates).

### Phase 4: Polish and Testing (Week 4)
**Goal:** PoC Demo Ready.

1. **User Feedback (1 Day)**
   - Add HUD messages for VorpX status (e.g., "VorpX Connected, Right-Hand Active").
   - Files: Use jip_fn_ui.h in MessageHandler.

2. **Full Testing (2 Days)**
   - Test with mod lists (VorpX + essentials like NVCS, ENB).
   - Fix conflicts (e.g., auto-disable conflicting features).

3. **Refactoring Cleanup (1 Day)**
   - #ifdef non-VorpX code out (e.g., custom HMD paths).
   - Add comments/docs for VorpX assumptions.
   - Files: All (e.g., NVCSSkeleton.cpp, Globals.cpp).

4. **Release PoC (End of Week)**
   - Package demo (plugin + instructions for VorpX setup).
   - Upload to Nexus with beta tag.

## Refactoring Specifics
Assuming VorpX is always present, revise code to simplify:
- **Globals.cpp:** Set vorpxMode default true; remove non-VorpX HMD paths (conditional compile).
- **NVCSSkeleton.cpp:** Optimize Update for VorpX (skip head, focus right-hand); add VorpX-specific offsets.
- **PluginMain.cpp:** Add VorpX detection in Load; disable custom threads if VorpX handles rendering.
- **VRSystem.cpp:** Deprecate non-VorpX conversions (move to #ifdef).
- **General:** Search/replace hardcoded checks with globals; add VorpX error handling (e.g., if not detected, log and disable VR).

## Risks and Timeline Adjustments
- **Risks:** VorpX conflicts (40% chance) – mitigate with extensive testing.
- **Timeline:** If issues, extend Phase 2 by 1 week.
- **Resources Needed:** VorpX license, VR headset, FNV modding tools (GECK, NVSE).

This plan ensures a mantıklı, step-by-step progression to PoC while optimizing for VorpX. 

## Progress Tracking
- **Completed Phase 1.1 (Setup and Integration)**: VorpX detection added to Globals.cpp.
- **Completed Phase 1.2 (Sync Tracking)**: Update function in NVCSSkeleton.cpp refactored to zero HMD in VorpX mode; placeholder hook added (to be functionalized).
- **Completed Phase 1.3 (Thread Sync)**: VorpX-specific sync added to UpdateThreadFunc in PluginMain.cpp.
- **Completed Phase 2 (Right-Hand PoC Development)**: Refined mapping, improved IK, basic gestures added.
- **Completed Phase 3 (Optimizations)**: Caching, config tweaks, and profiling added.
- **In Progress**: Phase 4 (Polish and Testing). 