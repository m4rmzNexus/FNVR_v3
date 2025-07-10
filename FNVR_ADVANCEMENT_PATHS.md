
# FNVR Advancement Paths Analysis

This document analyzes two separate paths for advancing the FNVR project to a stable Proof of Concept (PoC) and beyond. The analysis assumes a focus on right-hand NVCS integration, with left-hand in the background. Paths are evaluated for feasibility, based on Fallout New Vegas engine constraints and modding best practices.

## Path 1: Using VorpX (Recommended for Simplicity)
VorpX handles VR rendering (stereo, head tracking), so the project focuses on custom hand/skeleton integration via NVSE. This leverages existing code (vorpxMode checks) and reduces complexity.

### Pros
- Lower development effort: No need for custom DX hooks.
- Faster PoC: VorpX provides stable rendering baseline.
- Compatibility: Builds on community-tested VorpX FNV support.

### Cons
- Dependency on external tool (VorpX license required).
- Limited control: VorpX bugs (e.g., HUD distortion) may persist.
- Potential conflicts: VorpX's own tracking might interfere with custom hand IK.

### Detailed Steps (Timeline: 2-4 Weeks)
1. **Week 1: Setup and Integration**
   - Install/test VorpX with FNV.
   - Update NVCSSkeleton.cpp to auto-detect VorpX (via mod scanning in Globals.cpp).
   - Sync project tracking with VorpX head data (disable custom HMD in vorpxMode).
   - Test: Basic right-hand movement in VorpX mode.

2. **Week 2: Right-Hand PoC**
   - Refine IK in NVCSSkeleton.cpp (add pole vectors, INI offsets).
   - Hook VorpX events if possible (via API or memory scanning).
   - Optimize updates to match VorpX frame rate.
   - Test: Stable weapon handling in VR.

3. **Week 3: Optimizations**
   - Cache all VorpX-related data to avoid per-frame checks.
   - Add config toggles for VorpX-specific fixes (e.g., offset adjustments).
   - Profile performance in VorpX.

4. **Week 4: Polish and Testing**
   - Add user feedback (HUD messages for connection).
   - Test with mod lists (e.g., VorpX + NVCS overhauls).
   - Release PoC demo on Nexus.

### Risks and Mitigations
- Risk: VorpX updates break integration (30% chance). Mitigation: Version-pin VorpX in docs.
- Timeline Slip: If conflicts arise, extend testing by 1 week.

## Path 2: Without VorpX (Custom DirectX Hook and Stereo Rendering)
Develop custom DX9 hook for stereo rendering and VR input. This is independent but highly complex, requiring deep engine knowledge (e.g., NiDX9Renderer hooks).

### Pros
- Full control: Customizable rendering (e.g., better HUD, no external deps).
- Independence: No VorpX license/costs.
- Potential for innovation: True native-like VR in FNV.

### Cons
- High complexity: DX hooking risky (crashes, anti-cheat issues).
- Longer timeline: Engine reverse-engineering needed.
- Feasibility risk: Gamebryo's renderer may not handle stereo well (z-fighting, performance hits).

### Detailed Steps (Timeline: 6-12 Weeks)
1. **Weeks 1-2: Research and Setup**
   - Study FNV's NiDX9Renderer (via JIP-LN sources, disassembly tools like IDA).
   - Implement basic DX9 hook DLL (using MinHook or Detours) for Present/EndScene.
   - Add stereo rendering (duplicate viewport, offset cameras).
   - Test: Non-VR hook stability in FNV.

2. **Weeks 3-4: VR Input Integration**
   - Hook input system (xinput.h) for VR controllers.
   - Merge with existing pipe data (fnvr_pose_pipe.py) for head/hand tracking.
   - Implement custom head tracking by overriding camera node.
   - Test: Basic stereo view without tracking.

3. **Weeks 5-6: Right-Hand PoC**
   - Extend hook to manipulate NVCS bones in render loop.
   - Add custom IK solver tied to render frame.
   - Optimize for 90 Hz (multi-thread rendering if possible).
   - Test: Right-hand movement in custom stereo mode.

4. **Weeks 7-8: Optimizations**
   - Fix rendering artifacts (e.g., shader patches for stereo).
   - Add config for render params (resolution, FOV).
   - Profile and reduce latency.

5. **Weeks 9-12: Polish and Testing**
   - Handle edge cases (save/load, menus).
   - Test with mods (ensure hook doesn't break ENBs).
   - Release alpha with warnings (experimental).

### Risks and Mitigations
- Risk: Engine instability/crashes (60% chance). Mitigation: Use safe hooks, fallback to non-VR mode.
- Timeline Slip: If DX issues arise, pivot to simpler mono rendering (+2-4 weeks).

## Recommendation
Path 1 (VorpX) is more mantıklı for quick PoC – lower risk, faster results. Path 2 is ambitious for long-term, but start with Path 1 to build momentum. If proceeding, focus on small milestones and community testing. 