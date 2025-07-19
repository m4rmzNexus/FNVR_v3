#pragma once

// fnvr_pose_pipe.py ile uyumlu veri paketi yapısı
// Python: '<II4f3f4f3f3fd' = 88 bytes
// ÖNEMLI: Sıralama Python'daki struct.pack sırasıyla aynı olmalı!
#pragma pack(push, 1)
struct VRDataPacketV2
{
    // Header - Python format: II (2 unsigned ints)
    unsigned int version;                  // 4 bytes
    unsigned int flags;                    // 4 bytes
    
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
    
    // Total: 4 + 4 + 16 + 12 + 16 + 12 + 12 + 8 = 88 bytes
};
#pragma pack(pop)

// Simple flat structure for game usage
struct VRDataPacketFlat
{
    // Version and flags
    unsigned int version;
    unsigned int flags;
    
    // HMD data
    float hmd_px, hmd_py, hmd_pz;
    float hmd_qw, hmd_qx, hmd_qy, hmd_qz;
    
    // Right controller data
    float right_px, right_py, right_pz;
    float right_qw, right_qx, right_qy, right_qz;
    
    // Left controller data (mirrored for now)
    float left_px, left_py, left_pz;
    float left_qw, left_qx, left_qy, left_qz;
    
    // Controller inputs (zeroed for now)
    float right_trigger, right_grip;
    float left_trigger, left_grip;
    float right_menu, right_system;
    float left_menu, left_system;
    float right_pad_x, right_pad_y;
    float left_pad_x, left_pad_y;
    float a_button, b_button, x_button, y_button;
    
    // Timestamp
    double timestamp;
};

// Convert from wire format (V2) to flat game format
inline void ConvertV2ToFlat(const VRDataPacketV2& v2, VRDataPacketFlat& flat)
{
    // Copy version and flags
    flat.version = v2.version;
    flat.flags = v2.flags;
    
    // HMD data
    flat.hmd_px = v2.hmd_px;
    flat.hmd_py = v2.hmd_py;
    flat.hmd_pz = v2.hmd_pz;
    flat.hmd_qw = v2.hmd_qw;
    flat.hmd_qx = v2.hmd_qx;
    flat.hmd_qy = v2.hmd_qy;
    flat.hmd_qz = v2.hmd_qz;
    
    // Right controller data
    flat.right_px = v2.ctl_px;
    flat.right_py = v2.ctl_py;
    flat.right_pz = v2.ctl_pz;
    flat.right_qw = v2.ctl_qw;
    flat.right_qx = v2.ctl_qx;
    flat.right_qy = v2.ctl_qy;
    flat.right_qz = v2.ctl_qz;
    
    // Left controller - mirror from right for now
    flat.left_px = -v2.ctl_px;
    flat.left_py = v2.ctl_py;
    flat.left_pz = v2.ctl_pz;
    flat.left_qw = v2.ctl_qw;
    flat.left_qx = -v2.ctl_qx;
    flat.left_qy = v2.ctl_qy;
    flat.left_qz = v2.ctl_qz;
    
    // Zero out inputs for now
    flat.right_trigger = 0.0f;
    flat.right_grip = 0.0f;
    flat.left_trigger = 0.0f;
    flat.left_grip = 0.0f;
    flat.right_menu = 0.0f;
    flat.right_system = 0.0f;
    flat.left_menu = 0.0f;
    flat.left_system = 0.0f;
    flat.right_pad_x = 0.0f;
    flat.right_pad_y = 0.0f;
    flat.left_pad_x = 0.0f;
    flat.left_pad_y = 0.0f;
    flat.a_button = 0.0f;
    flat.b_button = 0.0f;
    flat.x_button = 0.0f;
    flat.y_button = 0.0f;
    
    // Copy timestamp
    flat.timestamp = v2.timestamp;
}

// For backward compatibility
typedef VRDataPacketFlat VRDataPacket;
#define ConvertV2ToFull ConvertV2ToFlat