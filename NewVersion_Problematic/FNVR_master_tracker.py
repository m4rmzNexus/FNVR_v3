#!/usr/bin/env python3
"""
FNVR Master Tracker
Combines the configurability of the GUI-generated tracker with the
gesture controls and optional scaling from the ALPHA tracker.
"""

import time
import ctypes
import struct
import openvr
import numpy as np
import json
import keyboard
import math
import os
import sys

# --- Helper Functions ---

def load_settings():
    """Loads settings from fnvr_settings.json"""
    settings_path = os.path.join(os.path.dirname(__file__), 'fnvr_settings.json')
    try:
        with open(settings_path, 'r') as f:
            return json.load(f)
    except FileNotFoundError:
        print(f"FATAL: {settings_path} not found. Please run the GUI first.")
        exit()
    except json.JSONDecodeError:
        print(f"FATAL: {settings_path} is corrupted. Please fix or delete it and run the GUI again.")
        exit()

def get_rotation(matrix):
    """Extract quaternion from rotation matrix"""
    q = openvr.HmdQuaternion_t()
    q.w = np.sqrt(max(0, 1 + matrix[0][0] + matrix[1][1] + matrix[2][2])) / 2
    q.x = np.sqrt(max(0, 1 + matrix[0][0] - matrix[1][1] - matrix[2][2])) / 2
    q.y = np.sqrt(max(0, 1 - matrix[0][0] + matrix[1][1] - matrix[2][2])) / 2
    q.z = np.sqrt(max(0, 1 - matrix[0][0] - matrix[1][1] + matrix[2][2])) / 2
    q.x = np.copysign(q.x, matrix[2][1] - matrix[1][2])
    q.y = np.copysign(q.y, matrix[0][2] - matrix[2][0])
    q.z = np.copysign(q.z, matrix[1][0] - matrix[0][1])
    return q

def get_position(matrix):
    """Extract position from transformation matrix"""
    v = openvr.HmdVector3_t()
    v.v = (ctypes.c_float * 3)(matrix[0][3], matrix[1][3], matrix[2][3])
    return v

def quaternion_conjugate(q):
    return openvr.HmdQuaternion_t(q.w, -q.x, -q.y, -q.z)

def quaternion_multiply(q1, q2):
    w = q1.w * q2.w - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z
    x = q1.w * q2.x + q1.x * q2.w + q1.y * q2.z - q1.z * q2.y
    y = q1.w * q2.y - q1.x * q2.z + q1.y * q2.w + q1.z * q2.x
    z = q1.w * q2.z + q1.x * q2.y - q1.y * q2.x + q1.z * q2.w
    return openvr.HmdQuaternion_t(w, x, y, z)

def rotate_vector_by_quaternion(vector, quaternion):
    pure_q_vector = openvr.HmdQuaternion_t(0, vector.v[0], vector.v[1], vector.v[2])
    quaternion_inverse = quaternion_conjugate(quaternion)
    rotated_q = quaternion_multiply(quaternion, quaternion_multiply(pure_q_vector, quaternion_inverse))
    rotated_vector = openvr.HmdVector3_t()
    rotated_vector.v = (ctypes.c_float * 3)(rotated_q.x, rotated_q.y, rotated_q.z)
    return rotated_vector

