#pragma once

#include "nvse/GameForms.h"
#include "nvse/GameAPI.h"   // For LookupFormByID, TESGlobal
#include "nvse/GameData.h"  // For DataHandler

// Helper macro to simplify null checks. Can be used anywhere Globals.h is included.
#define SAFE_SET_VALUE(global, value) if (global) { global->data = value; }

// New modern packet structure matching fnvr_pose_pipe.py
struct VRDataPacket {
    // Versioning
    int version;
    
    // HMD quaternion (w, x, y, z)
    float hmd_qw, hmd_qx, hmd_qy, hmd_qz;
    
    // HMD position (meters)
    float hmd_px, hmd_py, hmd_pz;
    
    // Right controller quaternion (w, x, y, z)
    float right_qw, right_qx, right_qy, right_qz;
    
    // Right controller position (meters)
    float right_px, right_py, right_pz;
    
    // Right controller position relative to HMD (meters, in HMD space)
    float right_rel_x, right_rel_y, right_rel_z;
    
    // Timestamp (seconds since epoch)
    double timestamp;
};

// Legacy packet for backwards compatibility (if needed)
struct LegacyVRDataPacket {
    int version;
    float hmd_x, hmd_y, hmd_z;
    float hmd_pitch, hmd_yaw, hmd_roll;
    float right_x, right_y, right_z;
    float right_pitch, right_yaw, right_roll;
    float left_x, left_y, left_z;
    float left_pitch, left_yaw, left_roll;
};

// This needs to be defined in PluginMain.cpp or somewhere central
#define PLUGIN_VERSION 2  // Updated for new packet format

namespace TESGlobals
{
    // HMD globals (matching ESP file names)
    extern TESGlobal* FNVRHMDX;
    extern TESGlobal* FNVRHMDY;
    extern TESGlobal* FNVRHMDZ;
    extern TESGlobal* FNVRHMDPitch;
    extern TESGlobal* FNVRHMDYaw;
    extern TESGlobal* FNVRHMDRoll;

    // Right controller globals (matching ESP file names)
    extern TESGlobal* FNVRRightX;
    extern TESGlobal* FNVRRightY;
    extern TESGlobal* FNVRRightZ;
    extern TESGlobal* FNVRRightPitch;
    extern TESGlobal* FNVRRightYaw;
    extern TESGlobal* FNVRRightRoll;

    // Left controller globals (matching ESP file names)
    extern TESGlobal* FNVRLeftX;
    extern TESGlobal* FNVRLeftY;
    extern TESGlobal* FNVRLeftZ;
    extern TESGlobal* FNVRLeftPitch;
    extern TESGlobal* FNVRLeftYaw;
    extern TESGlobal* FNVRLeftRoll;

    // Status Global
    extern TESGlobal* FNVRStatus; // 0=Disconnected, 1=Connected, 2=Ver Mismatch

    void UpdateGlobals(const VRDataPacket& packet);
    void InitGlobals();
    void ResetGlobals();
    
    // Utility functions for coordinate transformation and calibration
    void QuaternionToEuler(float qw, float qx, float qy, float qz, float& pitch, float& yaw, float& roll);
    void TransformOpenVRToGamebryo(float vr_x, float vr_y, float vr_z, float& game_x, float& game_y, float& game_z);
} 