#pragma once
#include "Globals.h"
#include <vector>
#include <string>

// OpenVR tipleri için basit tanımlamalar
struct HmdVector3_t {
    float v[3];
};

struct HmdVector2_t {
    float v[2];
};

struct HmdQuaternion_t {
    float w, x, y, z;
};

// Keep alias for compatibility
typedef HmdQuaternion_t HmdQuaternionf_t;

struct HmdMatrix34_t {
    float m[3][4];
};

namespace FNVR {

// Bethesda VR tarzı yapılandırma sistemi
struct VRConfig {
    // Pozisyon ayarları
    struct PositionConfig {
        float scale = 70.0f;        // Bethesda oyunları genelde 70 birim/metre kullanır
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        float offsetZ = 0.0f;
    };

    // Rotasyon ayarları
    struct RotationConfig {
        float pitchScale = 1.0f;
        float yawScale = 1.0f;
        float rollScale = 1.0f;
        float pitchOffset = 0.0f;
        float yawOffset = 0.0f;
        float rollOffset = 0.0f;
    };

    // Silah ayarları (VRIK/FRIK tarzı)
    struct WeaponConfig {
        // Primary hand grip
        float gripOffsetX = 0.0f;
        float gripOffsetY = -2.0f;
        float gripOffsetZ = 5.0f;
        float gripPitch = -15.0f;
        float gripYaw = 0.0f;
        float gripRoll = 0.0f;

        // Two-handed offset
        float twoHandedOffsetX = -10.0f;
        float twoHandedOffsetY = 0.0f;
        float twoHandedOffsetZ = 15.0f;

        // Aim down sights
        float adsOffsetX = 0.0f;
        float adsOffsetY = -1.0f;
        float adsOffsetZ = -3.0f;
    };

    PositionConfig hmdPosition;
    RotationConfig hmdRotation;
    PositionConfig rightPosition;
    RotationConfig rightRotation;
    PositionConfig leftPosition;
    RotationConfig leftRotation;
    WeaponConfig weapon;

    // IK tarzı ayarlar
    bool useRelativePositioning = true;
    bool useHandToWeaponIK = true;
    float playerHeight = 1.75f;  // metre
    float armLength = 0.65f;     // metre
};

// Bethesda tarzı VR Manager
class VRManager {
private:
    VRConfig config;
    bool isCalibrated = false;
    
    // Kalibrasyon verileri
    struct CalibrationData {
        float standingHeight = 0.0f;
        float armSpan = 0.0f;
        HmdMatrix34_t hmdCalibrationPose;
        HmdMatrix34_t rightCalibrationPose;
        HmdMatrix34_t leftCalibrationPose;
    } calibration;

    // IK hesaplamaları için
    struct IKBone {
        HmdVector3_t position;
        HmdQuaternionf_t rotation;
        float length;
    };

public:
    static VRManager& GetSingleton() {
        static VRManager instance;
        return instance;
    }

    // Başlatma ve temizleme
    void Initialize();
    void Shutdown();

    // Kalibrasyon (VRIK tarzı)
    void StartCalibration();
    void UpdateCalibration(const VRDataPacket& packet);
    void FinishCalibration();
    bool IsCalibrated() const { return isCalibrated; }

    // Ana güncelleme
    void Update(const VRDataPacket& packet);

    // Config yönetimi
    void LoadConfig(const std::string& path);
    void SaveConfig(const std::string& path);
    VRConfig& GetConfig() { return config; }

    // Bethesda tarzı transform fonksiyonları
    HmdMatrix34_t GetWeaponTransform(bool isAiming = false);
    HmdMatrix34_t GetHandTransform(bool isRightHand = true);
    
    // IK hesaplamaları
    void CalculateArmIK(const HmdVector3_t& shoulderPos, const HmdVector3_t& handPos, 
                        IKBone& upperArm, IKBone& forearm);

private:
    VRManager() = default;
    ~VRManager() = default;

    // Yardımcı fonksiyonlar
    HmdVector3_t TransformPoint(const HmdVector3_t& point, const HmdMatrix34_t& matrix);
    HmdQuaternionf_t MatrixToQuaternion(const HmdMatrix34_t& matrix);
    HmdMatrix34_t QuaternionToMatrix(const HmdQuaternionf_t& quat);
    HmdMatrix34_t MultiplyMatrices(const HmdMatrix34_t& a, const HmdMatrix34_t& b);
    
    // Coordinate system conversions (OpenVR <-> Gamebryo)
    void ConvertOpenVRToGamebryo(const HmdVector3_t& vrPos, HmdVector3_t& gamePos, float scale = 70.0f);
    void ConvertGamebryoToOpenVR(const HmdVector3_t& gamePos, HmdVector3_t& vrPos, float scale = 70.0f);
    void ConvertOpenVRQuaternionToGamebryo(const HmdQuaternion_t& vrQuat, HmdQuaternion_t& gameQuat);
};

// Bethesda tarzı VR Input sistemi
class VRInput {
public:
    enum class Gesture {
        None,
        Grip,
        Point,
        Fist,
        OpenHand,
        ThumbsUp
    };

    struct ControllerState {
        bool triggerPressed = false;
        bool gripPressed = false;
        bool menuPressed = false;
        bool touchpadPressed = false;
        float triggerValue = 0.0f;
        float gripValue = 0.0f;
        HmdVector2_t touchpadPos = {0.0f, 0.0f};
        Gesture currentGesture = Gesture::None;
    };

    static VRInput& GetSingleton() {
        static VRInput instance;
        return instance;
    }

    void Update();
    const ControllerState& GetRightController() const { return rightController; }
    const ControllerState& GetLeftController() const { return leftController; }
    
    // Gesture tanıma (VRIK tarzı)
    Gesture DetectGesture(bool isRightHand);

private:
    VRInput() = default;
    ControllerState rightController;
    ControllerState leftController;
};

} // namespace FNVR 