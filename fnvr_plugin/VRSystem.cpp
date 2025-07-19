#include "internal/prefix.h"  // JIP-LN SDK prefix - temel tipler için
#include "VRSystem.h"
#include <fstream>
#include <cmath>

// Basit log makrosu
#ifndef _MESSAGE
#define _MESSAGE(fmt, ...) ((void)0)
#endif

namespace FNVR {

// Matematik yardımcı fonksiyonları
static const float PI = 3.14159265359f;
static const float DEG2RAD = PI / 180.0f;
static const float RAD2DEG = 180.0f / PI;

// Matrix işlemleri
HmdVector3_t VRManager::TransformPoint(const HmdVector3_t& point, const HmdMatrix34_t& matrix) {
    HmdVector3_t result;
    result.v[0] = matrix.m[0][0] * point.v[0] + matrix.m[0][1] * point.v[1] + matrix.m[0][2] * point.v[2] + matrix.m[0][3];
    result.v[1] = matrix.m[1][0] * point.v[0] + matrix.m[1][1] * point.v[1] + matrix.m[1][2] * point.v[2] + matrix.m[1][3];
    result.v[2] = matrix.m[2][0] * point.v[0] + matrix.m[2][1] * point.v[1] + matrix.m[2][2] * point.v[2] + matrix.m[2][3];
    return result;
}

HmdQuaternionf_t VRManager::MatrixToQuaternion(const HmdMatrix34_t& matrix) {
    HmdQuaternionf_t q;
    float trace = matrix.m[0][0] + matrix.m[1][1] + matrix.m[2][2];
    
    if (trace > 0.0f) {
        float s = 0.5f / sqrtf(trace + 1.0f);
        q.w = 0.25f / s;
        q.x = (matrix.m[2][1] - matrix.m[1][2]) * s;
        q.y = (matrix.m[0][2] - matrix.m[2][0]) * s;
        q.z = (matrix.m[1][0] - matrix.m[0][1]) * s;
    } else {
        if (matrix.m[0][0] > matrix.m[1][1] && matrix.m[0][0] > matrix.m[2][2]) {
            float s = 2.0f * sqrtf(1.0f + matrix.m[0][0] - matrix.m[1][1] - matrix.m[2][2]);
            q.w = (matrix.m[2][1] - matrix.m[1][2]) / s;
            q.x = 0.25f * s;
            q.y = (matrix.m[0][1] + matrix.m[1][0]) / s;
            q.z = (matrix.m[0][2] + matrix.m[2][0]) / s;
        } else if (matrix.m[1][1] > matrix.m[2][2]) {
            float s = 2.0f * sqrtf(1.0f + matrix.m[1][1] - matrix.m[0][0] - matrix.m[2][2]);
            q.w = (matrix.m[0][2] - matrix.m[2][0]) / s;
            q.x = (matrix.m[0][1] + matrix.m[1][0]) / s;
            q.y = 0.25f * s;
            q.z = (matrix.m[1][2] + matrix.m[2][1]) / s;
        } else {
            float s = 2.0f * sqrtf(1.0f + matrix.m[2][2] - matrix.m[0][0] - matrix.m[1][1]);
            q.w = (matrix.m[1][0] - matrix.m[0][1]) / s;
            q.x = (matrix.m[0][2] + matrix.m[2][0]) / s;
            q.y = (matrix.m[1][2] + matrix.m[2][1]) / s;
            q.z = 0.25f * s;
        }
    }
    return q;
}

HmdMatrix34_t VRManager::QuaternionToMatrix(const HmdQuaternionf_t& q) {
    HmdMatrix34_t matrix;
    
    float xx = q.x * q.x;
    float yy = q.y * q.y;
    float zz = q.z * q.z;
    float xy = q.x * q.y;
    float xz = q.x * q.z;
    float yz = q.y * q.z;
    float wx = q.w * q.x;
    float wy = q.w * q.y;
    float wz = q.w * q.z;
    
    matrix.m[0][0] = 1.0f - 2.0f * (yy + zz);
    matrix.m[0][1] = 2.0f * (xy - wz);
    matrix.m[0][2] = 2.0f * (xz + wy);
    matrix.m[0][3] = 0.0f;
    
    matrix.m[1][0] = 2.0f * (xy + wz);
    matrix.m[1][1] = 1.0f - 2.0f * (xx + zz);
    matrix.m[1][2] = 2.0f * (yz - wx);
    matrix.m[1][3] = 0.0f;
    
    matrix.m[2][0] = 2.0f * (xz - wy);
    matrix.m[2][1] = 2.0f * (yz + wx);
    matrix.m[2][2] = 1.0f - 2.0f * (xx + yy);
    matrix.m[2][3] = 0.0f;
    
    return matrix;
}

HmdMatrix34_t VRManager::MultiplyMatrices(const HmdMatrix34_t& a, const HmdMatrix34_t& b) {
    HmdMatrix34_t result;
    
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) {
            result.m[i][j] = 0.0f;
            for (int k = 0; k < 3; k++) {
                result.m[i][j] += a.m[i][k] * b.m[k][j];
            }
            if (j == 3) {
                result.m[i][j] += a.m[i][3];
            }
        }
    }
    
    return result;
}

