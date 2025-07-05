#pragma once
#include "nvse/GameForms.h"
#include "nvse/GameAPI.h"

namespace FNVR {

// Enhanced Camera 1st Person Body Fix
class FirstPersonBodyFix {
public:
    static void ApplyFixes();
    
    // Skeleton görünürlük kontrolü
    static void EnsureSkeletonVisible();
    
    // Enhanced Camera uyumluluğu
    static void FixEnhancedCameraIssues();
    
    // VorpX modunda özel düzeltmeler
    static void ApplyVorpXFixes();
    
    // Visibility helper (her frame çağrılmalı)
    static void ForceVisible(NiAVObject* node);
    // Oyun döngüsünde her frame çağırılacak merkezi fonksiyon
    static void UpdateFrame();
    
private:
    // 1st person skeleton node'unu bul ve düzelt
    static void Fix1stPersonSkeleton();
    
    // Camera offset düzeltmeleri
    static void FixCameraOffsets();
    
    // Visibility flag'lerini düzelt
    static void ForceSkeletonVisibility();
};

} // namespace FNVR 