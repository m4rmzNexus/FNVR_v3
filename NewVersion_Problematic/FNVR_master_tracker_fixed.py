#!/usr/bin/env python3
"""
FNVR Master Tracker - FIXED VERSION
Controller detection logic changed to match Legacy tracker
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

def calculate_distance_xyz(x1, y1, z1, x2, y2, z2):
    """Calculates Euclidean distance between two 3D points."""
    return math.sqrt((x2 - x1)**2 + (y2 - y1)**2 + (z2 - z1)**2)

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
        
    print("\n--- FNVR Master Tracker Running (FIXED) ---")
    print("Controller Detection: Legacy Mode (First Available)")
    print(f"Gesture Controls: {'Enabled' if settings.get('enable_gestures', False) else 'Disabled'}")
    print(f"Scaling Mode: {'ALPHA' if settings.get('use_alpha_scaling', False) else 'GUI'}")
    print("-" * 45)
    
    # Device indices
    hmd_index = openvr.k_unTrackedDeviceIndex_Hmd
    controller_index = -1  # Will be found dynamically
    
    last_gesture_time = 0
    
    while True:
        # FIXED: Use Legacy-style controller detection
        if controller_index == -1:
            # Method 1: Try index 2 first (Legacy default)
            if vr.getTrackedDeviceClass(2) == openvr.TrackedDeviceClass_Controller:
                controller_index = 2
                print(f"Found controller at legacy index 2")
            else:
                # Method 2: Find first available controller
                for i in range(openvr.k_unMaxTrackedDeviceCount):
                    if vr.getTrackedDeviceClass(i) == openvr.TrackedDeviceClass_Controller:
                        controller_index = i
                        print(f"Found controller at index {i}")
                        break
                        
            if controller_index == -1:
                print("No controller found, retrying...")
                time.sleep(1)
                continue
            
        poses = vr.getDeviceToAbsoluteTrackingPose(
            openvr.TrackingUniverseStanding, 0.0, openvr.k_unMaxTrackedDeviceCount
        )
        
        hmd_pose = poses[hmd_index]
        controller_pose = poses[controller_index]
            
        if hmd_pose.bPoseIsValid and controller_pose.bPoseIsValid:
            # --- Core VR Math (IDENTICAL TO LEGACY) ---
            hmd_m = hmd_pose.mDeviceToAbsoluteTracking
            con_m = controller_pose.mDeviceToAbsoluteTracking
            
            hmd_pos = get_position(hmd_m)
            con_pos = get_position(con_m)
            
            hmd_rot = get_rotation(hmd_m)
            con_rot = get_rotation(con_m)
            
            # Calculate relative position
            world_diff = openvr.HmdVector3_t()
            world_diff.v[0] = con_pos.v[0] - hmd_pos.v[0]
            world_diff.v[1] = con_pos.v[1] - hmd_pos.v[1]
            world_diff.v[2] = con_pos.v[2] - hmd_pos.v[2]
            
            hmd_rotation_inverse = quaternion_conjugate(hmd_rot)
            relative_position = rotate_vector_by_quaternion(world_diff, hmd_rotation_inverse)
            relative_rotation = quaternion_multiply(hmd_rotation_inverse, con_rot)
            
            # IDENTICAL variable naming to Legacy
            inertiaZ, inertiaX, inertiaY = relative_position.v
            inertiaZr, inertiaXr, inertiaYr = relative_rotation.x, relative_rotation.y, relative_rotation.z

            # --- Gesture Control (IDENTICAL TO LEGACY) ---
            current_time = time.time()
            if settings.get('enable_gestures', False) and (current_time - last_gesture_time > 1.0):
                # PipBoy Gesture
                if settings.get('pipboy_gesture_enabled', False):
                    distance = calculate_distance_xyz(inertiaX, inertiaY, inertiaZ, 0.12, 0.24, -0.29)
                    if distance < 0.1:
                        print("Pip-Boy gesture activated!")
                        keyboard.press_and_release('Tab')
                        last_gesture_time = current_time
                        time.sleep(0.5)
                
                # Pause Gesture
                if settings.get('pause_gesture_enabled', False):
                    distance = calculate_distance_xyz(inertiaX, inertiaY, inertiaZ, -0.3158, -0.1897, -0.1316)
                    if distance < 0.1:
                        print("Pause gesture activated!")
                        keyboard.press_and_release('Escape')
                        last_gesture_time = current_time
                        time.sleep(0.75)

            # --- Scaling (IDENTICAL TO LEGACY when use_alpha_scaling=true) ---
            if settings.get('use_alpha_scaling', False):
                scaled_X = (inertiaX * 50) + 15
                scaled_Y = (inertiaY * -50) + -10
                scaled_Z = (inertiaZ * -50) + 0
                scaled_Xr = (inertiaXr * -120) + 10
                scaled_Zr = (inertiaZr * 120) + -75
            else:
                # GUI scaling mode
                scaled_X = (inertiaX * settings['position_scale']) + settings['camera_offset_x']
                scaled_Y = (inertiaY * (-settings['position_scale'] if settings['invert_y'] else settings['position_scale'])) + settings['camera_offset_y']
                scaled_Z = (inertiaZ * (-settings['position_scale'] if settings['invert_z'] else settings['position_scale'])) + settings['camera_offset_z']
                
                scaled_Xr = (inertiaXr * (-settings['rotation_scale'] if settings['invert_pitch'] else settings['rotation_scale'])) + settings['rotation_offset_x']
                scaled_Zr = (inertiaZr * (settings['rotation_scale'] if settings['invert_roll'] else -settings['rotation_scale'])) + settings['rotation_offset_z']

            # --- Data Packet Assembly ---
            packet = struct.pack(
                '<I18f',
                1,  # version
                0.0, 0.0, 0.0, 0.0, 0.0, 0.0,  # HMD data (unused in Legacy mode)
                scaled_X, scaled_Y, scaled_Z,
                scaled_Xr, 0.0, scaled_Zr,  # Y-rotation unused in Legacy
                0.0, 0.0, 0.0, 0.0, 0.0, 0.0  # Left controller (unused)
            )
            
            if not pipe.send(packet):
                print("Pipe disconnected, attempting to reconnect...")
                pipe.disconnect()
                if not pipe.create_and_wait():
                    break
        else:
            if not controller_pose.bPoseIsValid:
                print("Controller tracking lost, searching again...")
                controller_index = -1
                        
        time.sleep(1.0 / settings.get('update_rate', 60))

if __name__ == "__main__":
    try:
        main()
    finally:
        openvr.shutdown()
        print("OpenVR shutdown.") 