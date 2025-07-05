#!/usr/bin/env python3
"""
FNVR Calibrated Tracker - Optimized version with proper calibration
Based on FNVR_simple_tracker.py with fixes for weapon positioning issues
"""

import math
import keyboard
import openvr
import time
import ctypes
import struct
import numpy as np
import json
import os

# Named pipe for communication
PIPE_NAME = r"\\.\pipe\FNVRTracker"

# Configuration file path
CONFIG_PATH = "fnvr_calibration.json"

class CalibrationConfig:
    def __init__(self):
        self.load_defaults()
        self.load_from_file()
    
    def load_defaults(self):
        """Default calibration values matching legacy tracker"""
        self.config = {
            "position_multipliers": {
                "x": 50.0,
                "y": -50.0,
                "z": -50.0
            },
            "position_offsets": {
                "x": 15.0,
                "y": -10.0,
                "z": 0.0
            },
            "rotation_multipliers": {
                "x": -120.0,
                "y": 0.0,  # Not used in legacy
                "z": 120.0
            },
            "rotation_offsets": {
                "x": 10.0,
                "y": 0.0,
                "z": -75.0
            },
            "player_rotation": {
                "multiplier": -150.0,
                "offset": -7.5,
                "use_hmd_rotation": True  # Important: use HMD rotation, not controller
            },
            "smoothing": {
                "enabled": True,
                "position_factor": 0.8,  # 0-1, higher = less smoothing
                "rotation_factor": 0.9
            },
            "dead_zones": {
                "position": 0.001,  # Meters
                "rotation": 0.5     # Degrees
            },
            "calibration_gesture": {
                "enabled": True,
                "position": [0.0, -0.3, 0.2],  # Hold controller here to recalibrate
                "threshold": 0.1
            }
        }
    
    def load_from_file(self):
        """Load configuration from JSON file if it exists"""
        if os.path.exists(CONFIG_PATH):
            try:
                with open(CONFIG_PATH, 'r') as f:
                    loaded_config = json.load(f)
                    # Merge with defaults
                    self.merge_config(self.config, loaded_config)
                print(f"Loaded calibration from {CONFIG_PATH}")
            except Exception as e:
                print(f"Error loading config: {e}, using defaults")
    
    def save_to_file(self):
        """Save current configuration to JSON file"""
        try:
            with open(CONFIG_PATH, 'w') as f:
                json.dump(self.config, f, indent=4)
            print(f"Saved calibration to {CONFIG_PATH}")
        except Exception as e:
            print(f"Error saving config: {e}")
    
    def merge_config(self, base, updates):
        """Recursively merge configuration dictionaries"""
        for key, value in updates.items():
            if key in base and isinstance(base[key], dict) and isinstance(value, dict):
                self.merge_config(base[key], value)
            else:
                base[key] = value

def calculate_distance_xyz(x1, y1, z1, x2, y2, z2):
    distance = math.sqrt((x2 - x1)**2 + (y2 - y1)**2 + (z2 - z1)**2)
    return distance

def get_rotation(matrix):
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

