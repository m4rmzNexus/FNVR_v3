#pragma once

// fnvr_pose_pipe.py ile uyumlu veri paketi yapısı
// Python: '<I4f3f4f3f3fd' = 88 bytes
// ÖNEMLI: Sıralama Python'daki struct.pack sırasıyla aynı olmalı!
struct VRDataPacketV2
{
    int version;                          // 4 bytes
    
    // HMD quaternion - Python sırası: w, x, y, z
    float hmd_qw, hmd_qx, hmd_qy, hmd_qz; // 16 bytes
    
    // HMD position - Python sırası: x, y, z
    float hmd_px, hmd_py, hmd_pz;         // 12 bytes
    
    // Controller quaternion - Python sırası: w, x, y, z
    float ctl_qw, ctl_qx, ctl_qy, ctl_qz; // 16 bytes
    
    // Controller position - Python sırası: x, y, z
    float ctl_px, ctl_py, ctl_pz;         // 12 bytes
    
    // Relative position - Python sırası: x, y, z
    float rel_px, rel_py, rel_pz;         // 12 bytes
    
    // Timestamp 
    double timestamp;                      // 8 bytes
    
    // Total: 4 + 16 + 12 + 16 + 12 + 12 + 8 = 88 bytes
};

// Eski VRDataPacket ile uyumluluk için dönüştürme fonksiyonu
inline void ConvertV2ToFull(const VRDataPacketV2& v2, struct VRDataPacket& full)
{
    // Version
    full.version = v2.version;
    
    // HMD data
    full.hmd_px = v2.hmd_px;
    full.hmd_py = v2.hmd_py;
    full.hmd_pz = v2.hmd_pz;
    full.hmd_qw = v2.hmd_qw;
    full.hmd_qx = v2.hmd_qx;
    full.hmd_qy = v2.hmd_qy;
    full.hmd_qz = v2.hmd_qz;
    
    // Right controller data
    full.right_px = v2.ctl_px;
    full.right_py = v2.ctl_py;
    full.right_pz = v2.ctl_pz;
    full.right_qw = v2.ctl_qw;
    full.right_qx = v2.ctl_qx;
    full.right_qy = v2.ctl_qy;
    full.right_qz = v2.ctl_qz;
    
    // Left controller - HMD'den mirror et
    full.left_px = -v2.ctl_px;
    full.left_py = v2.ctl_py;
    full.left_pz = v2.ctl_pz;
    full.left_qw = v2.ctl_qw;
    full.left_qx = -v2.ctl_qx;
    full.left_qy = v2.ctl_qy;
    full.left_qz = v2.ctl_qz;
    
    // Geri kalan alanları sıfırla
    full.right_trigger = 0.0f;
    full.right_grip = 0.0f;
    full.left_trigger = 0.0f;
    full.left_grip = 0.0f;
    full.right_menu = 0.0f;
    full.right_system = 0.0f;
    full.left_menu = 0.0f;
    full.left_system = 0.0f;
    full.right_pad_x = 0.0f;
    full.right_pad_y = 0.0f;
    full.left_pad_x = 0.0f;
    full.left_pad_y = 0.0f;
    full.a_button = 0.0f;
    full.b_button = 0.0f;
    full.x_button = 0.0f;
    full.y_button = 0.0f;
}