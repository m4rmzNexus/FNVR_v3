#pragma once
#include "Globals.h"
#include "VRSystem.h"
#include <map>
#include <string>

namespace FNVR {

// FRIK tarzı gelişmiş skeleton yapısı
class FRIKSkeleton {
public:
    // Gelişmiş bone tanımlamaları
    struct BoneInfo {
        std::string name;
        int parentIndex;
        HmdVector3_t localPosition;
        HmdQuaternionf_t localRotation;
        float length;
        bool isIKTarget;
        bool isWeaponAttachPoint;
    };

    // FRIK tarzı skeleton bone listesi
    enum BoneIndex {
        // Root ve Pelvis
        BONE_ROOT = 0,
        BONE_PELVIS,
        
        // Omurga (Spine)
        BONE_SPINE,
        BONE_SPINE1,
        BONE_SPINE2,
        BONE_SPINE3,  // FRIK ekstra spine
        
        // Boyun ve Kafa
        BONE_NECK,
        BONE_NECK1,   // FRIK ekstra neck
        BONE_HEAD,
        
        // Sol Kol
        BONE_L_CLAVICLE,
        BONE_L_UPPERARM,
        BONE_L_FOREARM,
        BONE_L_HAND,
        
        // Sol El Parmakları (FRIK özelliği)
        BONE_L_THUMB1, BONE_L_THUMB2, BONE_L_THUMB3,
        BONE_L_INDEX1, BONE_L_INDEX2, BONE_L_INDEX3,
        BONE_L_MIDDLE1, BONE_L_MIDDLE2, BONE_L_MIDDLE3,
        BONE_L_RING1, BONE_L_RING2, BONE_L_RING3,
        BONE_L_PINKY1, BONE_L_PINKY2, BONE_L_PINKY3,
        
        // Sağ Kol
        BONE_R_CLAVICLE,
        BONE_R_UPPERARM,
        BONE_R_FOREARM,
        BONE_R_HAND,
        
        // Sağ El Parmakları (FRIK özelliği)
        BONE_R_THUMB1, BONE_R_THUMB2, BONE_R_THUMB3,
        BONE_R_INDEX1, BONE_R_INDEX2, BONE_R_INDEX3,
        BONE_R_MIDDLE1, BONE_R_MIDDLE2, BONE_R_MIDDLE3,
        BONE_R_RING1, BONE_R_RING2, BONE_R_RING3,
        BONE_R_PINKY1, BONE_R_PINKY2, BONE_R_PINKY3,
        
        // Sol Bacak
        BONE_L_THIGH,
        BONE_L_CALF,
        BONE_L_FOOT,
        BONE_L_TOE,    // FRIK ekstra toe
        
        // Sağ Bacak
        BONE_R_THIGH,
        BONE_R_CALF,
        BONE_R_FOOT,
        BONE_R_TOE,    // FRIK ekstra toe
        
        // Silah bağlantı noktaları (FRIK özelliği)
        BONE_WEAPON_PRIMARY,
        BONE_WEAPON_SECONDARY,
        BONE_WEAPON_HOLSTER_HIP,
        BONE_WEAPON_HOLSTER_BACK,
        
        // IK hedef noktaları (FRIK özelliği)
        BONE_IK_HAND_L,
        BONE_IK_HAND_R,
        BONE_IK_FOOT_L,
        BONE_IK_FOOT_R,
        BONE_IK_ELBOW_L,
        BONE_IK_ELBOW_R,
        BONE_IK_KNEE_L,
        BONE_IK_KNEE_R,
        
        BONE_COUNT
    };

    // Bone zinciri (IK için)
    struct BoneChain {
        std::vector<int> boneIndices;
        float totalLength;
        bool isArm;
        bool isLeg;
    };

    // Weapon attachment sistemi (FRIK tarzı)
    struct WeaponAttachment {
        BoneIndex attachBone;
        HmdVector3_t offset;
        HmdQuaternionf_t rotation;
        float scale;
        bool useTwoHandedGrip;
        BoneIndex secondaryGripBone;
    };

private:
    std::vector<BoneInfo> bones;
    std::map<std::string, int> boneNameToIndex;
    std::vector<BoneChain> ikChains;
    std::map<std::string, WeaponAttachment> weaponAttachments;
    
    // Mevcut pose
    std::vector<HmdMatrix34_t> currentPose;
    std::vector<HmdMatrix34_t> worldPose;

public:
    FRIKSkeleton();
    ~FRIKSkeleton();

    // Başlatma
    void Initialize();
    void LoadSkeletonDefinition(const std::string& path);
    
    // Bone erişimi
    int GetBoneIndex(const std::string& name) const;
    const BoneInfo& GetBone(int index) const;
    
    // Pose güncelleme
    void UpdateFromVR(const VRDataPacket& packet);
    void ApplyIK();
    void UpdateWorldPose();
    
    // IK sistemi (FRIK tarzı)
    void SolveTwoBoneIK(const BoneChain& chain, const HmdVector3_t& target, 
                        const HmdVector3_t& poleVector);
    void SolveFingerIK(int handBoneIndex, const VRInput::Gesture& gesture);
    
    // Weapon attachment
    void AttachWeapon(const std::string& weaponType, BoneIndex bone);
    void UpdateWeaponPose(const std::string& weaponType);
    HmdMatrix34_t GetWeaponTransform(const std::string& weaponType) const;
    
    // Animasyon blending
    void BlendWithAnimation(const std::string& animName, float blendFactor);
    
    // Debug ve görselleştirme
    void DrawDebugSkeleton();
    void ExportPose(const std::string& filename);
};

// FRIK tarzı el poz sistemi
class HandPoseSystem {
public:
    struct FingerPose {
        float curl[3];      // Her eklem için bükülme açısı
        float spread;       // Parmak yayılma açısı
    };
    
    struct HandPose {
        FingerPose thumb;
        FingerPose index;
        FingerPose middle;
        FingerPose ring;
        FingerPose pinky;
        float wristBend;
        float wristTwist;
    };

    // Önceden tanımlı el pozları (FRIK tarzı)
    static HandPose GetRelaxedPose();
    static HandPose GetFistPose();
    static HandPose GetPointingPose();
    static HandPose GetGripPose();
    static HandPose GetOpenPose();
    static HandPose GetWeaponGripPose(const std::string& weaponType);
    
    // Poz interpolasyonu
    static HandPose InterpolatePoses(const HandPose& a, const HandPose& b, float t);
    
    // Controller'dan el pozuna dönüşüm
    static HandPose CalculateFromController(const VRInput::ControllerState& state);
};

} // namespace FNVR 