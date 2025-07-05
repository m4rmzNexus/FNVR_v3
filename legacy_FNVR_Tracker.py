import math
import keyboard
import openvr
import time
import ctypes
import numpy as np


# Replace with your own config filepath.
file_path = 'E:/SteamLibrary/steamapps/common/Fallout New Vegas/Data/Config/' + 'Meh.ini'

def update_ini(iX, iY, iZ, iXr, iYr, iZr, pXr, pYr, pZr):
    try:
        with open(file_path, "w") as f:
            f.write("[Standard]\n")
            f.write(f'fCanIOpenThis = {1}\n')
            f.write(f"fiX = {(iX*50) + 15:.4f}\n")
            f.write(f"fiY = {(iY*-50) + -10:.4f}\n")
            f.write(f"fiZ = {(iZ*-50) + 0:.4f}\n")
            f.write(f"fiXr = {(iXr * -120) + 10:.4f}\n")
            f.write(f"fiZr = {(iZr * 120) + -75:.4f}\n")
            #f.write(f"fiYr = {(iYr * 0) + 0:.4f}\n")
            #f.write(f"fpXr = {(pXr * 0) + 0:.4f}\n")
            #f.write(f"fpYr = {(pYr * 0) + 0:.4f}\n")
            f.write(f"fpZr = {(pZr * -150) - 7.5:.4f}\n")
        #print('INI updated')
    except Exception as e:
        print('Error updating INI:', e)

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
        try:
            # Initialize OpenVR as a background application
            self.vr_system = openvr.init(openvr.VRApplication_Background)
            print("OpenVR initialized successfully.")
        except openvr.OpenVRError as e:
            print(f"Error initializing OpenVR: {e}")
            self.shutdown()
            exit()

    def shutdown(self):
        if self.vr_system is not None:
            openvr.shutdown()
            self.vr_system = None
            print("OpenVR shutdown complete.")

    def run(self):
        if self.vr_system is None:
            print("OpenVR not initialized. Cannot run tracking.")
            return

        hmd_index = openvr.k_unTrackedDeviceIndex_Hmd
        con_index = openvr.TrackedDeviceClass_Controller
        max_devices = openvr.k_unMaxTrackedDeviceCount
        TrackedDevicePose_t = openvr.TrackedDevicePose_t

        while True:
            # Check if HMD is connected
            if not self.vr_system.isTrackedDeviceConnected(hmd_index):
                print("HMD not connected.")
                time.sleep(1)
                continue

            origin = openvr.TrackingUniverseStanding

            predicted_seconds = 0.0

            poses_array = (TrackedDevicePose_t * max_devices)()

            returned_poses = self.vr_system.getDeviceToAbsoluteTrackingPose(
                origin, predicted_seconds, poses_array
            )

            if len(returned_poses) > hmd_index:
                hmd_pose = returned_poses[hmd_index]
                con_pose = returned_poses[con_index]

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

                    #print(f"Relative Position (HMD as origin): x: {relative_position.v[0]:.4f}, y: {relative_position.v[1]:.4f}, z: {relative_position.v[2]:.4f}")
                   # print(f"Relative Rotation (HMD as origin): w: {relative_rotation.w:.4f}, x: {relative_rotation.x:.4f}, y: {relative_rotation.y:.4f}, z: {relative_rotation.z:.4f}")
                    #print(f"HMD Rotation (World): w: {hmd_rotation_world.w:.4f}, x: {hmd_rotation_world.x:.4f}, y: {hmd_rotation_world.y:.4f}, z: {hmd_rotation_world.z:.4f}")

                    # InertiaController Bone Pose is set to match the controller's position.
                    inertiaZ, inertiaX, inertiaY = relative_position.v

                    # InertiaController Bone Rotation is set to match the controller's position.
                    inertiaZr, inertiaXr, inertiaYr = relative_rotation.x, relative_rotation.y, relative_rotation.z

                    # playerAngle is set to match the HMD's rotation minus the changes in rotation caused by nAccController Bone.
                    playerXr, playerZr, playerYr = hmd_rotation_world.x, hmd_rotation_world.y, hmd_rotation_world.z

                    # Finally, update the ini with the final values.
                    update_ini(inertiaX, inertiaY, inertiaZ, inertiaXr, inertiaYr, inertiaZr, inertiaZr, playerYr, -inertiaXr)

                    if keyboard.is_pressed('Tab'):

                        print(f"Con Pos: x: {relative_position.v[0]:.4f}, y: {relative_position.v[1]:.4f}, z: {relative_position.v[2]:.4f}")
                        print(f"Con Rot: w: {relative_rotation.w:.4f}, x: {relative_rotation.x:.4f}, y: {relative_rotation.y:.4f}, z: {relative_rotation.z:.4f}")

                    # Con Pos for accurate pipboy reading
                    #06 -24 -48
                    distance = calculate_distance_xyz(relative_position.v[0], relative_position.v[1], relative_position.v[2], 0.12, 0.24, -0.29)
                    if distance < 0.1:
                        update_ini(-0.1615, -0.5, 0.1281, 0.0655, 0.041, 0.6291, playerXr, playerYr, -inertiaXr)
                        keyboard.press('Tab')
                        time.sleep(0.05)
                        keyboard.release('Tab')
                        update_ini(-0.1615, -0.5, 0.1281, 0.0655, 0.041, 0.6291, playerXr, playerYr, -inertiaXr)

                    # Controller gesture for opening the pause menu:
                    distance = calculate_distance_xyz(relative_position.v[0], relative_position.v[1], relative_position.v[2], -0.3158, -0.1897, -0.1316)
                    if distance < 0.1:
                        print('Escaoe')
                        keyboard.press('Escape')
                        time.sleep(0.75)
                        keyboard.release('Escape')

                    # Var that gets set to 1 inside location statement, then checks if one above it then set


                    #   print(f"The distance between the two points is: {distance}")

                    #update_ini(relative_position.v[1]*35+15, -relative_position.v[2]*35, -relative_position.v[0]*35+8, -relative_rotation.y*75 + 10, 1*relative_rotation.z*25, -relative_rotation.w*75 + 55, hmd_rotation_world.w, hmd_rotation_world.x, hmd_rotation_world.y, hmd_rotation_world.z)
                    #update_ini(relative_position.v[1]*50 + 7, -relative_position.v[2]*30 + 25, -relative_position.v[0]*30, -relative_rotation.y*45, relative_rotation.z*30, -relative_rotation.w*45 + 35)
                else:
                    print("Invalid HMD pose.")
            else:
                print("Could not retrieve HMD pose.")

            time.sleep(1.5/60)

if __name__ == "__main__":
    tracking_app = SimpleTrackingApp()
    try:
        tracking_app.run()
    finally:
        tracking_app.shutdown()