// Coordinate system conversions
void VRManager::ConvertOpenVRToGamebryo(const HmdVector3_t& vrPos, HmdVector3_t& gamePos, float scale) {
    // OpenVR: Right-handed, +Y up, +X right, -Z forward (meters)
    // Gamebryo: Left-handed, +Z up, +X right, +Y forward (game units)
    // Verified from Gemini analysis and community testing
    
    gamePos.v[0] =  vrPos.v[0] * scale;  // X -> X (right stays right)
    gamePos.v[1] = -vrPos.v[2] * scale;  // -Z -> Y (forward)
    gamePos.v[2] =  vrPos.v[1] * scale;  // Y -> Z (up)
}

void VRManager::ConvertGamebryoToOpenVR(const HmdVector3_t& gamePos, HmdVector3_t& vrPos, float scale) {
    // Inverse transformation (rarely needed)
    if (scale != 0.0f) {
        vrPos.v[0] =  gamePos.v[0] / scale;  // X -> X
        vrPos.v[1] =  gamePos.v[2] / scale;  // Z -> Y
        vrPos.v[2] = -gamePos.v[1] / scale;  // Y -> -Z
    }
}

// Quaternion conversion for rotations
void VRManager::ConvertOpenVRQuaternionToGamebryo(const HmdQuaternion_t& vrQuat, HmdQuaternion_t& gameQuat) {
    // OpenVR to Gamebryo requires a -90 degree rotation around X axis
    // This is equivalent to pre-multiplying by quaternion (0.7071, -0.7071, 0, 0)
    const float r_sqrt2_inv = 0.7071067811865476f; // 1/sqrt(2)
    
    gameQuat.w = (vrQuat.w + vrQuat.x) * r_sqrt2_inv;
    gameQuat.x = (vrQuat.x - vrQuat.w) * r_sqrt2_inv;
    gameQuat.y = (vrQuat.y + vrQuat.z) * r_sqrt2_inv;
    gameQuat.z = (vrQuat.z - vrQuat.y) * r_sqrt2_inv;
    
    // Normalize to prevent drift
    float mag = sqrtf(gameQuat.w*gameQuat.w + gameQuat.x*gameQuat.x + 
                     gameQuat.y*gameQuat.y + gameQuat.z*gameQuat.z);
    if (mag > 0.0f) {
        float invMag = 1.0f / mag;
        gameQuat.w *= invMag;
        gameQuat.x *= invMag;
        gameQuat.y *= invMag;
        gameQuat.z *= invMag;
    }
}

// VRManager implementasyonu
void VRManager::Initialize() {
    _MESSAGE("FNVR | VRManager::Initialize");
    
    // Config dosyasını yükle
    char configPath[MAX_PATH];
    GetModuleFileNameA(NULL, configPath, MAX_PATH);
    std::string path(configPath);
    path = path.substr(0, path.find_last_of("\\/")) + "\\Data\\NVSE\\Plugins\\FNVR.ini";
    
    LoadConfig(path);
    
    // Başlangıç kalibrasyonu
    isCalibrated = false;
}

