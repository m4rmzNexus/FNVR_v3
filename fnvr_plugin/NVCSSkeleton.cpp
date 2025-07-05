#include "NVCSSkeleton.h"
#include "nvse/PluginAPI.h"
#include <cmath>

namespace FNVR {

// Bone isimleri - NVCS skeleton'daki gerçek isimler
const char* NVCSSkeleton::GetBoneName(NVCSBone bone) {
    static const char* boneNames[] = {
        "Bip01",
        "Bip01 NonAccum",
        "Bip01 Pelvis",
        "Bip01 Spine",
        "Bip01 Spine1", 
        "Bip01 Spine2",
        "Bip01 Neck",
        "Bip01 Neck1",
        "Bip01 Head",
        "Bip01 R Clavicle",
        "Bip01 R UpperArm",
        "Bip01 R Forearm",
        "Bip01 R Hand",
        "Bip01 L Clavicle",
        "Bip01 L UpperArm",
        "Bip01 L Forearm",
        "Bip01 L Hand",
        "Bip01 R Finger0",
        "Bip01 R Finger01",
        "Bip01 R Finger02",
        "Bip01 R Finger1",
        "Bip01 R Finger11",
        "Bip01 R Finger12",
        "Weapon",
        "Weapon2",
        "Camera1st",
        "Bip01 L Thigh",
        "Bip01 L Calf",
        "Bip01 L Foot",
        "Bip01 R Thigh",
        "Bip01 R Calf",
        "Bip01 R Foot"
    };
    
    if (bone >= 0 && bone < NVCS_BONE_COUNT) {
        return boneNames[bone];
    }
    return "Unknown";
}

// VR to NVCS Mapping Implementation
void NVCSSkeleton::VRToNVCSMapping::MapHMDToHead(const VRDataPacket& vrData, 
                                                  HmdVector3_t& headPos, 
                                                  HmdQuaternionf_t& headRot) {
    // OpenVR'dan gelen HMD verisini Gamebryo koordinatlarına dönüştür
    // OpenVR: +Y up, -Z forward, +X right
    // Gamebryo: +Z up, +Y forward, +X right
    // Fallout NV: 70 units = 1 meter
    
    // Pozisyon dönüşümü - doğru mapping
    headPos.v[0] = vrData.hmd_px * 70.0f;   // X aynı kalır
    headPos.v[1] = vrData.hmd_pz * 70.0f;   // Y = Z (OpenVR forward = Gamebryo forward)
    headPos.v[2] = vrData.hmd_py * 70.0f;   // Z = Y (OpenVR up = Gamebryo up)
    
    // Quaternion dönüşümü - doğru rotasyon
    // OpenVR quaternion'ı Gamebryo'ya dönüştür
    headRot.w = vrData.hmd_qw;
    headRot.x = vrData.hmd_qx;
    headRot.y = vrData.hmd_qz;  // Y ve Z eksenleri yer değiştirir
    headRot.z = vrData.hmd_qy;
}

void NVCSSkeleton::VRToNVCSMapping::MapControllerToHand(const VRDataPacket& vrData, 
                                                         bool isRight,
                                                         HmdVector3_t& handPos, 
                                                         HmdQuaternionf_t& handRot) {
    if (isRight) {
        // Sağ controller pozisyonu - düzeltilmiş koordinat sistemi
        handPos.v[0] = vrData.right_px * 70.0f;   // X aynı
        handPos.v[1] = vrData.right_pz * 70.0f;   // Y = Z (forward)
        handPos.v[2] = vrData.right_py * 70.0f;   // Z = Y (up)
        
        // Sağ controller rotasyonu - düzeltilmiş
        handRot.w = vrData.right_qw;
        handRot.x = vrData.right_qx;
        handRot.y = vrData.right_qz;  // Y ve Z yer değiştirir
        handRot.z = vrData.right_qy;
        
        // Controller grip rotasyonu düzeltmesi (silah doğru tutulsun)
        // Vive/Index controller'lar için tipik düzeltme
        float gripAngle = -45.0f * 3.14159f / 180.0f; // -45 derece pitch
        float cosHalf = cos(gripAngle / 2);
        float sinHalf = sin(gripAngle / 2);
        
        // Pitch rotasyonu ekle (X ekseni etrafında)
        HmdQuaternionf_t gripRot = {cosHalf, sinHalf, 0, 0};
        
        // Quaternion çarpımı ile birleştir
        HmdQuaternionf_t result;
        result.w = handRot.w * gripRot.w - handRot.x * gripRot.x - handRot.y * gripRot.y - handRot.z * gripRot.z;
        result.x = handRot.w * gripRot.x + handRot.x * gripRot.w + handRot.y * gripRot.z - handRot.z * gripRot.y;
        result.y = handRot.w * gripRot.y - handRot.x * gripRot.z + handRot.y * gripRot.w + handRot.z * gripRot.x;
        result.z = handRot.w * gripRot.z + handRot.x * gripRot.y - handRot.y * gripRot.x + handRot.z * gripRot.w;
        
        handRot = result;
    } else {
        // Sol controller desteği - mirror sağ controller
        handPos.v[0] = -vrData.right_px * 70.0f;  // X'i ters çevir (sol tarafa)
        handPos.v[1] = vrData.right_pz * 70.0f;   
        handPos.v[2] = vrData.right_py * 70.0f;
        
        // Sol el için rotasyon (şimdilik basit mirror)
        handRot.w = vrData.right_qw;
        handRot.x = -vrData.right_qx;  // X rotasyonunu ters çevir
        handRot.y = vrData.right_qz;
        handRot.z = vrData.right_qy;
    }
}

void NVCSSkeleton::VRToNVCSMapping::CalculateArmIK(const HmdVector3_t& shoulderPos, 
                                                    const HmdVector3_t& handPos,
                                                    float upperArmLength, 
                                                    float foreArmLength,
                                                    HmdQuaternionf_t& upperArmRot, 
                                                    HmdQuaternionf_t& foreArmRot) {
    // Basit 2-bone IK hesaplaması
    float dx = handPos.v[0] - shoulderPos.v[0];
    float dy = handPos.v[1] - shoulderPos.v[1];
    float dz = handPos.v[2] - shoulderPos.v[2];
    float distance = sqrt(dx*dx + dy*dy + dz*dz);
    
    // Mesafe çok uzaksa, maksimum uzanma mesafesine kısıtla
    float maxReach = upperArmLength + foreArmLength - 1.0f;
    if (distance > maxReach) {
        float scale = maxReach / distance;
        dx *= scale;
        dy *= scale;
        dz *= scale;
        distance = maxReach;
    }
    
    // Cosine rule ile açıları hesapla
    float a = upperArmLength;
    float b = foreArmLength;
    float c = distance;
    
    // Dirsek açısı
    float elbowAngle = acos((a*a + b*b - c*c) / (2*a*b));
    
    // Omuz açısı
    float shoulderAngle = acos((a*a + c*c - b*b) / (2*a*c));
    
    // Quaternion'lara dönüştür (basitleştirilmiş)
    // TODO: Gerçek 3D IK implementasyonu
    upperArmRot.w = cos(shoulderAngle / 2);
    upperArmRot.x = sin(shoulderAngle / 2);
    upperArmRot.y = 0;
    upperArmRot.z = 0;
    
    foreArmRot.w = cos(elbowAngle / 2);
    foreArmRot.x = sin(elbowAngle / 2);
    foreArmRot.y = 0;
    foreArmRot.z = 0;
}

// Manager Implementation
NVCSSkeleton::Manager* NVCSSkeleton::Manager::s_instance = nullptr;

NVCSSkeleton::Manager& NVCSSkeleton::Manager::GetSingleton() {
    if (!s_instance) {
        s_instance = new Manager();
    }
    return *s_instance;
}

void NVCSSkeleton::Manager::Initialize() {
    _MESSAGE("FNVR | NVCS Skeleton Manager initialized");
    
    // Başlangıç pozisyonlarını ayarla
    for (int i = 0; i < NVCS_BONE_COUNT; i++) {
        m_bonePositions[static_cast<NVCSBone>(i)] = {0, 0, 0};
        m_boneRotations[static_cast<NVCSBone>(i)] = {1, 0, 0, 0}; // Identity quaternion
    }
    
    // INI dosyasından kalibre değerlerini oku
    char iniPath[MAX_PATH];
    GetModuleFileNameA(NULL, iniPath, MAX_PATH);
    strcpy(strrchr(iniPath, '\\'), "\\Data\\NVSE\\Plugins\\FNVR.ini");
    
    m_shoulderWidth = GetPrivateProfileIntA("NVCS", "ShoulderWidth", 40, iniPath);
    m_upperArmLength = GetPrivateProfileIntA("NVCS", "UpperArmLength", 30, iniPath);
    m_foreArmLength = GetPrivateProfileIntA("NVCS", "ForeArmLength", 25, iniPath);
    m_playerHeight = GetPrivateProfileIntA("NVCS", "PlayerHeight", 175, iniPath);
}

void NVCSSkeleton::Manager::Update(const VRDataPacket& vrData) {
    // VorpX mode kontrolü
    char iniPath[MAX_PATH];
    GetModuleFileNameA(NULL, iniPath, MAX_PATH);
    strcpy(strrchr(iniPath, '\\'), "\\Data\\NVSE\\Plugins\\FNVR.ini");
    
    int vorpxMode = GetPrivateProfileIntA("General", "VorpXMode", 0, iniPath);
    
    if (vorpxMode) {
        // VorpX modunda sadece controller tracking
        UpdateVorpXMode(vrData);
        return;
    }
    
    // Normal mod - HMD -> Head mapping
    HmdVector3_t headPos;
    HmdQuaternionf_t headRot;
    m_mapper.MapHMDToHead(vrData, headPos, headRot);
    
    m_bonePositions[NVCS_BIP01_HEAD] = headPos;
    m_boneRotations[NVCS_BIP01_HEAD] = headRot;
    
    // Camera pozisyonunu player eye node'una ayarla
    // Bu 1st person body'nin görünmesini sağlar
    HmdVector3_t cameraPos = headPos;
    // VorpX için kamera offset'i kaldır
    if (!vorpxMode) {
        cameraPos.v[1] += 8.0f;  // 8 unit ileri
        cameraPos.v[2] += 5.0f;  // 5 unit yukarı
    }
    
    m_bonePositions[NVCS_CAMERA1ST] = cameraPos;
    m_boneRotations[NVCS_CAMERA1ST] = headRot;
    
    // Right Controller -> Right Hand mapping
    HmdVector3_t rightHandPos;
    HmdQuaternionf_t rightHandRot;
    m_mapper.MapControllerToHand(vrData, true, rightHandPos, rightHandRot);
    
    m_bonePositions[NVCS_BIP01_R_HAND] = rightHandPos;
    m_boneRotations[NVCS_BIP01_R_HAND] = rightHandRot;
    
    // Omuz pozisyonunu hesapla (head'den aşağı ve sağa)
    HmdVector3_t rightShoulderPos;
    rightShoulderPos.v[0] = headPos.v[0] + (m_shoulderWidth / 2);
    rightShoulderPos.v[1] = headPos.v[1];
    rightShoulderPos.v[2] = headPos.v[2] - 15.0f; // Omuz head'den aşağıda
    
    m_bonePositions[NVCS_BIP01_R_CLAVICLE] = rightShoulderPos;
    
    // IK ile kol pozisyonlarını hesapla
    HmdQuaternionf_t upperArmRot, foreArmRot;
    m_mapper.CalculateArmIK(rightShoulderPos, rightHandPos, 
                           m_upperArmLength, m_foreArmLength,
                           upperArmRot, foreArmRot);
    
    m_boneRotations[NVCS_BIP01_R_UPPERARM] = upperArmRot;
    m_boneRotations[NVCS_BIP01_R_FOREARM] = foreArmRot;
    
    // Weapon pozisyonunu güncelle
    UpdateWeaponPosition(rightHandPos, rightHandRot);
}

void NVCSSkeleton::Manager::UpdateWeaponPosition(const HmdVector3_t& handPos, 
                                                  const HmdQuaternionf_t& handRot) {
    // INI dosyasından weapon offset'leri oku
    char iniPath[MAX_PATH];
    GetModuleFileNameA(NULL, iniPath, MAX_PATH);
    strcpy(strrchr(iniPath, '\\'), "\\Data\\NVSE\\Plugins\\FNVR.ini");
    
    char buffer[32];
    GetPrivateProfileStringA("NVCS", "WeaponOffsetX", "0.0", buffer, sizeof(buffer), iniPath);
    float offsetX = (float)atof(buffer);
    GetPrivateProfileStringA("NVCS", "WeaponOffsetY", "10.0", buffer, sizeof(buffer), iniPath);
    float offsetY = (float)atof(buffer);
    GetPrivateProfileStringA("NVCS", "WeaponOffsetZ", "-5.0", buffer, sizeof(buffer), iniPath);
    float offsetZ = (float)atof(buffer);
    
    // Weapon bone'u el pozisyonuna göre ayarla
    // Offset'leri el rotasyonuna göre dönüştür (local space)
    HmdVector3_t weaponPos = handPos;
    
    // Quaternion'dan rotation matrix'e dönüştür (basitleştirilmiş)
    float qw = handRot.w, qx = handRot.x, qy = handRot.y, qz = handRot.z;
    
    // Right vector
    float rx = 1 - 2*(qy*qy + qz*qz);
    float ry = 2*(qx*qy + qw*qz);
    float rz = 2*(qx*qz - qw*qy);
    
    // Forward vector  
    float fx = 2*(qx*qy - qw*qz);
    float fy = 1 - 2*(qx*qx + qz*qz);
    float fz = 2*(qy*qz + qw*qx);
    
    // Up vector
    float ux = 2*(qx*qz + qw*qy);
    float uy = 2*(qy*qz - qw*qx);
    float uz = 1 - 2*(qx*qx + qy*qy);
    
    // Local offset'leri world space'e dönüştür
    weaponPos.v[0] += rx * offsetX + fx * offsetY + ux * offsetZ;
    weaponPos.v[1] += ry * offsetX + fy * offsetY + uy * offsetZ;
    weaponPos.v[2] += rz * offsetX + fz * offsetY + uz * offsetZ;
    
    m_bonePositions[NVCS_WEAPON] = weaponPos;
    m_boneRotations[NVCS_WEAPON] = handRot;
}

void NVCSSkeleton::Manager::UpdateVorpXMode(const VRDataPacket& vrData) {
    // VorpX modunda sadece controller pozisyonlarını güncelle
    // Head tracking VorpX tarafından yapılıyor
    
    // Right Controller -> Right Hand mapping
    HmdVector3_t rightHandPos;
    HmdQuaternionf_t rightHandRot;
    m_mapper.MapControllerToHand(vrData, true, rightHandPos, rightHandRot);
    
    // VorpX için özel ölçekleme
    float vorpxScale = 1.0f;  // VorpX kendi ölçeklemeyi yapıyor
    rightHandPos.v[0] *= vorpxScale;
    rightHandPos.v[1] *= vorpxScale;
    rightHandPos.v[2] *= vorpxScale;
    
    m_bonePositions[NVCS_BIP01_R_HAND] = rightHandPos;
    m_boneRotations[NVCS_BIP01_R_HAND] = rightHandRot;
    
    // Weapon pozisyonunu güncelle
    UpdateWeaponPosition(rightHandPos, rightHandRot);
    
    // Sol el (mirror)
    HmdVector3_t leftHandPos;
    HmdQuaternionf_t leftHandRot;
    m_mapper.MapControllerToHand(vrData, false, leftHandPos, leftHandRot);
    
    leftHandPos.v[0] *= vorpxScale;
    leftHandPos.v[1] *= vorpxScale;
    leftHandPos.v[2] *= vorpxScale;
    
    m_bonePositions[NVCS_BIP01_L_HAND] = leftHandPos;
    m_boneRotations[NVCS_BIP01_L_HAND] = leftHandRot;
}

void NVCSSkeleton::Manager::Calibrate(const VRDataPacket& vrData) {
    // T-pose kalibrasyonu
    _MESSAGE("FNVR | Calibrating NVCS skeleton...");
    
    // Oyuncu boyunu HMD yüksekliğinden hesapla
    m_playerHeight = vrData.hmd_py * 70.0f;
    
    // Omuz genişliğini tahmin et (boy oranına göre)
    m_shoulderWidth = m_playerHeight * 0.25f;
    
    // Kol uzunluklarını tahmin et
    m_upperArmLength = m_playerHeight * 0.17f;
    m_foreArmLength = m_playerHeight * 0.15f;
    
    _MESSAGE("FNVR | Calibration complete: Height=%.1f, Shoulder=%.1f", 
             m_playerHeight, m_shoulderWidth);
}

HmdVector3_t NVCSSkeleton::Manager::GetBonePosition(NVCSBone bone) const {
    auto it = m_bonePositions.find(bone);
    if (it != m_bonePositions.end()) {
        return it->second;
    }
    return {0, 0, 0};
}

HmdQuaternionf_t NVCSSkeleton::Manager::GetBoneRotation(NVCSBone bone) const {
    auto it = m_boneRotations.find(bone);
    if (it != m_boneRotations.end()) {
        return it->second;
    }
    return {1, 0, 0, 0};
}

void NVCSSkeleton::Manager::LogBonePositions() {
    _MESSAGE("FNVR | NVCS Bone Positions:");
    _MESSAGE("  Head: %.1f, %.1f, %.1f", 
             m_bonePositions[NVCS_BIP01_HEAD].v[0],
             m_bonePositions[NVCS_BIP01_HEAD].v[1],
             m_bonePositions[NVCS_BIP01_HEAD].v[2]);
    _MESSAGE("  R Hand: %.1f, %.1f, %.1f",
             m_bonePositions[NVCS_BIP01_R_HAND].v[0],
             m_bonePositions[NVCS_BIP01_R_HAND].v[1],
             m_bonePositions[NVCS_BIP01_R_HAND].v[2]);
    _MESSAGE("  Weapon: %.1f, %.1f, %.1f",
             m_bonePositions[NVCS_WEAPON].v[0],
             m_bonePositions[NVCS_WEAPON].v[1],
             m_bonePositions[NVCS_WEAPON].v[2]);
}

} // namespace FNVR 