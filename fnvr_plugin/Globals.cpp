#include "Globals.h"
#include "VRSystem.h"
#include "NVCSSkeleton.h"
#include "nvse/PluginAPI.h" // For _MESSAGE
#include "nvse/GameAPI.h" // For Console_Print
#include "nvse/GameData.h"
#include "nvse/ScriptUtils.h"
#include <vector>
#include <cmath>

namespace TESGlobals
{
    // Define the extern variables declared in the header
    TESGlobal* FNVRHMDX = nullptr;
    TESGlobal* FNVRHMDY = nullptr;
    TESGlobal* FNVRHMDZ = nullptr;
    TESGlobal* FNVRHMDPitch = nullptr;
    TESGlobal* FNVRHMDYaw = nullptr;
    TESGlobal* FNVRHMDRoll = nullptr;

    TESGlobal* FNVRRightX = nullptr;
    TESGlobal* FNVRRightY = nullptr;
    TESGlobal* FNVRRightZ = nullptr;
    TESGlobal* FNVRRightPitch = nullptr;
    TESGlobal* FNVRRightYaw = nullptr;
    TESGlobal* FNVRRightRoll = nullptr;

    TESGlobal* FNVRLeftX = nullptr;
    TESGlobal* FNVRLeftY = nullptr;
    TESGlobal* FNVRLeftZ = nullptr;
    TESGlobal* FNVRLeftPitch = nullptr;
    TESGlobal* FNVRLeftYaw = nullptr;
    TESGlobal* FNVRLeftRoll = nullptr;
    
    TESGlobal* FNVRStatus = nullptr;

    // A helper to keep track of all globals for easier iteration
    std::vector<TESGlobal**> allGlobals;
    
    // Constants for coordinate transformation and scaling
    const float POSITION_SCALE = 50.0f;  // meters to game units
    const float POSITION_OFFSET_X = 15.0f;
    const float POSITION_OFFSET_Y = -10.0f;
    const float POSITION_OFFSET_Z = 0.0f;
    const float ROTATION_SCALE_PITCH = -120.0f;
    const float ROTATION_SCALE_YAW = 1.0f;  // Direct mapping for VorpX compatibility
    const float ROTATION_SCALE_ROLL = 120.0f;
    const float ROTATION_OFFSET_PITCH = 10.0f;
    const float ROTATION_OFFSET_YAW = 0.0f;
    const float ROTATION_OFFSET_ROLL = -75.0f;

    // Utility function: Convert quaternion to Euler angles (in degrees)
    void QuaternionToEuler(float qw, float qx, float qy, float qz, float& pitch, float& yaw, float& roll)
    {
        // Roll (x-axis rotation)
        float sinr_cosp = 2.0f * (qw * qx + qy * qz);
        float cosr_cosp = 1.0f - 2.0f * (qx * qx + qy * qy);
        roll = std::atan2(sinr_cosp, cosr_cosp) * 180.0f / 3.14159265359f;

        // Pitch (y-axis rotation)
        float sinp = 2.0f * (qw * qy - qz * qx);
        if (std::abs(sinp) >= 1)
            pitch = std::copysign(90.0f, sinp); // use 90 degrees if out of range
        else
            pitch = std::asin(sinp) * 180.0f / 3.14159265359f;

        // Yaw (z-axis rotation)
        float siny_cosp = 2.0f * (qw * qz + qx * qy);
        float cosy_cosp = 1.0f - 2.0f * (qy * qy + qz * qz);
        yaw = std::atan2(siny_cosp, cosy_cosp) * 180.0f / 3.14159265359f;
    }

    // Utility function: Transform OpenVR coordinates to Gamebryo coordinates
    void TransformOpenVRToGamebryo(float vr_x, float vr_y, float vr_z, float& game_x, float& game_y, float& game_z)
    {
        // OpenVR: +Y up, +X right, -Z forward
        // Gamebryo: +Z up, +X right, +Y forward
        // Transformation:
        game_x = vr_x;
        game_y = -vr_z;
        game_z = vr_y;
    }