class CalibratedTrackingApp:
    def __init__(self):
        self.vr_system = None
        self.pipe_handle = None
        self.kernel32 = ctypes.windll.kernel32
        self.config = CalibrationConfig()
        
        # Smoothing variables
        self.last_position = None
        self.last_rotation = None
        self.calibration_offset = {"x": 0, "y": 0, "z": 0}
        
        try:
            # Initialize OpenVR as a background application
            self.vr_system = openvr.init(openvr.VRApplication_Background)
            print("OpenVR initialized successfully.")
            print("Press 'C' to save current position as calibration center")
            print("Press 'R' to reset calibration")
            print("Press 'S' to save calibration settings")
        except openvr.OpenVRError as e:
            print(f"Error initializing OpenVR: {e}")
            self.shutdown()
            exit()

    def create_pipe_and_wait(self):
        """Create named pipe and wait for game connection"""
        self.pipe_handle = self.kernel32.CreateNamedPipeW(
            PIPE_NAME, 0x00000002, 0, 255, 512, 512, 0, None
        )
        if self.pipe_handle == -1:
            print(f"Failed to create pipe: {self.kernel32.GetLastError()}")
            return False
            
        print("Waiting for game connection...")
        if not self.kernel32.ConnectNamedPipe(self.pipe_handle, None):
            error = self.kernel32.GetLastError()
            if error != 535:  # ERROR_PIPE_CONNECTED
                print(f"Connect failed: {error}")
                self.kernel32.CloseHandle(self.pipe_handle)
                self.pipe_handle = None
                return False
        
        print("Game connected!")
        return True

    def apply_smoothing(self, current_pos, current_rot):
        """Apply smoothing to reduce jitter"""
        if not self.config.config["smoothing"]["enabled"]:
            return current_pos, current_rot
        
        if self.last_position is None:
            self.last_position = current_pos
            self.last_rotation = current_rot
            return current_pos, current_rot
        
        pos_factor = self.config.config["smoothing"]["position_factor"]
        rot_factor = self.config.config["smoothing"]["rotation_factor"]
        
        # Smooth position
        smoothed_pos = {}
        for axis in ['x', 'y', 'z']:
            smoothed_pos[axis] = (current_pos[axis] * pos_factor + 
                                  self.last_position[axis] * (1 - pos_factor))
        
        # Smooth rotation
        smoothed_rot = {}
        for axis in ['x', 'y', 'z']:
            smoothed_rot[axis] = (current_rot[axis] * rot_factor + 
                                  self.last_rotation[axis] * (1 - rot_factor))
        
        self.last_position = smoothed_pos
        self.last_rotation = smoothed_rot
        
        return smoothed_pos, smoothed_rot

    def apply_dead_zones(self, value, dead_zone):
        """Apply dead zone to reduce micro-movements"""
        if abs(value) < dead_zone:
            return 0.0
        return value

    def send_data(self, scaled_X, scaled_Y, scaled_Z, scaled_Xr, scaled_Zr, player_yaw):
        """Send data packet to game via pipe"""
        if not self.pipe_handle:
            return False
            
        # Packet format matching NVSE plugin expectation
        packet = struct.pack(
            '<I18f',
            1,  # version
            # HMD data (not used in legacy, but required by packet format)
            0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
            # Right controller data
            scaled_X, scaled_Y, scaled_Z,
            scaled_Xr, player_yaw, scaled_Zr,
            # Left controller data (unused)
            0.0, 0.0, 0.0, 0.0, 0.0, 0.0
        )
        
        bytes_written = ctypes.c_ulong()
        success = self.kernel32.WriteFile(
            self.pipe_handle, packet, len(packet),
            ctypes.byref(bytes_written), None
        )
        
        if not success:
            print("Pipe disconnected, attempting to reconnect...")
            self.kernel32.CloseHandle(self.pipe_handle)
            self.pipe_handle = None
            return False
            
        return True

    def handle_calibration_input(self, relative_position):
        """Handle calibration keyboard inputs"""
        if keyboard.is_pressed('c'):
            # Save current position as calibration center
            self.calibration_offset = {
                "x": -relative_position.v[0],
                "y": -relative_position.v[1], 
                "z": -relative_position.v[2]
            }
            print(f"Calibration set: X={self.calibration_offset['x']:.3f}, "
                  f"Y={self.calibration_offset['y']:.3f}, Z={self.calibration_offset['z']:.3f}")
            time.sleep(0.3)  # Debounce
        
        if keyboard.is_pressed('r'):
            # Reset calibration
            self.calibration_offset = {"x": 0, "y": 0, "z": 0}
            print("Calibration reset")
            time.sleep(0.3)  # Debounce
        
        if keyboard.is_pressed('s'):
            # Save configuration
            self.config.save_to_file()
            time.sleep(0.3)  # Debounce

    def shutdown(self):
        if self.pipe_handle:
            self.kernel32.CloseHandle(self.pipe_handle)
            self.pipe_handle = None
            
        if self.vr_system is not None:
            openvr.shutdown()
            self.vr_system = None
            print("OpenVR shutdown complete.")

    def run(self):
        if self.vr_system is None:
            print("OpenVR not initialized. Cannot run tracking.")
            return
            
        if not self.create_pipe_and_wait():
            return

        hmd_index = openvr.k_unTrackedDeviceIndex_Hmd
        max_devices = openvr.k_unMaxTrackedDeviceCount
        TrackedDevicePose_t = openvr.TrackedDevicePose_t

        # Find right controller index
        right_controller_index = None
        for device_index in range(max_devices):
            if self.vr_system.isTrackedDeviceConnected(device_index):
                device_class = self.vr_system.getTrackedDeviceClass(device_index)
                if device_class == openvr.TrackedDeviceClass_Controller:
                    role = self.vr_system.getControllerRoleForTrackedDeviceIndex(device_index)
                    if role == openvr.TrackedControllerRole_RightHand:
                        right_controller_index = device_index
                        print(f"Found right controller at index: {device_index}")
                        break

        while True:
            # Reconnect pipe if needed
            if not self.pipe_handle:
                if not self.create_pipe_and_wait():
                    break

            # Check if HMD is connected
            if not self.vr_system.isTrackedDeviceConnected(hmd_index):
                print("HMD not connected.")
                time.sleep(1)
                continue
                
            # Check if controller is connected
            if right_controller_index is None or not self.vr_system.isTrackedDeviceConnected(right_controller_index):
                print("Controller not connected, searching...")
                # Try to find controller again
                for device_index in range(max_devices):
                    if self.vr_system.isTrackedDeviceConnected(device_index):
                        device_class = self.vr_system.getTrackedDeviceClass(device_index)
                        if device_class == openvr.TrackedDeviceClass_Controller:
                            right_controller_index = device_index
                            print(f"Found controller at index: {device_index}")
                            break
                if right_controller_index is None:
                    time.sleep(1)
                    continue

            origin = openvr.TrackingUniverseStanding
            predicted_seconds = 0.0
            poses_array = (TrackedDevicePose_t * max_devices)()

            returned_poses = self.vr_system.getDeviceToAbsoluteTrackingPose(
                origin, predicted_seconds, poses_array
            )

            if len(returned_poses) > hmd_index and len(returned_poses) > right_controller_index:
                hmd_pose = returned_poses[hmd_index]
                con_pose = returned_poses[right_controller_index]

                if hmd_pose.bPoseIsValid and con_pose.bPoseIsValid:
                    m = hmd_pose.mDeviceToAbsoluteTracking
                    n = con_pose.mDeviceToAbsoluteTracking

                    hmd_position_world = get_position(m)
                    con_position_world = get_position(n)

                    hmd_matrix = [
                        [m[0][0], m[0][1], m[0][2], m[0][3]],
                        [m[1][0], m[1][1], m[1][2], m[1][3]],
                        [m[2][0], m[2][1], m[2][2], m[2][3]]
                    ]
                    hmd_rotation_world = get_rotation(hmd_matrix)

                    con_matrix = [
                        [n[0][0], n[0][1], n[0][2], n[0][3]],
                        [n[1][0], n[1][1], n[1][2], n[1][3]],
                        [n[2][0], n[2][1], n[2][2], n[2][3]]
                    ]
                    con_rotation_world = get_rotation(con_matrix)

                    world_diff_x = con_position_world.v[0] - hmd_position_world.v[0]
                    world_diff_y = con_position_world.v[1] - hmd_position_world.v[1]
                    world_diff_z = con_position_world.v[2] - hmd_position_world.v[2]
                    world_diff_vector = openvr.HmdVector3_t()
                    world_diff_vector.v = (ctypes.c_float * 3)(world_diff_x, world_diff_y, world_diff_z)

                    hmd_rotation_inverse = quaternion_conjugate(hmd_rotation_world)
                    relative_position = rotate_vector_by_quaternion(world_diff_vector, hmd_rotation_inverse)
                    relative_rotation = quaternion_multiply(hmd_rotation_inverse, con_rotation_world)

                    # Handle calibration input
                    self.handle_calibration_input(relative_position)

                    # Check for calibration gesture
                    if self.config.config["calibration_gesture"]["enabled"]:
                        cal_pos = self.config.config["calibration_gesture"]["position"]
                        distance = calculate_distance_xyz(
                            relative_position.v[0], relative_position.v[1], relative_position.v[2],
                            cal_pos[0], cal_pos[1], cal_pos[2]
                        )
                        if distance < self.config.config["calibration_gesture"]["threshold"]:
                            self.calibration_offset = {
                                "x": -relative_position.v[0] + cal_pos[0],
                                "y": -relative_position.v[1] + cal_pos[1],
                                "z": -relative_position.v[2] + cal_pos[2]
                            }
                            print("Calibration gesture detected - position reset")

                    # Apply calibration offset
                    calibrated_x = relative_position.v[0] + self.calibration_offset["x"]
                    calibrated_y = relative_position.v[1] + self.calibration_offset["y"]
                    calibrated_z = relative_position.v[2] + self.calibration_offset["z"]

                    # InertiaController Bone Pose with calibration
                    inertiaZ, inertiaX, inertiaY = calibrated_z, calibrated_x, calibrated_y

                    # InertiaController Bone Rotation
                    inertiaZr, inertiaXr, inertiaYr = relative_rotation.x, relative_rotation.y, relative_rotation.z

                    # HMD rotation for player angle
                    playerXr, playerZr, playerYr = hmd_rotation_world.x, hmd_rotation_world.y, hmd_rotation_world.z

                    # Apply smoothing
                    pos_dict = {"x": inertiaX, "y": inertiaY, "z": inertiaZ}
                    rot_dict = {"x": inertiaXr, "y": inertiaYr, "z": inertiaZr}
                    smoothed_pos, smoothed_rot = self.apply_smoothing(pos_dict, rot_dict)

                    # Apply dead zones
                    cfg = self.config.config
                    for axis in ['x', 'y', 'z']:
                        smoothed_pos[axis] = self.apply_dead_zones(
                            smoothed_pos[axis], cfg["dead_zones"]["position"]
                        )
                        smoothed_rot[axis] = self.apply_dead_zones(
                            smoothed_rot[axis], cfg["dead_zones"]["rotation"] * 0.0174533  # Convert to radians
                        )

                    # Apply scaling and offsets from configuration
                    scaled_X = (smoothed_pos['x'] * cfg["position_multipliers"]["x"]) + cfg["position_offsets"]["x"]
                    scaled_Y = (smoothed_pos['y'] * cfg["position_multipliers"]["y"]) + cfg["position_offsets"]["y"]
                    scaled_Z = (smoothed_pos['z'] * cfg["position_multipliers"]["z"]) + cfg["position_offsets"]["z"]
                    scaled_Xr = (smoothed_rot['x'] * cfg["rotation_multipliers"]["x"]) + cfg["rotation_offsets"]["x"]
                    scaled_Zr = (smoothed_rot['z'] * cfg["rotation_multipliers"]["z"]) + cfg["rotation_offsets"]["z"]
                    
                    # Use HMD rotation for player yaw if configured (fixes the main issue!)
                    if cfg["player_rotation"]["use_hmd_rotation"]:
                        player_yaw = (playerYr * cfg["player_rotation"]["multiplier"]) + cfg["player_rotation"]["offset"]
                    else:
                        player_yaw = (-smoothed_rot['x'] * cfg["player_rotation"]["multiplier"]) + cfg["player_rotation"]["offset"]

                    # Send data via pipe
                    if not self.send_data(scaled_X, scaled_Y, scaled_Z, scaled_Xr, scaled_Zr, player_yaw):
                        continue

                    if keyboard.is_pressed('Tab'):
                        print(f"Con Pos: x: {calibrated_x:.4f}, y: {calibrated_y:.4f}, z: {calibrated_z:.4f}")
                        print(f"Con Rot: w: {relative_rotation.w:.4f}, x: {relative_rotation.x:.4f}, "
                              f"y: {relative_rotation.y:.4f}, z: {relative_rotation.z:.4f}")
                        print(f"Scaled: X={scaled_X:.2f}, Y={scaled_Y:.2f}, Z={scaled_Z:.2f}")

                    # Pipboy gesture
                    distance = calculate_distance_xyz(calibrated_x, calibrated_y, calibrated_z, 0.12, 0.24, -0.29)
                    if distance < 0.1:
                        # Send special pipboy position
                        self.send_data(
                            (-0.1615 * cfg["position_multipliers"]["x"]) + cfg["position_offsets"]["x"],
                            (-0.5 * cfg["position_multipliers"]["y"]) + cfg["position_offsets"]["y"],
                            (0.1281 * cfg["position_multipliers"]["z"]) + cfg["position_offsets"]["z"],
                            (0.0655 * cfg["rotation_multipliers"]["x"]) + cfg["rotation_offsets"]["x"],
                            (0.6291 * cfg["rotation_multipliers"]["z"]) + cfg["rotation_offsets"]["z"],
                            (-0.0655 * cfg["player_rotation"]["multiplier"]) + cfg["player_rotation"]["offset"]
                        )
                        keyboard.press('Tab')
                        time.sleep(0.05)
                        keyboard.release('Tab')

                    # Pause menu gesture
                    distance = calculate_distance_xyz(calibrated_x, calibrated_y, calibrated_z, -0.3158, -0.1897, -0.1316)
                    if distance < 0.1:
                        print('Escape')
                        keyboard.press('Escape')
                        time.sleep(0.75)
                        keyboard.release('Escape')

                else:
                    print("Invalid HMD or controller pose.")
            else:
                print("Could not retrieve poses.")

            time.sleep(1.5/60)  # ~40 FPS

if __name__ == "__main__":
    tracking_app = CalibratedTrackingApp()
    try:
        tracking_app.run()
    finally:
        tracking_app.shutdown()