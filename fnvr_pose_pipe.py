#!/usr/bin/env python3
"""
FNVR Pose Pipe - Modern Raw Pose Tracker
- Sends raw OpenVR HMD and controller pose (quaternion + position) and timestamp via named pipe
- No legacy scaling, no heuristics, no game-specific offsets
- For use with a modern NVSE plugin that handles all calibration, scaling, and coordinate transforms
"""

import openvr
import time
import ctypes
import struct
import numpy as np

PIPE_NAME = r"\\.\pipe\FNVRTracker"

# Utility: get quaternion from 3x4 OpenVR matrix
def get_quaternion(matrix):
    m = matrix
    qw = np.sqrt(max(0, 1 + m[0][0] + m[1][1] + m[2][2])) / 2
    qx = np.sqrt(max(0, 1 + m[0][0] - m[1][1] - m[2][2])) / 2
    qy = np.sqrt(max(0, 1 - m[0][0] + m[1][1] - m[2][2])) / 2
    qz = np.sqrt(max(0, 1 - m[0][0] - m[1][1] + m[2][2])) / 2
    qx = np.copysign(qx, m[2][1] - m[1][2])
    qy = np.copysign(qy, m[0][2] - m[2][0])
    qz = np.copysign(qz, m[1][0] - m[0][1])
    return (qw, qx, qy, qz)

def get_position(matrix):
    return (matrix[0][3], matrix[1][3], matrix[2][3])

def quaternion_conjugate(q):
    return (q[0], -q[1], -q[2], -q[3])

def quaternion_multiply(q1, q2):
    w = q1[0] * q2[0] - q1[1] * q2[1] - q1[2] * q2[2] - q1[3] * q2[3]
    x = q1[0] * q2[1] + q1[1] * q2[0] + q1[2] * q2[3] - q1[3] * q2[2]
    y = q1[0] * q2[2] - q1[1] * q2[3] + q1[2] * q2[0] + q1[3] * q2[1]
    z = q1[0] * q2[3] + q1[1] * q2[2] - q1[2] * q2[1] + q1[3] * q2[0]
    return (w, x, y, z)

def rotate_vector_by_quaternion(v, q):
    # v: (x, y, z), q: (w, x, y, z)
    qvec = (0.0, v[0], v[1], v[2])
    qinv = quaternion_conjugate(q)
    qres = quaternion_multiply(q, quaternion_multiply(qvec, qinv))
    return (qres[1], qres[2], qres[3])