void VRManager::Shutdown() {
    _MESSAGE("FNVR | VRManager::Shutdown");
}

void VRManager::LoadConfig(const std::string& path) {
    _MESSAGE("FNVR | Loading config from: %s", path.c_str());
    
    // INI dosyasından okuma (basitleştirilmiş)
    auto readFloat = [&](const char* section, const char* key, float defaultValue) -> float {
        char buffer[32];
        GetPrivateProfileStringA(section, key, std::to_string(defaultValue).c_str(), buffer, 32, path.c_str());
        return std::stof(buffer);
    };
    
    // Pozisyon ayarları
    config.rightPosition.scale = readFloat("RightController", "PositionScale", 70.0f);
    config.rightPosition.offsetX = readFloat("RightController", "PositionOffsetX", 0.0f);
    config.rightPosition.offsetY = readFloat("RightController", "PositionOffsetY", 0.0f);
    config.rightPosition.offsetZ = readFloat("RightController", "PositionOffsetZ", 0.0f);
    
    // Silah ayarları
    config.weapon.gripOffsetX = readFloat("Weapon", "GripOffsetX", 0.0f);
    config.weapon.gripOffsetY = readFloat("Weapon", "GripOffsetY", -2.0f);
    config.weapon.gripOffsetZ = readFloat("Weapon", "GripOffsetZ", 5.0f);
    config.weapon.gripPitch = readFloat("Weapon", "GripPitch", -15.0f);
    config.weapon.gripYaw = readFloat("Weapon", "GripYaw", 0.0f);
    config.weapon.gripRoll = readFloat("Weapon", "GripRoll", 0.0f);
    
    // ADS ayarları
    config.weapon.adsOffsetX = readFloat("Weapon", "ADSOffsetX", 0.0f);
    config.weapon.adsOffsetY = readFloat("Weapon", "ADSOffsetY", -1.0f);
    config.weapon.adsOffsetZ = readFloat("Weapon", "ADSOffsetZ", -3.0f);
    
    _MESSAGE("FNVR | Config loaded successfully");
}

void VRManager::SaveConfig(const std::string& path) {
    // INI dosyasına yazma
    auto writeFloat = [&](const char* section, const char* key, float value) {
        WritePrivateProfileStringA(section, key, std::to_string(value).c_str(), path.c_str());
    };
    
    // Ayarları kaydet
    writeFloat("RightController", "PositionScale", config.rightPosition.scale);
    writeFloat("RightController", "PositionOffsetX", config.rightPosition.offsetX);
    writeFloat("RightController", "PositionOffsetY", config.rightPosition.offsetY);
    writeFloat("RightController", "PositionOffsetZ", config.rightPosition.offsetZ);
    
    writeFloat("Weapon", "GripOffsetX", config.weapon.gripOffsetX);
    writeFloat("Weapon", "GripOffsetY", config.weapon.gripOffsetY);
    writeFloat("Weapon", "GripOffsetZ", config.weapon.gripOffsetZ);
    writeFloat("Weapon", "GripPitch", config.weapon.gripPitch);
    writeFloat("Weapon", "GripYaw", config.weapon.gripYaw);
    writeFloat("Weapon", "GripRoll", config.weapon.gripRoll);
}

