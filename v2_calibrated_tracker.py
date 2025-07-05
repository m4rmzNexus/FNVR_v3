#!/usr/bin/env python3
"""
FNVR Simple Tracker - v2 Calibrated Version
- Fixes rotational drift by correctly calculating player yaw from the HMD.
- Based on legacyFNVR_Tracker.py but using named pipe instead of INI
"""

import math
import keyboard
import openvr
import time
import ctypes
import struct
import numpy as np

# ================== CONFIGURABLE PARAMETERS ==================
# Scale and offset applied to the absolute HMD yaw **before** it is
# sent to the game.  The legacy tracker effectively produced ~±20-25°
# values, not full 360°.  Adjust as needed in-game.
PLAYER_YAW_SCALE = -0.25  # multiply HMD yaw in degrees (e.g. -90 → 22.5)
PLAYER_YAW_OFFSET = 0.0   # additional offset in degrees if weapon is off-center
# When playing via VorpX the camera yaw is already applied by VorpX,
# so we must not send a second yaw value.  Set this to True in that case.
VORPX_MODE = True
VORPX_USE_LEGACY_YAW = True  # If True and VorpX mode, mimic legacy yaw-from-controller behaviour
# ==============================================================

# Named pipe for communication
PIPE_NAME = r"\\.\\pipe\\FNVRTracker"

def quaternion_to_yaw(q):
    """Calculates yaw (Y-axis rotation) from a quaternion."""
    # This formula is for a Y-up coordinate system like OpenVR's
    siny_cosp = 2 * (q.w * q.y + q.x * q.z)
    cosy_cosp = 1 - 2 * (q.y * q.y + q.z * q.z)
    yaw = np.arctan2(siny_cosp, cosy_cosp)
    return yaw # Result is in radians

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

class SimpleTrackingApp:
    def __init__(self):
        self.vr_system = None
        self.pipe_handle = None
        self.kernel32 = ctypes.windll.kernel32
        
        try:
            # Initialize OpenVR as a background application
            self.vr_system = openvr.init(openvr.VRApplication_Background)
            print("OpenVR initialized successfully.")
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

    def send_data(self, scaled_X, scaled_Y, scaled_Z, scaled_Xr, scaled_Zr, player_yaw):
        """Send data packet to game via pipe"""
        if not self.pipe_handle:
            return False
            
        # Packet format matching NVSE plugin expectation
        # 1 int (version) + 18 floats
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
        
        # print(f"Packet size: {len(packet)} bytes (expected: 76)")
        
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
                    # Check if this is the right controller
                    role = self.vr_system.getControllerRoleForTrackedDeviceIndex(device_index)
                    if role == openvr.TrackedControllerRole_RightHand:
                        right_controller_index = device_index
                        print(f"Found right controller at index: {device_index}")
                        break
        
        if right_controller_index is None:
            # Fallback: use first controller found
            for device_index in range(max_devices):
                if self.vr_system.isTrackedDeviceConnected(device_index):
                    device_class = self.vr_system.getTrackedDeviceClass(device_index)
                    if device_class == openvr.TrackedDeviceClass_Controller:
                        right_controller_index = device_index
                        print(f"Using controller at index: {device_index} (fallback)")
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

                    # InertiaController Bone Pose is set to match the controller's position.
                    inertiaZ, inertiaX, inertiaY = relative_position.v

                    # InertiaController Bone Rotation is set to match the controller's position.
                    inertiaZr, inertiaXr, inertiaYr = relative_rotation.x, relative_rotation.y, relative_rotation.z

                    # Apply exact same scaling as legacy for controller
                    scaled_X = (inertiaX * 50) + 15
                    scaled_Y = (inertiaY * -50) + -10
                    scaled_Z = (inertiaZ * -50) + 0
                    scaled_Xr = (inertiaXr * -120) + 10 # Corresponds to Pitch
                    scaled_Zr = (inertiaZr * 120) + -75 # Corresponds to Roll

                    # === YAW CALCULATION FIX ===
                    # The game expects the HMD's absolute Yaw rotation for the player's body.
                    # We calculate this from the HMD's world rotation quaternion.
                    hmd_yaw_radians = quaternion_to_yaw(hmd_rotation_world)
                    hmd_yaw_degrees = np.degrees(hmd_yaw_radians)
                    
                    # VorpX handling
                    if VORPX_MODE:
                        if VORPX_USE_LEGACY_YAW:
                            # Re-use the original heuristic: derive yaw from controller roll (-inertiaXr)
                            player_yaw = (-inertiaXr * -150) - 7.5  # matches legacy INI scaling
                        else:
                            player_yaw = 0.0  # send zero yaw
                    else:
                        # Normal (non-VorpX) mode – use scaled HMD yaw
                        player_yaw = (hmd_yaw_degrees * PLAYER_YAW_SCALE) + PLAYER_YAW_OFFSET
                    
                    # Debug: Print scaled values
                    # print(f"Sending: X={scaled_X:.2f}, Y={scaled_Y:.2f}, Z={scaled_Z:.2f}")
                    # print(f"         Xr={scaled_Xr:.2f}, Yaw={player_yaw:.2f}, Zr={scaled_Zr:.2f}")
                    
                    # Send data via pipe
                    if not self.send_data(scaled_X, scaled_Y, scaled_Z, scaled_Xr, scaled_Zr, player_yaw):
                        continue

                    if keyboard.is_pressed('Tab'):
                        print(f"Con Pos: x: {relative_position.v[0]:.4f}, y: {relative_position.v[1]:.4f}, z: {relative_position.v[2]:.4f}")
                        print(f"Con Rot: w: {relative_rotation.w:.4f}, x: {relative_rotation.x:.4f}, y: {relative_rotation.y:.4f}, z: {relative_rotation.z:.4f}")

                    # Con Pos for accurate pipboy reading
                    distance = calculate_distance_xyz(relative_position.v[0], relative_position.v[1], relative_position.v[2], 0.12, 0.24, -0.29)
                    if distance < 0.1:
                        # Send special pipboy position
                        self.send_data(
                            (-0.1615 * 50) + 15,
                            (-0.5 * -50) + -10,
                            (0.1281 * -50) + 0,
                            (0.0655 * -120) + 10,
                            (0.6291 * 120) + -75,
                            player_yaw # Use the current correct player_yaw
                        )
                        keyboard.press('Tab')
                        time.sleep(0.05)
                        keyboard.release('Tab')

                    # Controller gesture for opening the pause menu:
                    distance = calculate_distance_xyz(relative_position.v[0], relative_position.v[1], relative_position.v[2], -0.3158, -0.1897, -0.1316)
                    if distance < 0.1:
                        print('Escape')
                        keyboard.press('Escape')
                        time.sleep(0.75)
                        keyboard.release('Escape')

                else:
                    print("Invalid HMD or Controller pose.")
            else:
                print("Could not retrieve HMD or Controller pose.")

            time.sleep(1.0/120.0)  # 120 FPS for VorpX / smoother tracking

if __name__ == "__main__":
    tracking_app = SimpleTrackingApp()
    try:
        tracking_app.run()
    finally:
        tracking_app.shutdown() 