    void InitGlobals()
    {
        // This function should be called from `kMessage_DeferredInit`
        DataHandler* dataHandler = DataHandler::Get();
        if (!dataHandler) {
            _MESSAGE("FNVR | Error: Could not get DataHandler.");
            return;
        }

        const ModInfo* modInfo = dataHandler->LookupModByName("FNVRGlobals.esp");
        if (!modInfo)
        {
            _MESSAGE("FNVR | Error: FNVRGlobals.esp not found. Make sure the plugin is active.");
            
            // Debug: List all loaded mods
            _MESSAGE("FNVR | Debug: Listing all loaded mods:");
            for (UInt32 i = 0; i < dataHandler->modList.modInfoList.Count(); i++) {
                ModInfo* mod = dataHandler->modList.modInfoList.GetNthItem(i);
                if (mod) {
                    _MESSAGE("FNVR |   [%02X] %s", mod->modIndex, mod->name);
                }
            }
            return;
        }
        UInt8 modIndex = modInfo->modIndex;
        _MESSAGE("FNVR | FNVRGlobals.esp found with modIndex: %02X", modIndex);

        auto findGlobal = [&](UInt32 baseID) -> TESGlobal* {
            UInt32 formID = (modIndex << 24) | baseID;
            _MESSAGE("FNVR | Debug: Looking for FormID 0x%08X (modIndex=%02X, baseID=%06X)", formID, modIndex, baseID);
            
            TESForm* form = LookupFormByID(formID);
            if (!form) {
                _MESSAGE("FNVR | Error: Form not found for FormID 0x%08X", formID);
            } else if (form->typeID != kFormType_TESGlobal) {
                _MESSAGE("FNVR | Error: Form found but wrong type. Expected %d (TESGlobal), got %d", kFormType_TESGlobal, form->typeID);
            } else {
                _MESSAGE("FNVR | Success: Found global variable at FormID 0x%08X", formID);
                return (TESGlobal*)form;
            }
            
            _MESSAGE("FNVR | Error: Global variable with base ID %06X not found in FNVRGlobals.esp.", baseID);
            return nullptr;
        };
        
        // FormIDs - These need to be verified with your actual ESP
        FNVRStatus = findGlobal(0xAE4);      // FNVRStatus
        FNVRHMDX = findGlobal(0xAE5);       // FNVRHMDX
        FNVRHMDY = findGlobal(0xAE6);       // FNVRHMDY
        FNVRHMDZ = findGlobal(0xAE7);       // FNVRHMDZ
        FNVRHMDPitch = findGlobal(0xAE8);   // FNVRHMDPitch
        FNVRHMDYaw = findGlobal(0xAE9);     // FNVRHMDYaw
        FNVRHMDRoll = findGlobal(0xAEA);    // FNVRHMDRoll

        FNVRRightX = findGlobal(0xAEB);     // FNVRRightX
        FNVRRightY = findGlobal(0xAEC);     // FNVRRightY
        FNVRRightZ = findGlobal(0xAED);     // FNVRRightZ
        FNVRRightPitch = findGlobal(0xAEE); // FNVRRightPitch
        FNVRRightYaw = findGlobal(0xAEF);   // FNVRRightYaw
        FNVRRightRoll = findGlobal(0xAF0);  // FNVRRightRoll

        FNVRLeftX = findGlobal(0xAF1);      // FNVRLeftX
        FNVRLeftY = findGlobal(0xAF2);      // FNVRLeftY
        FNVRLeftZ = findGlobal(0xAF3);      // FNVRLeftZ
        FNVRLeftPitch = findGlobal(0xAF4);  // FNVRLeftPitch
        FNVRLeftYaw = findGlobal(0xAF5);    // FNVRLeftYaw
        FNVRLeftRoll = findGlobal(0xAF6);   // FNVRLeftRoll

        // Populate the vector for easier iteration
        allGlobals = {
            &FNVRHMDX, &FNVRHMDY, &FNVRHMDZ, &FNVRHMDPitch, &FNVRHMDYaw, &FNVRHMDRoll,
            &FNVRRightX, &FNVRRightY, &FNVRRightZ, &FNVRRightPitch, &FNVRRightYaw, &FNVRRightRoll,
            &FNVRLeftX, &FNVRLeftY, &FNVRLeftZ, &FNVRLeftPitch, &FNVRLeftYaw, &FNVRLeftRoll,
            &FNVRStatus // Status is not a tracking value, so it's not in the main reset loop
        };

        _MESSAGE("FNVR | Globals Initialized.");
        ResetGlobals(); // Set to 0 on init
    }

    void ResetGlobals()
    {
        // Set all tracking values to 0.0
        for (auto* globalPtr : allGlobals)
        {
            if (*globalPtr) {
                (*globalPtr)->data = 0.0f;
            }
        }

        // Set status to disconnected
        SAFE_SET_VALUE(FNVRStatus, 0.0f);
    }