class RawPosePipe:
    def __init__(self):
        self.vr_system = None
        self.pipe_handle = None
        self.kernel32 = ctypes.windll.kernel32
        try:
            self.vr_system = openvr.init(openvr.VRApplication_Background)
            print("OpenVR initialized.")
        except openvr.OpenVRError as e:
            print(f"OpenVR init error: {e}")
            exit(1)

    def create_pipe_and_wait(self):
        self.pipe_handle = self.kernel32.CreateNamedPipeW(
            PIPE_NAME, 0x00000002, 0, 255, 512, 512, 0, None
        )
        if self.pipe_handle == -1:
            print(f"Failed to create pipe: {self.kernel32.GetLastError()}")
            return False
        print("Waiting for game connection...")
        if not self.kernel32.ConnectNamedPipe(self.pipe_handle, None):
            error = self.kernel32.GetLastError()
            if error != 535:
                print(f"Connect failed: {error}")
                self.kernel32.CloseHandle(self.pipe_handle)
                self.pipe_handle = None
                return False
        print("Game connected!")
        return True

    def send_pose(self, hmd_q, hmd_p, ctl_q, ctl_p, rel_p, timestamp):
        # New unified packet format with flags
        # Base packet: version (I), flags (I), hmd_q (4f), hmd_p (3f), ctl_q (4f), ctl_p (3f), rel_p (3f), timestamp (d)
        # Total: 88 bytes (4+4+16+12+16+12+12+8)
        
        # For now, only send basic data (no left controller, no inputs)
        flags = 0x01  # VR_FLAG_BASIC_DATA
        
        packet = struct.pack(
            '<II4f3f4f3f3fd',
            2,                # version
            flags,            # flags (basic data only)
            *hmd_q, *hmd_p,   # HMD quaternion, position
            *ctl_q, *ctl_p,   # Right controller quaternion, position
            *rel_p,           # Controller pos relative to HMD (meters)
            timestamp         # Timestamp (seconds since epoch)
        )
        # Debug: Print packet size on first send
        if not hasattr(self, '_first_packet_logged'):
            print(f"Sending packet size: {len(packet)} bytes")
            self._first_packet_logged = True
        bytes_written = ctypes.c_ulong()
        success = self.kernel32.WriteFile(
            self.pipe_handle, packet, len(packet),
            ctypes.byref(bytes_written), None
        )
        if not success:
            print("Pipe disconnected, reconnecting...")
            self.kernel32.CloseHandle(self.pipe_handle)
            self.pipe_handle = None
            return False
        return True

    def shutdown(self):
        if self.pipe_handle:
            self.kernel32.CloseHandle(self.pipe_handle)
            self.pipe_handle = None
        if self.vr_system:
            openvr.shutdown()
            self.vr_system = None
            print("OpenVR shutdown.")

    def run(self):
        if not self.create_pipe_and_wait():
            return
        hmd_index = openvr.k_unTrackedDeviceIndex_Hmd
        max_devices = openvr.k_unMaxTrackedDeviceCount
        TrackedDevicePose_t = openvr.TrackedDevicePose_t
        right_controller_index = None
        for device_index in range(max_devices):
            if self.vr_system.isTrackedDeviceConnected(device_index):
                device_class = self.vr_system.getTrackedDeviceClass(device_index)
                if device_class == openvr.TrackedDeviceClass_Controller:
                    role = self.vr_system.getControllerRoleForTrackedDeviceIndex(device_index)
                    if role == openvr.TrackedControllerRole_RightHand:
                        right_controller_index = device_index
                        break
        if right_controller_index is None:
            for device_index in range(max_devices):
                if self.vr_system.isTrackedDeviceConnected(device_index):
                    device_class = self.vr_system.getTrackedDeviceClass(device_index)
                    if device_class == openvr.TrackedDeviceClass_Controller:
                        right_controller_index = device_index
                        break
        # Setup dummy data for when controller is not found
        dummy_ctl_q = (1.0, 0.0, 0.0, 0.0)  # Identity quaternion
        dummy_ctl_p = (0.0, 0.0, 0.0)       # Origin position
        dummy_rel_p = (0.0, 0.0, 0.0)       # No relative position
        
        print(f"Controller index: {right_controller_index}")
        if right_controller_index is None:
            print("WARNING: No controller found! Using dummy data.")
        
        while True:
            if not self.pipe_handle:
                if not self.create_pipe_and_wait():
                    break
            poses_array = (TrackedDevicePose_t * max_devices)()
            returned_poses = self.vr_system.getDeviceToAbsoluteTrackingPose(
                openvr.TrackingUniverseStanding, 0.0, poses_array
            )
            
            if len(returned_poses) > hmd_index:
                hmd_pose = returned_poses[hmd_index]
                
                if hmd_pose.bPoseIsValid:
                    m = hmd_pose.mDeviceToAbsoluteTracking
                    hmd_q = get_quaternion(m)
                    hmd_p = get_position(m)
                    
                    # Check if we have a controller and it's valid
                    if right_controller_index is not None and len(returned_poses) > right_controller_index:
                        ctl_pose = returned_poses[right_controller_index]
                        if ctl_pose.bPoseIsValid:
                            n = ctl_pose.mDeviceToAbsoluteTracking
                            ctl_q = get_quaternion(n)
                            ctl_p = get_position(n)
                            # Controller pos relative to HMD (in HMD space)
                            world_diff = (ctl_p[0] - hmd_p[0], ctl_p[1] - hmd_p[1], ctl_p[2] - hmd_p[2])
                            hmd_q_conj = quaternion_conjugate(hmd_q)
                            rel_p = rotate_vector_by_quaternion(world_diff, hmd_q_conj)
                        else:
                            # Controller not tracking
                            ctl_q = dummy_ctl_q
                            ctl_p = dummy_ctl_p
                            rel_p = dummy_rel_p
                    else:
                        # No controller
                        ctl_q = dummy_ctl_q
                        ctl_p = dummy_ctl_p
                        rel_p = dummy_rel_p
                    
                    timestamp = time.time()
                    self.send_pose(hmd_q, hmd_p, ctl_q, ctl_p, rel_p, timestamp)
                    
            time.sleep(1.0/120.0)  # 120 Hz update

if __name__ == "__main__":
    app = RawPosePipe()
    try:
        app.run()
    finally:
        app.shutdown() 