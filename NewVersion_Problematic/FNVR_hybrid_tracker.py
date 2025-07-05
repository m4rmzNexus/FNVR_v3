#!/usr/bin/env python3
"""
FNVR Hybrid Tracker - Supports both Named Pipe and INI file output
This version can work with both the new DLL system and the old INI system
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

# --- Configuration ---
USE_NAMED_PIPE = True  # Set to False to use INI file instead
USE_BOTH = True        # Set to True to use BOTH methods simultaneously
INI_FILE_PATH = 'E:/SteamLibrary/steamapps/common/Fallout New Vegas/Data/Config/Meh.ini'  # Update this path!

# --- Helper Functions ---

def load_settings():
    """Loads settings from fnvr_settings.json"""
    settings_path = os.path.join(os.path.dirname(__file__), 'fnvr_settings.json')
    try:
        with open(settings_path, 'r') as f:
            return json.load(f)
    except FileNotFoundError:
        print(f"WARNING: {settings_path} not found. Using defaults.")
        return {
            "use_alpha_scaling": True,
            "position_scale": 50.0,
            "rotation_scale": 120.0,
            "camera_offset_x": 15.0,
            "camera_offset_y": -10.0,
            "camera_offset_z": 0.0,
            "rotation_offset_x": 10.0,
            "rotation_offset_z": -75.0,
            "update_rate": 60,
            "enable_gestures": True,
            "pipboy_gesture_enabled": True,
            "pause_gesture_enabled": True,
            "gesture_activation_radius": 0.1,
            "pipboy_gesture_coords": [0.12, 0.24, -0.29],
            "pause_gesture_coords": [-0.31, -0.18, -0.13]
        }

def update_ini(iX, iY, iZ, iXr, iYr, iZr, pXr, pYr, pZr):
    """Write data to INI file (legacy method)"""
    try:
        # Create directory if it doesn't exist
        ini_dir = os.path.dirname(INI_FILE_PATH)
        if not os.path.exists(ini_dir):
            os.makedirs(ini_dir)
            
        with open(INI_FILE_PATH, "w") as f:
            f.write("[Standard]\n")
            f.write(f'fCanIOpenThis = {1}\n')
            f.write(f"fiX = {(iX*50) + 15:.4f}\n")
            f.write(f"fiY = {(iY*-50) + -10:.4f}\n")
            f.write(f"fiZ = {(iZ*-50) + 0:.4f}\n")
            f.write(f"fiXr = {(iXr * -120) + 10:.4f}\n")
            f.write(f"fiZr = {(iZr * 120) + -75:.4f}\n")
            f.write(f"fpZr = {(pZr * -150) - 7.5:.4f}\n")
        return True
    except Exception as e:
        print(f'Error updating INI: {e}')
        return False

def calculate_distance_xyz(x1, y1, z1, x2, y2, z2):
    distance = math.sqrt((x2 - x1)**2 + (y2 - y1)**2 + (z2 - z1)**2)
    return distance

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
            
        print("Waiting for game connection via Named Pipe...")
        if not self.kernel32.ConnectNamedPipe(self.handle, None):
            error = self.kernel32.GetLastError()
            if error != 535: # ERROR_PIPE_CONNECTED
                print(f"Connect failed: {error}")
                self.kernel32.CloseHandle(self.handle)
                self.handle = None
                return False
        
        print("Game connected via Named Pipe!")
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
    
    print("\n" + "="*60)
    print("FNVR HYBRID TRACKER")
    print("="*60)
    print(f"Mode: {'BOTH (Named Pipe + INI)' if USE_BOTH else ('Named Pipe' if USE_NAMED_PIPE else 'INI File')}")
    print(f"INI Path: {INI_FILE_PATH}")
    print(f"Alpha Scaling: {'Enabled' if settings.get('use_alpha_scaling', True) else 'Disabled'}")
    print(f"Gestures: {'Enabled' if settings.get('enable_gestures', True) else 'Disabled'}")
    print("="*60 + "\n")

    # Initialize OpenVR
    try:
        vr = openvr.init(openvr.VRApplication_Background)
    except openvr.OpenVRError as e:
        print(f"Error initializing OpenVR: {e}")
        print("Make sure SteamVR is running!")
        return
        
    # Initialize communication
    pipe = None
    if USE_NAMED_PIPE or USE_BOTH:
        pipe = NamedPipeServer()
        if not pipe.create_and_wait():
            if not USE_BOTH:
                openvr.shutdown()
                return
            else:
                print("Named Pipe failed, continuing with INI only...")
                pipe = None
    
    if not USE_NAMED_PIPE or USE_BOTH:
        print(f"Using INI file mode. Writing to: {INI_FILE_PATH}")
        
    # Find devices
    hmd_index = openvr.k_unTrackedDeviceIndex_Hmd
    right_controller_index = -1
    
    last_gesture_time = 0
    frame_count = 0
    
    print("\nTracker running. Press Ctrl+C to stop.\n")
    
    while True:
        frame_count += 1
        
        # Find right controller if not found
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
            # Get matrices
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
            
            inertiaZ, inertiaX, inertiaY = relative_position.v
            inertiaZr, inertiaXr, inertiaYr = relative_rotation.x, relative_rotation.y, relative_rotation.z
            playerXr, playerZr, playerYr = hmd_rot.x, hmd_rot.y, hmd_rot.z
            
            # Gesture detection
            current_time = time.time()
            if settings.get('enable_gestures', True) and (current_time - last_gesture_time > 1.0):
                # PipBoy gesture
                if settings.get('pipboy_gesture_enabled', True):
                    coords = settings.get('pipboy_gesture_coords', [0.12, 0.24, -0.29])
                    radius = settings.get('gesture_activation_radius', 0.1)
                    dist = calculate_distance_xyz(inertiaX, inertiaY, inertiaZ, coords[0], coords[1], coords[2])
                    if dist < radius:
                        print("Pip-Boy gesture activated!")
                        keyboard.press_and_release('Tab')
                        last_gesture_time = current_time
                        time.sleep(0.5)
                
                # Pause gesture
                if settings.get('pause_gesture_enabled', True):
                    coords = settings.get('pause_gesture_coords', [-0.31, -0.18, -0.13])
                    radius = settings.get('gesture_activation_radius', 0.1)
                    dist = calculate_distance_xyz(inertiaX, inertiaY, inertiaZ, coords[0], coords[1], coords[2])
                    if dist < radius:
                        print("Pause gesture activated!")
                        keyboard.press_and_release('Escape')
                        last_gesture_time = current_time
                        time.sleep(0.5)
            
            # Send data via INI
            if not USE_NAMED_PIPE or USE_BOTH:
                success = update_ini(inertiaX, inertiaY, inertiaZ, inertiaXr, inertiaYr, inertiaZr, playerXr, playerYr, -inertiaXr)
                if frame_count % 300 == 0:  # Print status every ~5 seconds at 60fps
                    print(f"INI Update: {'✓' if success else '✗'} | Frame: {frame_count}")
            
            # Send data via Named Pipe
            if pipe and (USE_NAMED_PIPE or USE_BOTH):
                # Use alpha scaling values for compatibility
                scaled_X = (inertiaX * 50) + 15
                scaled_Y = (inertiaY * -50) + -10
                scaled_Z = (inertiaZ * -50) + 0
                scaled_Xr = (inertiaXr * -120) + 10
                scaled_Zr = (inertiaZr * 120) + -75
                player_yaw = 0.0  # Not used in legacy system
                
                # Pack data
                packet = struct.pack(
                    '<I18f',
                    1,  # version
                    0.0, 0.0, 0.0, 0.0, 0.0, 0.0,  # HMD data (not used)
                    scaled_X, scaled_Y, scaled_Z,
                    scaled_Xr, player_yaw, scaled_Zr,
                    0.0, 0.0, 0.0, 0.0, 0.0, 0.0  # Left controller (not used)
                )
                
                if not pipe.send(packet):
                    print("Pipe disconnected, attempting to reconnect...")
                    pipe.disconnect()
                    if not pipe.create_and_wait():
                        if not USE_BOTH:
                            break
                        else:
                            print("Pipe reconnection failed, continuing with INI only...")
                            pipe = None
                elif frame_count % 300 == 0:
                    print(f"Pipe Send: ✓ | Frame: {frame_count}")
                        
        time.sleep(1.0 / settings.get('update_rate', 60))

if __name__ == "__main__":
    try:
        main()
    finally:
        openvr.shutdown()
        print("\nTracker stopped. OpenVR shutdown.") 