    void UpdateGlobals(const VRDataPacket& packet)
    {
        // NVCS Skeleton sistemini kullan
        FNVR::NVCSSkeleton::Manager& skeletonMgr = FNVR::NVCSSkeleton::Manager::GetSingleton();
        
        // İlk çalıştırmada kalibrasyon
        static bool firstRun = true;
        if (firstRun) {
            skeletonMgr.Calibrate(packet);
            firstRun = false;
        }
        
        // Skeleton güncelle
        skeletonMgr.Update(packet);
        
        // VorpX mode kontrolü
        char iniPath[MAX_PATH];
        GetModuleFileNameA(NULL, iniPath, MAX_PATH);
        strcpy(strrchr(iniPath, '\\'), "\\Data\\NVSE\\Plugins\\FNVR.ini");
        int vorpxMode = GetPrivateProfileIntA("General", "VorpXMode", 0, iniPath);
        
        if (!vorpxMode) {
            // Normal mod - HMD değerleri (Head bone'dan)
            HmdVector3_t headPos = skeletonMgr.GetBonePosition(FNVR::NVCSSkeleton::NVCS_BIP01_HEAD);
            HmdQuaternionf_t headRot = skeletonMgr.GetBoneRotation(FNVR::NVCSSkeleton::NVCS_BIP01_HEAD);
            
            // Quaternion'dan Euler açılarına dönüştür
            float headPitch, headYaw, headRoll;
            QuaternionToEuler(headRot.w, headRot.x, headRot.y, headRot.z, headPitch, headYaw, headRoll);
            
            SAFE_SET_VALUE(FNVRHMDX, headPos.v[0]);
            SAFE_SET_VALUE(FNVRHMDY, headPos.v[1]);
            SAFE_SET_VALUE(FNVRHMDZ, headPos.v[2]);
            SAFE_SET_VALUE(FNVRHMDPitch, headPitch);
            SAFE_SET_VALUE(FNVRHMDYaw, headYaw);
            SAFE_SET_VALUE(FNVRHMDRoll, headRoll);
        } else {
            // VorpX modunda HMD değerlerini sıfırla (VorpX kendi tracking'ini kullanıyor)
            SAFE_SET_VALUE(FNVRHMDX, 0.0f);
            SAFE_SET_VALUE(FNVRHMDY, 0.0f);
            SAFE_SET_VALUE(FNVRHMDZ, 0.0f);
            SAFE_SET_VALUE(FNVRHMDPitch, 0.0f);
            SAFE_SET_VALUE(FNVRHMDYaw, 0.0f);
            SAFE_SET_VALUE(FNVRHMDRoll, 0.0f);
        }
        
        // Right Hand değerleri (Weapon bone'dan daha iyi olabilir)
        HmdVector3_t weaponPos = skeletonMgr.GetBonePosition(FNVR::NVCSSkeleton::NVCS_WEAPON);
        HmdQuaternionf_t weaponRot = skeletonMgr.GetBoneRotation(FNVR::NVCSSkeleton::NVCS_WEAPON);
        
        float weaponPitch, weaponYaw, weaponRoll;
        QuaternionToEuler(weaponRot.w, weaponRot.x, weaponRot.y, weaponRot.z, weaponPitch, weaponYaw, weaponRoll);
        
        SAFE_SET_VALUE(FNVRRightX, weaponPos.v[0]);
        SAFE_SET_VALUE(FNVRRightY, weaponPos.v[1]);
        SAFE_SET_VALUE(FNVRRightZ, weaponPos.v[2]);
        SAFE_SET_VALUE(FNVRRightPitch, weaponPitch);
        SAFE_SET_VALUE(FNVRRightYaw, weaponYaw);
        SAFE_SET_VALUE(FNVRRightRoll, weaponRoll);
        
        // Left Hand değerleri (şimdilik sağ el verisini mirror et)
        HmdVector3_t leftHandPos = skeletonMgr.GetBonePosition(FNVR::NVCSSkeleton::NVCS_BIP01_L_HAND);
        HmdQuaternionf_t leftHandRot = skeletonMgr.GetBoneRotation(FNVR::NVCSSkeleton::NVCS_BIP01_L_HAND);
        
        float leftPitch, leftYaw, leftRoll;
        QuaternionToEuler(leftHandRot.w, leftHandRot.x, leftHandRot.y, leftHandRot.z, leftPitch, leftYaw, leftRoll);
        
        SAFE_SET_VALUE(FNVRLeftX, leftHandPos.v[0]);
        SAFE_SET_VALUE(FNVRLeftY, leftHandPos.v[1]);
        SAFE_SET_VALUE(FNVRLeftZ, leftHandPos.v[2]);
        SAFE_SET_VALUE(FNVRLeftPitch, leftPitch);
        SAFE_SET_VALUE(FNVRLeftYaw, leftYaw);
        SAFE_SET_VALUE(FNVRLeftRoll, leftRoll);
        
        // Status güncellemesi
        SAFE_SET_VALUE(FNVRStatus, 1.0f); // Bağlı
        
        // Debug log (her 120 frame'de bir)
        static int frameCount = 0;
        if (++frameCount % 120 == 0) {
            skeletonMgr.LogBonePositions();
        }
    }
} 