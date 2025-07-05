#pragma once

// New modern packet structure matching fnvr_pose_pipe.py
struct VRDataPacket
{
    int version;                        // Version field
    
    float hmd_px, hmd_py, hmd_pz;       // HMD position
    float hmd_qw, hmd_qx, hmd_qy, hmd_qz; // HMD rotation (quaternion)
    
    float right_px, right_py, right_pz; // Right controller position
    float right_qw, right_qx, right_qy, right_qz; // Right controller rotation
    
    float left_px, left_py, left_pz;   // Left controller position
    float left_qw, left_qx, left_qy, left_qz; // Left controller rotation
    
    // Controller inputs
    float right_trigger, right_grip;
    float left_trigger, left_grip;
    
    // Buttons (0 or 1)
    float right_menu, right_system;
    float left_menu, left_system;
    
    // Thumbstick/touchpad
    float right_pad_x, right_pad_y;
    float left_pad_x, left_pad_y;
    
    // Button states
    float a_button, b_button;
    float x_button, y_button;
}; 