void VRManager::Update(const VRDataPacket& packet) {
    // Kalibrasyon kontrolü
    if (!isCalibrated) {
        _MESSAGE("FNVR | Warning: VR system not calibrated");
        return;
    }
    
    // HMD verilerini dönüştür
    HmdVector3_t hmdPosVR = {packet.hmd.px, packet.hmd.py, packet.hmd.pz};
    HmdVector3_t hmdPosGame;
    ConvertOpenVRToGamebryo(hmdPosVR, hmdPosGame);
    
    // Controller verilerini dönüştür
    HmdVector3_t rightPosVR = {packet.rightController.px, packet.rightController.py, packet.rightController.pz};
    HmdVector3_t rightPosGame;
    ConvertOpenVRToGamebryo(rightPosVR, rightPosGame);
    
    // Relative pozisyon hesapla (Bethesda tarzı)
    HmdVector3_t relativePos;
    relativePos.v[0] = (rightPosGame.v[0] - hmdPosGame.v[0]) * config.rightPosition.scale + config.rightPosition.offsetX;
    relativePos.v[1] = (rightPosGame.v[1] - hmdPosGame.v[1]) * config.rightPosition.scale + config.rightPosition.offsetY;
    relativePos.v[2] = (rightPosGame.v[2] - hmdPosGame.v[2]) * config.rightPosition.scale + config.rightPosition.offsetZ;
    
    // Silah transformunu hesapla
    HmdMatrix34_t weaponTransform = GetWeaponTransform(false);
    
    // IK hesaplaması (opsiyonel)
    if (config.useHandToWeaponIK) {
        // Omuz pozisyonunu tahmin et
        HmdVector3_t shoulderPos;
        shoulderPos.v[0] = hmdPosGame.v[0] + 15.0f;  // Sağ omuz offset
        shoulderPos.v[1] = hmdPosGame.v[1] - 5.0f;
        shoulderPos.v[2] = hmdPosGame.v[2] - 20.0f;
        
        IKBone upperArm, forearm;
        CalculateArmIK(shoulderPos, rightPosGame, upperArm, forearm);
    }
    
    // Global değişkenleri güncelle
    SAFE_SET_VALUE(TESGlobals::FNVRRightX, relativePos.v[0]);
    SAFE_SET_VALUE(TESGlobals::FNVRRightY, relativePos.v[1]);
    SAFE_SET_VALUE(TESGlobals::FNVRRightZ, relativePos.v[2]);
    
    // Rotasyon verilerini güncelle
    float pitch, yaw, roll;
    TESGlobals::QuaternionToEuler(packet.rightController.qw, packet.rightController.qx, packet.rightController.qy, packet.rightController.qz, pitch, yaw, roll);
    
    // Silah rotasyonunu uygula
    pitch += config.weapon.gripPitch;
    yaw += config.weapon.gripYaw;
    roll += config.weapon.gripRoll;
    
    SAFE_SET_VALUE(TESGlobals::FNVRRightPitch, pitch);
    SAFE_SET_VALUE(TESGlobals::FNVRRightYaw, yaw);
    SAFE_SET_VALUE(TESGlobals::FNVRRightRoll, roll);
}

HmdMatrix34_t VRManager::GetWeaponTransform(bool isAiming) {
    HmdMatrix34_t transform;
    
    // Identity matrix başlat
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) {
            transform.m[i][j] = (i == j) ? 1.0f : 0.0f;
        }
    }
    
    // Silah offset'lerini uygula
    if (isAiming) {
        transform.m[0][3] = config.weapon.adsOffsetX;
        transform.m[1][3] = config.weapon.adsOffsetY;
        transform.m[2][3] = config.weapon.adsOffsetZ;
    } else {
        transform.m[0][3] = config.weapon.gripOffsetX;
        transform.m[1][3] = config.weapon.gripOffsetY;
        transform.m[2][3] = config.weapon.gripOffsetZ;
    }
    
    // Rotasyon uygula
    HmdQuaternionf_t rotation;
    float pitchRad = config.weapon.gripPitch * DEG2RAD;
    float yawRad = config.weapon.gripYaw * DEG2RAD;
    float rollRad = config.weapon.gripRoll * DEG2RAD;
    
    // Euler'dan quaternion'a dönüşüm
    float cy = cosf(yawRad * 0.5f);
    float sy = sinf(yawRad * 0.5f);
    float cp = cosf(pitchRad * 0.5f);
    float sp = sinf(pitchRad * 0.5f);
    float cr = cosf(rollRad * 0.5f);
    float sr = sinf(rollRad * 0.5f);
    
    rotation.w = cr * cp * cy + sr * sp * sy;
    rotation.x = sr * cp * cy - cr * sp * sy;
    rotation.y = cr * sp * cy + sr * cp * sy;
    rotation.z = cr * cp * sy - sr * sp * cy;
    
    HmdMatrix34_t rotMatrix = QuaternionToMatrix(rotation);
    transform = MultiplyMatrices(transform, rotMatrix);
    
    return transform;
}