def quaternion_to_euler(q):
    """Convert quaternion (w, x, y, z) to Euler angles for New Vegas (X=pitch, Y=roll, Z=yaw) in degrees."""
    # Roll (x-axis rotation)
    sinr_cosp = 2 * (q.w * q.x + q.y * q.z)
    cosr_cosp = 1 - 2 * (q.x * q.x + q.y * q.y)
    roll = math.atan2(sinr_cosp, cosr_cosp)

    # Pitch (y-axis rotation)
    sinp = 2 * (q.w * q.y - q.z * q.x)
    if abs(sinp) >= 1:
        pitch = math.copysign(math.pi / 2, sinp)  # use 90 degrees if out of range
    else:
        pitch = math.asin(sinp)

    # Yaw (z-axis rotation)
    siny_cosp = 2 * (q.w * q.z + q.x * q.y)
    cosy_cosp = 1 - 2 * (q.y * q.y + q.z * q.z)
    yaw = math.atan2(siny_cosp, cosy_cosp)

    # Convert radians to degrees (57.2957795131 = 180/pi)
    pitch_deg = pitch * 57.2957795131
    yaw_deg = yaw * 57.2957795131
    roll_deg = roll * 57.2957795131

    # Return in New Vegas order: X=pitch, Y=roll, Z=yaw
    return pitch_deg, roll_deg, yaw_deg

def calculate_distance_xyz(x1, y1, z1, x2, y2, z2):
    """Calculates Euclidean distance between two 3D points."""
    return math.sqrt((x2 - x1)**2 + (y2 - y1)**2 + (z2 - z1)**2)

def apply_deadzone(value, deadzone):
    """Apply deadzone to a value - returns 0 if within deadzone"""
    if abs(value) < deadzone:
        return 0.0
    return value

def apply_max_reach(value, max_reach):
    """Clamp value to max reach limits"""
    return max(min(value, max_reach), -max_reach)

def apply_smoothing(current, target, factor):
    """Apply exponential smoothing between current and target value"""
    if factor <= 0:
        return target
    return current + (target - current) * (1.0 - factor)

# --- Communication Class ---

class NamedPipeServer:
    def __init__(self, name=r"\\.\pipe\FNVRTracker"):
        self.name = name
        self.kernel32 = ctypes.windll.kernel32
        self.handle = None
        
    def create_and_wait(self):
        self.handle = self.kernel32.CreateNamedPipeW(
            self.name, 0x00000002, 0, 255, 512, 512, 0, None
        )
        if self.handle == -1:
            print(f"Failed to create pipe: {self.kernel32.GetLastError()}")
            return False
            
        print("Waiting for game connection...")
        if not self.kernel32.ConnectNamedPipe(self.handle, None):
            error = self.kernel32.GetLastError()
            if error != 535: # ERROR_PIPE_CONNECTED
                print(f"Connect failed: {error}")
                self.kernel32.CloseHandle(self.handle)
                self.handle = None
                return False
        
        print("Game connected!")
        return True
        
    def send(self, data):
        if not self.handle: return False
        bytes_written = ctypes.c_ulong()
        success = self.kernel32.WriteFile(
            self.handle, data, len(data),
            ctypes.byref(bytes_written), None
        )
        return success

    def disconnect(self):
        if self.handle:
            self.kernel32.CloseHandle(self.handle)
            self.handle = None

# --- Main Application ---

