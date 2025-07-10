#pragma once

#include "nvse/GameForms.h"
#include "nvse/GameAPI.h"   // For LookupFormByID, TESGlobal
#include "nvse/GameData.h"  // For DataHandler
#include "VRDataPacket.h"   // VRDataPacket tanımı

// Helper macro to simplify null checks. Can be used anywhere Globals.h is included.
// JIP-LN SDK: TESGlobal type check için typeID kullanıyoruz
#define SAFE_SET_VALUE(global, value) \
    do { \
        if (global && global->refID) { \
            global->data = value; \
        } \
    } while(0)

// Basit log makrosu - JIP-LN SDK'da _MESSAGE tanımlı değilse
#ifndef _MESSAGE
#define _MESSAGE(fmt, ...) ((void)0)  // Şimdilik boş, gerçek loglama eklenebilir
#endif



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