void VRManager::CalculateArmIK(const HmdVector3_t& shoulderPos, const HmdVector3_t& handPos, 
                                IKBone& upperArm, IKBone& forearm) {
    // Basit 2-bone IK hesaplaması (VRIK tarzı)
    float upperArmLength = config.armLength * 0.45f;  // Üst kol
    float forearmLength = config.armLength * 0.55f;   // Alt kol
    
    // Omuz-el mesafesi
    float dx = handPos.v[0] - shoulderPos.v[0];
    float dy = handPos.v[1] - shoulderPos.v[1];
    float dz = handPos.v[2] - shoulderPos.v[2];
    float distance = sqrtf(dx*dx + dy*dy + dz*dz);
    
    // Maksimum erişim kontrolü
    float maxReach = upperArmLength + forearmLength;
    if (distance > maxReach) {
        distance = maxReach * 0.99f;  // Biraz kısalt
    }
    
    // IK açıları hesapla (law of cosines)
    float a = upperArmLength;
    float b = forearmLength;
    float c = distance;
    
    float angleA = acosf((b*b + c*c - a*a) / (2.0f * b * c));
    float angleB = acosf((a*a + c*c - b*b) / (2.0f * a * c));
    
    // Pozisyonları ayarla
    upperArm.position = shoulderPos;
    upperArm.length = upperArmLength;
    
    // Dirsek pozisyonu
    float elbowBend = 0.3f;  // Doğal dirsek bükülmesi
    HmdVector3_t elbowPos;
    elbowPos.v[0] = shoulderPos.v[0] + (dx * upperArmLength / distance);
    elbowPos.v[1] = shoulderPos.v[1] + (dy * upperArmLength / distance) - elbowBend;
    elbowPos.v[2] = shoulderPos.v[2] + (dz * upperArmLength / distance);
    
    forearm.position = elbowPos;
    forearm.length = forearmLength;
}

// Kalibrasyon fonksiyonları
void VRManager::StartCalibration() {
    _MESSAGE("FNVR | Starting VR calibration");
    isCalibrated = false;
    
    // Kalibrasyon verilerini sıfırla
    calibration.standingHeight = 0.0f;
    calibration.armSpan = 0.0f;
}

void VRManager::UpdateCalibration(const VRDataPacket& packet) {
    // HMD yüksekliğini kaydet
    if (calibration.standingHeight == 0.0f) {
        calibration.standingHeight = packet.hmd.py;
        _MESSAGE("FNVR | Calibration: Standing height = %.2f meters", calibration.standingHeight);
    }
    
    // Controller pozisyonlarını kaydet
    calibration.hmdCalibrationPose.m[0][3] = packet.hmd.px;
    calibration.hmdCalibrationPose.m[1][3] = packet.hmd.py;
    calibration.hmdCalibrationPose.m[2][3] = packet.hmd.pz;
    
    calibration.rightCalibrationPose.m[0][3] = packet.rightController.px;
    calibration.rightCalibrationPose.m[1][3] = packet.rightController.py;
    calibration.rightCalibrationPose.m[2][3] = packet.rightController.pz;
}

void VRManager::FinishCalibration() {
    // Oyuncu boyunu hesapla
    config.playerHeight = calibration.standingHeight;
    
    // Kol uzunluğunu tahmin et (boy bazlı)
    config.armLength = config.playerHeight * 0.37f;  // Ortalama oran
    
    isCalibrated = true;
    _MESSAGE("FNVR | Calibration complete. Player height: %.2fm, Arm length: %.2fm", 
             config.playerHeight, config.armLength);
    
    // Config'i kaydet
    char configPath[MAX_PATH];
    GetModuleFileNameA(NULL, configPath, MAX_PATH);
    std::string path(configPath);
    path = path.substr(0, path.find_last_of("\\/")) + "\\Data\\NVSE\\Plugins\\FNVR.ini";
    SaveConfig(path);
}

} // namespace FNVR 