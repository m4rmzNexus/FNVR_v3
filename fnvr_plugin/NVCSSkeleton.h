#pragma once
#include "Globals.h"
#include "VRSystem.h"
#include <map>
#include <string>

namespace FNVR {

// NVCS (New Vegas Compatibility Skeleton) tabanlı VR sistemi
class NVCSSkeleton {
public:
    // NVCS Bone tanımlamaları
    enum NVCSBone {
        // Root
        NVCS_BIP01 = 0,
        NVCS_BIP01_NONACCUM,
        NVCS_BIP01_PELVIS,
        
        // Spine
        NVCS_BIP01_SPINE,
        NVCS_BIP01_SPINE1,
        NVCS_BIP01_SPINE2,
        
        // Neck & Head
        NVCS_BIP01_NECK,
        NVCS_BIP01_NECK1,
        NVCS_BIP01_HEAD,
        
        // Right Arm
        NVCS_BIP01_R_CLAVICLE,
        NVCS_BIP01_R_UPPERARM,
        NVCS_BIP01_R_FOREARM,
        NVCS_BIP01_R_HAND,
        
        // Left Arm
        NVCS_BIP01_L_CLAVICLE,
        NVCS_BIP01_L_UPPERARM,
        NVCS_BIP01_L_FOREARM,
        NVCS_BIP01_L_HAND,
        
        // Right Hand Fingers
        NVCS_BIP01_R_FINGER0,
        NVCS_BIP01_R_FINGER01,
        NVCS_BIP01_R_FINGER02,
        NVCS_BIP01_R_FINGER1,
        NVCS_BIP01_R_FINGER11,
        NVCS_BIP01_R_FINGER12,
        
        // Weapon Bones
        NVCS_WEAPON,
        NVCS_WEAPON2,
        
        // Camera
        NVCS_CAMERA1ST,
        
        // Legs (for full body tracking future)
        NVCS_BIP01_L_THIGH,
        NVCS_BIP01_L_CALF,
        NVCS_BIP01_L_FOOT,
        NVCS_BIP01_R_THIGH,
        NVCS_BIP01_R_CALF,
        NVCS_BIP01_R_FOOT,
        
        NVCS_BONE_COUNT
    };

    // Bone isimleri
    static const char* GetBoneName(NVCSBone bone);
    
    // VR Controller'dan NVCS bone'larına mapping
    struct VRToNVCSMapping {
        // HMD -> Head/Camera mapping
        void MapHMDToHead(const VRDataPacket& vrData, HmdVector3_t& headPos, HmdQuaternionf_t& headRot);
        
        // Right Controller -> Right Hand/Weapon mapping
        void MapControllerToHand(const VRDataPacket& vrData, bool isRight, 
                                HmdVector3_t& handPos, HmdQuaternionf_t& handRot);
        
        // IK hesaplamaları
        void CalculateArmIK(const HmdVector3_t& shoulderPos, const HmdVector3_t& handPos,
                           float upperArmLength, float foreArmLength,
                           HmdQuaternionf_t& upperArmRot, HmdQuaternionf_t& foreArmRot);
    };

    // NVCS skeleton yönetimi
    class Manager {
    private:
        static Manager* s_instance;
        
        // Bone pozisyon ve rotasyonları
        std::map<NVCSBone, HmdVector3_t> m_bonePositions;
        std::map<NVCSBone, HmdQuaternionf_t> m_boneRotations;
        
        // Kalibre edilmiş değerler
        float m_shoulderWidth = 40.0f;      // Omuz genişliği (game units)
        float m_upperArmLength = 30.0f;     // Üst kol uzunluğu
        float m_foreArmLength = 25.0f;      // Alt kol uzunluğu
        float m_playerHeight = 175.0f;      // Oyuncu boyu (game units)
        
        // VR to NVCS mapper
        VRToNVCSMapping m_mapper;
        
    public:
        static Manager& GetSingleton();
        
        void Initialize();
        void Update(const VRDataPacket& vrData);
        void UpdateVorpXMode(const VRDataPacket& vrData);
        void Calibrate(const VRDataPacket& vrData);
        
        // Bone getter/setter
        HmdVector3_t GetBonePosition(NVCSBone bone) const;
        HmdQuaternionf_t GetBoneRotation(NVCSBone bone) const;
        
        // Weapon positioning
        void UpdateWeaponPosition(const HmdVector3_t& handPos, const HmdQuaternionf_t& handRot);
        
        // Debug
        void LogBonePositions();
    };
};

} // namespace FNVR 