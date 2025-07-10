#pragma once
#include <cstdint>

// Tek, birleşik veri paketi yapısı
#pragma pack(push, 1)
struct VRDataPacket
{
    // Header
    uint32_t version;           // Protocol version (2 for current)
    uint32_t flags;            // Bit flags for available data
    
    // HMD Data - Always present
    struct {
        float qw, qx, qy, qz;  // Quaternion (matches Python order)
        float px, py, pz;      // Position
    } hmd;
    
    // Primary Controller (Right) - Always present
    struct {
        float qw, qx, qy, qz;  // Quaternion
        float px, py, pz;      // Position
    } rightController;
    
    // Relative position data
    float rel_px, rel_py, rel_pz;  // Controller relative to HMD
    
    // Timestamp
    double timestamp;
    
    // Extended data (only if flags indicate)
    struct {
        // Secondary Controller (Left)
        float left_qw, left_qx, left_qy, left_qz;
        float left_px, left_py, left_pz;
        
        // Controller Inputs
        float right_trigger, right_grip;
        float left_trigger, left_grip;
        
        // Buttons
        uint32_t buttonStates;  // Bit field for all buttons
        
        // Touchpad/Thumbstick
        float right_pad_x, right_pad_y;
        float left_pad_x, left_pad_y;
    } extended;
};
#pragma pack(pop)

// Flags for available data
enum VRDataFlags : uint32_t {
    VR_FLAG_BASIC_DATA = 0x01,      // HMD + Right controller
    VR_FLAG_LEFT_CONTROLLER = 0x02, // Left controller data present
    VR_FLAG_INPUTS = 0x04,          // Button/trigger data present
    VR_FLAG_TOUCHPAD = 0x08,        // Touchpad data present
};

// Helper to check packet size based on flags
inline size_t GetPacketSize(uint32_t flags) {
    size_t size = offsetof(VRDataPacket, extended);  // Base size
    
    if (flags & VR_FLAG_LEFT_CONTROLLER)
        size += sizeof(float) * 7;  // left controller data
    if (flags & VR_FLAG_INPUTS)
        size += sizeof(float) * 4 + sizeof(uint32_t);  // triggers + buttons
    if (flags & VR_FLAG_TOUCHPAD)
        size += sizeof(float) * 4;  // touchpad data
        
    return size;
} 