#pragma once
#include "Globals.h"

namespace FNVR {

// VorpX Uyumluluk Sistemi
class VorpXCompatibility {
public:
    // VorpX'in kendi head tracking'i olduğu için bizimkini devre dışı bırakma
    static bool IsVorpXDetected();
    
    // VorpX modunda çalışma ayarları
    struct VorpXMode {
        bool disableHeadTracking = true;      // VorpX kendi tracking'ini kullanıyor
        bool disableFOVAdjustment = true;     // VorpX FOV'u kendisi ayarlıyor
        bool useWeaponOnlyTracking = true;    // Sadece silah pozisyonunu takip et
        float positionScaleOverride = 1.0f;   // VorpX için pozisyon ölçeği
    };
    
    static VorpXMode GetVorpXSettings();
};

// 1st Person Skeleton görünürlük düzeltmesi
class FirstPersonFix {
public:
    // 1st person skeleton'ı görünür yapma
    static void EnableFirstPersonBody();
    
    // Kamera pozisyonunu düzeltme
    static void FixCameraPosition(HmdVector3_t& cameraPos);
    
    // FOV zoom düzeltmesi
    static void DisableVorpXZoom();
};

} // namespace FNVR 