def main():
    settings = load_settings()

    # Initialize OpenVR
    try:
        vr = openvr.init(openvr.VRApplication_Background)
    except openvr.OpenVRError as e:
        print(f"Error initializing OpenVR: {e}")
        return
        
    pipe = NamedPipeServer()
    if not pipe.create_and_wait():
        openvr.shutdown()
        return
        
    print("\n--- FNVR Master Tracker Running ---")
    print(f"Gesture Controls: {'Enabled' if settings.get('enable_gestures', True) else 'Disabled'}")
    print(f"Scaling Mode: {'ALPHA' if settings.get('use_alpha_scaling', False) else 'GUI'}")
    print(f"HMD Tracking: {'Enabled' if settings.get('enable_hmd_position_tracking', False) else 'Disabled'}")
    print("-" * 35)
    
    # Find devices
    hmd_index = openvr.k_unTrackedDeviceIndex_Hmd
    right_controller_index = -1
    
    # Smoothing variables for controller
    smooth_con_pos = [0, 0, 0]
    smooth_con_rot = [0, 0]
    
    # Smoothing variables for HMD
    smooth_hmd_pos = [0, 0, 0]
    smooth_hmd_rot = [0, 0, 0]

    last_gesture_time = 0
    
    while True:
        if right_controller_index == -1:
            for i in range(openvr.k_unMaxTrackedDeviceCount):
                if vr.getTrackedDeviceClass(i) == openvr.TrackedDeviceClass_Controller and \
                   vr.getControllerRoleForTrackedDeviceIndex(i) == openvr.TrackedControllerRole_RightHand:
                    right_controller_index = i
                    print(f"Found right controller at index {i}")
                    break
            if right_controller_index == -1:
                print("Right controller not found, retrying...")
                time.sleep(1)
                continue
            
        poses = vr.getDeviceToAbsoluteTrackingPose(
            openvr.TrackingUniverseStanding, 0.0, openvr.k_unMaxTrackedDeviceCount
        )
        
        hmd_pose = poses[hmd_index]
        controller_pose = poses[right_controller_index]
            
        if hmd_pose.bPoseIsValid and controller_pose.bPoseIsValid:
            # --- Core VR Math ---
            hmd_m = hmd_pose.mDeviceToAbsoluteTracking
            con_m = controller_pose.mDeviceToAbsoluteTracking
            
            hmd_pos = get_position(hmd_m)
            con_pos = get_position(con_m)

            # Apply controller offset from settings
            con_pos.v[0] += settings.get('controller_offset_x', 0)
            con_pos.v[1] += settings.get('controller_offset_y', 0)
            con_pos.v[2] += settings.get('controller_offset_z', 0)
            
            hmd_rot = get_rotation(hmd_m)
            con_rot = get_rotation(con_m)
            
            world_diff = openvr.HmdVector3_t()
            world_diff.v[0] = con_pos.v[0] - hmd_pos.v[0]
            world_diff.v[1] = con_pos.v[1] - hmd_pos.v[1]
            world_diff.v[2] = con_pos.v[2] - hmd_pos.v[2]
            
            hmd_rotation_inverse = quaternion_conjugate(hmd_rot)
            relative_position = rotate_vector_by_quaternion(world_diff, hmd_rotation_inverse)
            relative_rotation = quaternion_multiply(hmd_rotation_inverse, con_rot)
            
            inertiaZ, inertiaX, inertiaY = relative_position.v
            inertiaZr, inertiaXr, inertiaYr = relative_rotation.x, relative_rotation.y, relative_rotation.z

            # --- Gesture Control (from ALPHA) ---
            current_time = time.time()
            if settings.get('enable_gestures', False) and (current_time - last_gesture_time > 1.0):
                # PipBoy Gesture
                if settings.get('pipboy_gesture_enabled', False):
                    coords = settings.get('pipboy_gesture_coords', [0,0,0])
                    radius = settings.get('gesture_activation_radius', 0.1)
                    dist = calculate_distance_xyz(inertiaX, inertiaY, inertiaZ, coords[0], coords[1], coords[2])
                    if dist < radius:
                        print("Pip-Boy gesture activated!")
                        keyboard.press_and_release('Tab')
                        last_gesture_time = current_time
                        time.sleep(0.5) # Prevent immediate re-trigger
                
                # Pause Gesture
                if settings.get('pause_gesture_enabled', False):
                    coords = settings.get('pause_gesture_coords', [0,0,0])
                    radius = settings.get('gesture_activation_radius', 0.1)
                    dist = calculate_distance_xyz(inertiaX, inertiaY, inertiaZ, coords[0], coords[1], coords[2])
                    if dist < radius:
                        print("Pause gesture activated!")
                        keyboard.press_and_release('Escape')
                        last_gesture_time = current_time
                        time.sleep(0.5) # Prevent immediate re-trigger

            # --- Scaling and Final Calculations ---
            if settings.get('use_alpha_scaling', False):
                # Use ALPHA tracker's original hardcoded scaling
                scaled_X = (inertiaX * 50) + 15
                scaled_Y = (inertiaY * -50) + -10
                scaled_Z = (inertiaZ * -50) + 0
                scaled_Xr = (inertiaXr * -120) + 10
                scaled_Zr = (inertiaZr * 120) + -75
            else:
                # Use GUI settings for scaling and inversion
                scaled_X = (inertiaX * settings['position_scale']) + settings['camera_offset_x']
                scaled_Y = (inertiaY * (-settings['position_scale'] if settings['invert_y'] else settings['position_scale'])) + settings['camera_offset_y']
                scaled_Z = (inertiaZ * (-settings['position_scale'] if settings['invert_z'] else settings['position_scale'])) + settings['camera_offset_z']
                
                scaled_Xr = (inertiaXr * (-settings['rotation_scale'] if settings['invert_pitch'] else settings['rotation_scale'])) + settings['rotation_offset_x']
                scaled_Zr = (inertiaZr * (settings['rotation_scale'] if settings['invert_roll'] else -settings['rotation_scale'])) + settings['rotation_offset_z']

            # --- Apply Filters to Controller ---
            # Apply deadzones
            if settings.get('apply_deadzone', False):
                deadzone_pos = settings.get('deadzone_position', 0.05)
                deadzone_rot = settings.get('deadzone_rotation', 0.05)
                
                scaled_X = apply_deadzone(scaled_X, deadzone_pos)
                scaled_Y = apply_deadzone(scaled_Y, deadzone_pos)
                scaled_Z = apply_deadzone(scaled_Z, deadzone_pos)
                scaled_Xr = apply_deadzone(scaled_Xr, deadzone_rot)
                scaled_Zr = apply_deadzone(scaled_Zr, deadzone_rot)
            
            # Apply max reach
            if settings.get('apply_max_reach', False):
                max_reach_pos = settings.get('max_reach_position', 50.0)
                max_reach_rot = settings.get('max_reach_rotation', 90.0)
                
                scaled_X = apply_max_reach(scaled_X, max_reach_pos)
                scaled_Y = apply_max_reach(scaled_Y, max_reach_pos)
                scaled_Z = apply_max_reach(scaled_Z, max_reach_pos)
                scaled_Xr = apply_max_reach(scaled_Xr, max_reach_rot)
                scaled_Zr = apply_max_reach(scaled_Zr, max_reach_rot)
            
            # Apply smoothing
            if settings.get('apply_smoothing', False):
                smooth_factor_pos = settings.get('smoothing_factor_position', 0.5)
                smooth_factor_rot = settings.get('smoothing_factor_rotation', 0.5)
                
                smooth_con_pos[0] = apply_smoothing(smooth_con_pos[0], scaled_X, smooth_factor_pos)
                smooth_con_pos[1] = apply_smoothing(smooth_con_pos[1], scaled_Y, smooth_factor_pos)
                smooth_con_pos[2] = apply_smoothing(smooth_con_pos[2], scaled_Z, smooth_factor_pos)
                smooth_con_rot[0] = apply_smoothing(smooth_con_rot[0], scaled_Xr, smooth_factor_rot)
                smooth_con_rot[1] = apply_smoothing(smooth_con_rot[1], scaled_Zr, smooth_factor_rot)
                
                scaled_X, scaled_Y, scaled_Z = smooth_con_pos
                scaled_Xr, scaled_Zr = smooth_con_rot[0], smooth_con_rot[1]
            
            # --- Player Yaw Calculation ---
            # Use HMD yaw for player body rotation if HMD tracking is enabled
            player_yaw = 0.0
            if settings.get('enable_hmd_position_tracking', False) and settings.get('use_hmd_yaw_for_player', True):
                _, _, player_yaw = quaternion_to_euler(hmd_rot)
                player_yaw *= settings.get('player_yaw_scale', 1.0)

            # --- HMD Data Calculation ---
            hmd_data = [0.0] * 6
            if settings.get('enable_hmd_position_tracking', False):
                hmd_pitch, hmd_roll, hmd_yaw = quaternion_to_euler(hmd_rot)
                
                # Apply position offsets
                hmd_x = hmd_pos.v[0] * settings.get('hmd_position_scale', 100.0) + settings.get('hmd_offset_x', 0)
                hmd_y = hmd_pos.v[1] * settings.get('hmd_position_scale', 100.0) + settings.get('hmd_offset_y', 0)
                hmd_z = hmd_pos.v[2] * settings.get('hmd_position_scale', 100.0) + settings.get('hmd_offset_z', 0)
                
                # Apply rotation offsets (in degrees)
                hmd_pitch += settings.get('hmd_pitch_offset', 0)
                hmd_yaw += settings.get('hmd_yaw_offset', 0)
                hmd_roll += settings.get('hmd_roll_offset', 0)
                
                # Apply deadzones if enabled
                if settings.get('apply_hmd_deadzone', False):
                    deadzone = settings.get('hmd_deadzone', 0.05)
                    hmd_x = apply_deadzone(hmd_x, deadzone)
                    hmd_y = apply_deadzone(hmd_y, deadzone)
                    hmd_z = apply_deadzone(hmd_z, deadzone)
                
                # Apply max reach if enabled
                if settings.get('apply_hmd_max_reach', False):
                    max_reach = settings.get('hmd_max_reach', 50.0)
                    hmd_x = apply_max_reach(hmd_x, max_reach)
                    hmd_y = apply_max_reach(hmd_y, max_reach)
                    hmd_z = apply_max_reach(hmd_z, max_reach)
                
                # Apply smoothing if enabled
                if settings.get('apply_hmd_smoothing', False):
                    smooth_factor = settings.get('hmd_smoothing_factor', 0.5)
                    smooth_hmd_pos[0] = apply_smoothing(smooth_hmd_pos[0], hmd_x, smooth_factor)
                    smooth_hmd_pos[1] = apply_smoothing(smooth_hmd_pos[1], hmd_y, smooth_factor)
                    smooth_hmd_pos[2] = apply_smoothing(smooth_hmd_pos[2], hmd_z, smooth_factor)
                    smooth_hmd_rot[0] = apply_smoothing(smooth_hmd_rot[0], hmd_pitch, smooth_factor)
                    smooth_hmd_rot[1] = apply_smoothing(smooth_hmd_rot[1], hmd_yaw, smooth_factor)
                    smooth_hmd_rot[2] = apply_smoothing(smooth_hmd_rot[2], hmd_roll, smooth_factor)
                    
                    hmd_x, hmd_y, hmd_z = smooth_hmd_pos
                    hmd_pitch, hmd_yaw, hmd_roll = smooth_hmd_rot
                
                # Pack data in NVSE expected format: X, Y, Z, pitch, yaw, roll
                hmd_data = [
                    hmd_x, hmd_y, hmd_z,
                    hmd_pitch, hmd_yaw, hmd_roll
                ]

            # --- Data Packet Assembly and Transmission ---
            packet = struct.pack(
                '<I18f',
                1,  # version
                *hmd_data,
                scaled_X, scaled_Y, scaled_Z,
                scaled_Xr, player_yaw, scaled_Zr, # Y-rotation now uses player yaw from HMD
                0.0, 0.0, 0.0, 0.0, 0.0, 0.0 # Left controller (unused)
            )
            
            if not pipe.send(packet):
                print("Pipe disconnected, attempting to reconnect...")
                pipe.disconnect()
                if not pipe.create_and_wait():
                    break # Exit if reconnection fails
                        
        time.sleep(1.0 / settings.get('update_rate', 90))

if __name__ == "__main__":
    app = None
    try:
        main()
    finally:
        openvr.shutdown()
        print("OpenVR shutdown.") 