#!/usr/bin/env python3
"""
FNVR Settings GUI - Complete control over VR tracking
"""

import tkinter as tk
from tkinter import ttk, messagebox
import json
import os
import subprocess
import threading
import sys

class FNVRSettingsGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("FNVR Settings Control Panel")
        self.root.geometry("800x900")
        
        # Define the path to the development directory
        self.dev_path = os.path.dirname(__file__)
        
        # Settings file path
        self.settings_file = os.path.join(self.dev_path, "fnvr_settings.json")
        
        # Default settings - ensure all new keys are here
        self.default_settings = {
            "position_scale": 10.0,
            "rotation_scale": 30.0,
            "camera_offset_x": 0.0,  # Changed from 30.0 to 0.0 for true head position
            "camera_offset_y": 0.0,
            "camera_offset_z": 0.0,
            "rotation_offset_x": 0.0,
            "rotation_offset_y": 0.0,
            "rotation_offset_z": 0.0,
            "update_rate": 120,
            "debug_mode": False,
            "debug_frequency": 60,
            "invert_x": False,
            "invert_y": True,
            "invert_z": True,
            "invert_pitch": True,
            "invert_yaw": False,
            "invert_roll": False,
            "use_smoothing": False,
            "smoothing_factor": 0.1,
            "deadzone": 0.0,
            "max_reach": 100.0,
            "controller_offset_x": 0.0,
            "controller_offset_y": 0.0,
            "controller_offset_z": 0.0,
            "weapon_pitch_correction": 0.0,
            "weapon_yaw_correction": 0.0,
            "use_gesture_detection": False,
            "pipboy_gesture_threshold": 0.1,
            "pause_gesture_threshold": 0.1,
            "enable_hmd_position_tracking": False,
            "hmd_offset_x": 0.0,
            "hmd_offset_y": 0.0,
            "hmd_offset_z": 0.0,
            "use_alpha_scaling": False,
            "enable_gestures": False,
            "pipboy_gesture_enabled": False,
            "pipboy_gesture_coords": [0.12, 0.24, -0.29],
            "pause_gesture_enabled": False,
            "pause_gesture_coords": [-0.31, -0.18, -0.13],
            "gesture_activation_radius": 0.1,
            # HMD-specific settings
            "hmd_position_scale": 100.0,
            "hmd_pitch_offset": 0.0,
            "hmd_yaw_offset": 0.0,
            "hmd_roll_offset": 0.0,
            "apply_hmd_deadzone": False,
            "hmd_deadzone": 0.05,
            "apply_hmd_max_reach": False,
            "hmd_max_reach": 50.0,
            "apply_hmd_smoothing": False,
            "hmd_smoothing_factor": 0.5,
            "use_hmd_yaw_for_player": True,
            "player_yaw_scale": 1.0,
            # Controller filtering
            "apply_deadzone": False,
            "deadzone_position": 0.05,
            "deadzone_rotation": 0.05,
            "apply_max_reach": False,
            "max_reach_position": 50.0,
            "max_reach_rotation": 90.0,
            "apply_smoothing": False,
            "smoothing_factor_position": 0.5,
            "smoothing_factor_rotation": 0.5
        }
        
        self.settings = self.load_settings()
        self.tracker_process = None
        
        self.create_widgets()
        
    def load_settings(self):
        """Load settings from file or create default"""
        if os.path.exists(self.settings_file):
            with open(self.settings_file, 'r') as f:
                return json.load(f)
        return self.default_settings.copy()
    
    def save_settings(self):
        """Save current settings to file"""
        with open(self.settings_file, 'w') as f:
            json.dump(self.settings, f, indent=4)
        messagebox.showinfo("Success", "Settings saved!")
    
    def create_widgets(self):
        """Create all GUI elements"""
        
        # Create notebook for tabs
        notebook = ttk.Notebook(self.root)
        notebook.pack(fill='both', expand=True, padx=10, pady=10)
        
        # Tab 1: Basic Settings
        basic_frame = ttk.Frame(notebook)
        notebook.add(basic_frame, text="Basic Settings")
        self.create_basic_settings(basic_frame)
        
        # Tab 2: Advanced Settings
        advanced_frame = ttk.Frame(notebook)
        notebook.add(advanced_frame, text="Advanced Settings")
        self.create_advanced_settings(advanced_frame)
        
        # Tab 3: Experimental
        experimental_frame = ttk.Frame(notebook)
        notebook.add(experimental_frame, text="Experimental")
        self.create_experimental_settings(experimental_frame)
        
        # Tab 4: HMD Settings
        hmd_frame = ttk.Frame(notebook)
        notebook.add(hmd_frame, text="HMD/Camera")
        self.create_hmd_settings(hmd_frame)
        
        # Tab 5: Debug
        debug_frame = ttk.Frame(notebook)
        notebook.add(debug_frame, text="Debug")
        self.create_debug_settings(debug_frame)
        
        # Bottom control panel
        control_frame = ttk.Frame(self.root)
        control_frame.pack(fill='x', padx=10, pady=10)
        self.create_controls(control_frame)
        
    def create_basic_settings(self, parent):
        """Basic settings tab"""
        # Scaling mode selection
        mode_frame = ttk.LabelFrame(parent, text="Scaling Mode", padding=10)
        mode_frame.pack(fill='x', padx=10, pady=5)
        
        self.create_checkbox(mode_frame, "Use ALPHA Scaling (Original hardcoded values)", "use_alpha_scaling", 0, 0)
        ttk.Label(mode_frame, text="When enabled, uses the original ALPHA tracker scaling.", 
                 foreground="gray").grid(row=1, column=0, sticky='w', padx=20)
        
        frame = ttk.LabelFrame(parent, text="GUI Movement Scaling (when ALPHA mode is off)", padding=10)
        frame.pack(fill='x', padx=10, pady=5)
        
        # Position Scale
        self.create_slider(frame, "Position Scale", "position_scale", 
                          5.0, 50.0, 0, "Controls hand reach distance")
        
        # Rotation Scale
        self.create_slider(frame, "Rotation Scale", "rotation_scale", 
                          10.0, 120.0, 1, "Controls hand rotation sensitivity")
        
        # Camera Offset
        camera_frame = ttk.LabelFrame(parent, text="Camera Position", padding=10)
        camera_frame.pack(fill='x', padx=10, pady=5)
        
        self.create_slider(camera_frame, "Camera Distance (X)", "camera_offset_x", 
                          -50.0, 100.0, 0, "Move camera forward/back (0 = true head position)")
        self.create_slider(camera_frame, "Camera Height (Y)", "camera_offset_y", 
                          -50.0, 50.0, 1, "Move camera left/right")
        self.create_slider(camera_frame, "Camera Vertical (Z)", "camera_offset_z", 
                          -50.0, 50.0, 2, "Move camera up/down")
        
        # Update Rate
        rate_frame = ttk.LabelFrame(parent, text="Performance", padding=10)
        rate_frame.pack(fill='x', padx=10, pady=5)
        
        self.create_slider(rate_frame, "Update Rate (Hz)", "update_rate", 
                          30, 240, 0, "Higher = smoother but more CPU")
        
    def create_advanced_settings(self, parent):
        """Advanced settings tab"""
        
        # Inversion settings
        invert_frame = ttk.LabelFrame(parent, text="Axis Inversion", padding=10)
        invert_frame.pack(fill='x', padx=10, pady=5)
        
        inv_grid = ttk.Frame(invert_frame)
        inv_grid.pack()
        
        self.create_checkbox(inv_grid, "Invert X", "invert_x", 0, 0)
        self.create_checkbox(inv_grid, "Invert Y", "invert_y", 0, 1)
        self.create_checkbox(inv_grid, "Invert Z", "invert_z", 0, 2)
        self.create_checkbox(inv_grid, "Invert Pitch", "invert_pitch", 1, 0)
        self.create_checkbox(inv_grid, "Invert Yaw", "invert_yaw", 1, 1)
        self.create_checkbox(inv_grid, "Invert Roll", "invert_roll", 1, 2)
        
        # Rotation offsets
        rot_frame = ttk.LabelFrame(parent, text="Rotation Corrections", padding=10)
        rot_frame.pack(fill='x', padx=10, pady=5)
        
        self.create_slider(rot_frame, "Pitch Offset", "rotation_offset_x", 
                          -180.0, 180.0, 0, "Adjust weapon up/down angle")
        self.create_slider(rot_frame, "Yaw Offset", "rotation_offset_y", 
                          -180.0, 180.0, 1, "Adjust weapon left/right angle")
        self.create_slider(rot_frame, "Roll Offset", "rotation_offset_z", 
                          -180.0, 180.0, 2, "Adjust weapon twist angle")
        
        # Controller offsets
        ctrl_frame = ttk.LabelFrame(parent, text="Controller Position Offset", padding=10)
        ctrl_frame.pack(fill='x', padx=10, pady=5)
        
        self.create_slider(ctrl_frame, "X Offset", "controller_offset_x", 
                          -0.5, 0.5, 0, "Adjust controller X position")
        self.create_slider(ctrl_frame, "Y Offset", "controller_offset_y", 
                          -0.5, 0.5, 1, "Adjust controller Y position")
        self.create_slider(ctrl_frame, "Z Offset", "controller_offset_z", 
                          -0.5, 0.5, 2, "Adjust controller Z position")
        
    def create_experimental_settings(self, parent):
        """Experimental features tab"""
        
        # Controller Filtering
        filter_frame = ttk.LabelFrame(parent, text="Controller Motion Filtering", padding=10)
        filter_frame.pack(fill='x', padx=10, pady=5)
        
        # Deadzone
        dz_frame = ttk.Frame(filter_frame)
        dz_frame.pack(fill='x', pady=5)
        self.create_checkbox(dz_frame, "Apply Deadzone", "apply_deadzone", 0, 0)
        self.create_slider(filter_frame, "Position Deadzone", "deadzone_position", 
                          0.0, 0.2, 1, "Ignore small position changes")
        self.create_slider(filter_frame, "Rotation Deadzone", "deadzone_rotation", 
                          0.0, 0.2, 2, "Ignore small rotation changes")
        
        # Max reach
        mr_frame = ttk.Frame(filter_frame)
        mr_frame.pack(fill='x', pady=5)
        self.create_checkbox(mr_frame, "Apply Max Reach", "apply_max_reach", 0, 0)
        self.create_slider(filter_frame, "Position Max Reach", "max_reach_position", 
                          10.0, 100.0, 4, "Maximum hand distance")
        self.create_slider(filter_frame, "Rotation Max Reach", "max_reach_rotation", 
                          45.0, 180.0, 5, "Maximum rotation degrees")
        
        # Smoothing
        sm_frame = ttk.Frame(filter_frame)
        sm_frame.pack(fill='x', pady=5)
        self.create_checkbox(sm_frame, "Apply Smoothing", "apply_smoothing", 0, 0)
        self.create_slider(filter_frame, "Position Smoothing", "smoothing_factor_position", 
                          0.0, 1.0, 7, "Smooth position (0=none, 1=max)")
        self.create_slider(filter_frame, "Rotation Smoothing", "smoothing_factor_rotation", 
                          0.0, 1.0, 8, "Smooth rotation (0=none, 1=max)")
        
        # Weapon correction
        weapon_frame = ttk.LabelFrame(parent, text="Weapon Alignment", padding=10)
        weapon_frame.pack(fill='x', padx=10, pady=5)
        
        self.create_slider(weapon_frame, "Pitch Correction", "weapon_pitch_correction", 
                          -45.0, 45.0, 0, "Fine-tune weapon pitch")
        self.create_slider(weapon_frame, "Yaw Correction", "weapon_yaw_correction", 
                          -45.0, 45.0, 1, "Fine-tune weapon yaw")
        
        # Gestures
        gesture_frame = ttk.LabelFrame(parent, text="Gesture Detection (ALPHA Feature)", padding=10)
        gesture_frame.pack(fill='x', padx=10, pady=5)
        
        self.create_checkbox(gesture_frame, "Enable Gestures", "enable_gestures", 0, 0)
        
        # Pip-Boy gesture
        pb_frame = ttk.Frame(gesture_frame)
        pb_frame.pack(fill='x', pady=5)
        self.create_checkbox(pb_frame, "Enable Pip-Boy Gesture", "pipboy_gesture_enabled", 0, 0)
        self.create_slider(gesture_frame, "Gesture Activation Radius", "gesture_activation_radius", 
                          0.05, 0.3, 1, "Distance threshold for gesture activation")
        
        # Pause gesture
        pause_frame = ttk.Frame(gesture_frame)
        pause_frame.pack(fill='x', pady=5)
        self.create_checkbox(pause_frame, "Enable Pause Gesture", "pause_gesture_enabled", 0, 0)
        
        ttk.Label(gesture_frame, text="Note: Gesture coordinates are preconfigured from ALPHA tracker.", 
                 foreground="gray").pack(pady=5)
        
    def create_debug_settings(self, parent):
        """Debug settings tab"""
        
        debug_frame = ttk.LabelFrame(parent, text="Debug Options", padding=10)
        debug_frame.pack(fill='x', padx=10, pady=5)
        
        self.create_checkbox(debug_frame, "Enable Debug Output", "debug_mode", 0, 0)
        self.create_slider(debug_frame, "Debug Print Frequency", "debug_frequency", 
                          1, 120, 1, "Print every N frames")
        
        # Info display
        info_frame = ttk.LabelFrame(parent, text="Current Values", padding=10)
        info_frame.pack(fill='both', expand=True, padx=10, pady=5)
        
        self.info_text = tk.Text(info_frame, height=10, width=60)
        self.info_text.pack(fill='both', expand=True)
        
        ttk.Button(info_frame, text="Refresh Info", 
                   command=self.refresh_info).pack(pady=5)
        
    def create_hmd_settings(self, parent):
        """HMD/Camera specific settings"""
        
        # HMD tracking enable
        enable_frame = ttk.LabelFrame(parent, text="HMD Tracking", padding=10)
        enable_frame.pack(fill='x', padx=10, pady=5)
        
        self.create_checkbox(enable_frame, "Enable HMD Position Tracking", "enable_hmd_position_tracking", 0, 0)
        self.create_checkbox(enable_frame, "Use HMD Yaw for Player Body", "use_hmd_yaw_for_player", 1, 0)
        
        # HMD position
        pos_frame = ttk.LabelFrame(parent, text="HMD Position Adjustments", padding=10)
        pos_frame.pack(fill='x', padx=10, pady=5)
        
        self.create_slider(pos_frame, "HMD Position Scale", "hmd_position_scale", 
                          50.0, 200.0, 0, "Scale HMD movement (100 = 1:1)")
        self.create_slider(pos_frame, "HMD X Offset", "hmd_offset_x", 
                          -50.0, 50.0, 1, "Fine-tune camera X position")
        self.create_slider(pos_frame, "HMD Y Offset", "hmd_offset_y", 
                          -50.0, 50.0, 2, "Fine-tune camera Y position")
        self.create_slider(pos_frame, "HMD Z Offset", "hmd_offset_z", 
                          -50.0, 50.0, 3, "Fine-tune camera Z position")
        
        # HMD rotation
        rot_frame = ttk.LabelFrame(parent, text="HMD Rotation Adjustments", padding=10)
        rot_frame.pack(fill='x', padx=10, pady=5)
        
        self.create_slider(rot_frame, "HMD Pitch Offset", "hmd_pitch_offset", 
                          -45.0, 45.0, 0, "Adjust look up/down angle")
        self.create_slider(rot_frame, "HMD Yaw Offset", "hmd_yaw_offset", 
                          -45.0, 45.0, 1, "Adjust look left/right angle")
        self.create_slider(rot_frame, "HMD Roll Offset", "hmd_roll_offset", 
                          -45.0, 45.0, 2, "Adjust head tilt angle")
        self.create_slider(rot_frame, "Player Yaw Scale", "player_yaw_scale", 
                          0.0, 2.0, 3, "Scale player body rotation (1.0 = 1:1)")
        
        # HMD filtering
        filter_frame = ttk.LabelFrame(parent, text="HMD Motion Filtering", padding=10)
        filter_frame.pack(fill='x', padx=10, pady=5)
        
        # Deadzone
        dz_row = ttk.Frame(filter_frame)
        dz_row.pack(fill='x', pady=5)
        self.create_checkbox(dz_row, "Apply HMD Deadzone", "apply_hmd_deadzone", 0, 0)
        self.create_slider(filter_frame, "HMD Deadzone", "hmd_deadzone", 
                          0.0, 0.2, 1, "Ignore small HMD movements")
        
        # Max reach
        mr_row = ttk.Frame(filter_frame)
        mr_row.pack(fill='x', pady=5)
        self.create_checkbox(mr_row, "Apply HMD Max Reach", "apply_hmd_max_reach", 0, 0)
        self.create_slider(filter_frame, "HMD Max Reach", "hmd_max_reach", 
                          10.0, 100.0, 3, "Limit HMD movement range")
        
        # Smoothing
        sm_row = ttk.Frame(filter_frame)
        sm_row.pack(fill='x', pady=5)
        self.create_checkbox(sm_row, "Apply HMD Smoothing", "apply_hmd_smoothing", 0, 0)
        self.create_slider(filter_frame, "HMD Smoothing", "hmd_smoothing_factor", 
                          0.0, 1.0, 5, "Smooth HMD movements (0=none, 1=max)")
        
    def create_controls(self, parent):
        """Bottom control buttons"""
        ttk.Button(parent, text="Save Settings", 
                   command=self.save_settings).pack(side='left', padx=5)
        ttk.Button(parent, text="Load Defaults", 
                   command=self.load_defaults).pack(side='left', padx=5)
        
        ttk.Separator(parent, orient='vertical').pack(side='left', fill='y', padx=10)
        
        self.start_button = ttk.Button(parent, text="Start Tracker", 
                                       command=self.toggle_tracker)
        self.start_button.pack(side='left', padx=5)
        
        self.status_label = ttk.Label(parent, text="Tracker: Stopped")
        self.status_label.pack(side='left', padx=20)
        
    def create_slider(self, parent, label, key, min_val, max_val, row, tooltip=""):
        """Create a slider with label"""
        frame = ttk.Frame(parent)
        frame.grid(row=row, column=0, sticky='ew', pady=5)
        parent.grid_columnconfigure(0, weight=1)
        
        ttk.Label(frame, text=label).pack(side='left', padx=(0, 10))
        
        var = tk.DoubleVar(value=self.get_setting(key))
        slider = ttk.Scale(frame, from_=min_val, to=max_val, 
                          variable=var, orient='horizontal')
        slider.pack(side='left', fill='x', expand=True)
        
        value_label = ttk.Label(frame, text=f"{self.get_setting(key):.2f}")
        value_label.pack(side='left', padx=(10, 0))
        
        def update_value(val):
            self.settings[key] = float(val)
            value_label.config(text=f"{float(val):.2f}")
            
        slider.config(command=update_value)
        
        if tooltip:
            self.create_tooltip(slider, tooltip)
            
    def create_checkbox(self, parent, label, key, row, col):
        """Create a checkbox"""
        var = tk.BooleanVar(value=self.get_setting(key))
        cb = ttk.Checkbutton(parent, text=label, variable=var,
                            command=lambda: setattr(self.settings, key, var.get()))
        cb.grid(row=row, column=col, sticky='w', padx=10, pady=5)
        
        # Update settings when changed
        def update_setting():
            self.settings[key] = var.get()
        cb.config(command=update_setting)
        
    def create_tooltip(self, widget, text):
        """Create hover tooltip"""
        def on_enter(event):
            tooltip = tk.Toplevel()
            tooltip.wm_overrideredirect(True)
            tooltip.wm_geometry(f"+{event.x_root+10}+{event.y_root+10}")
            label = tk.Label(tooltip, text=text, background="yellow")
            label.pack()
            widget.tooltip = tooltip
            
        def on_leave(event):
            if hasattr(widget, 'tooltip'):
                widget.tooltip.destroy()
                
        widget.bind('<Enter>', on_enter)
        widget.bind('<Leave>', on_leave)
        
    def load_defaults(self):
        """Reset to default settings"""
        if messagebox.askyesno("Reset", "Reset all settings to defaults?"):
            self.settings = self.default_settings.copy()
            self.save_settings()
            self.root.destroy()
            root = tk.Tk()
            app = FNVRSettingsGUI(root)
            root.mainloop()
            
    def refresh_info(self):
        """Update debug info display"""
        info = "Current Settings:\n"
        info += "-" * 40 + "\n"
        for key, value in self.settings.items():
            info += f"{key}: {value}\n"
        
        self.info_text.delete(1.0, tk.END)
        self.info_text.insert(1.0, info)
        
    def toggle_tracker(self):
        """Start/stop the tracker"""
        if self.tracker_process is None:
            self.start_tracker()
        else:
            self.stop_tracker()
            
    def start_tracker(self):
        """Start the tracker with current settings"""
        # First save settings
        self.save_settings()
        
        tracker_script_path = os.path.join(self.dev_path, "FNVR_master_tracker.py")
        
        if not os.path.exists(tracker_script_path):
            messagebox.showerror("Error", f"Tracker script not found at:\n{tracker_script_path}")
            return

        # Start master tracker in background using the same python interpreter
        self.tracker_process = subprocess.Popen(
            [sys.executable, tracker_script_path],
            creationflags=subprocess.CREATE_NEW_CONSOLE
        )
        
        self.start_button.config(text="Stop Tracker")
        self.status_label.config(text="Tracker: Running")
        
    def stop_tracker(self):
        """Stop the tracker"""
        if self.tracker_process:
            self.tracker_process.terminate()
            self.tracker_process = None
            
        self.start_button.config(text="Start Tracker")
        self.status_label.config(text="Tracker: Stopped")
        
    def generate_tracker_script(self):
        """This function is no longer needed as we use a master script."""
        # This method can be removed or left empty.
        # It's kept here to avoid breaking old calls if any, but its body is gone.
        pass

    def get_setting(self, key):
        """Safely get a setting, falling back to default if key is missing."""
        return self.settings.get(key, self.default_settings.get(key))

if __name__ == "__main__":
    root = tk.Tk()
    app = FNVRSettingsGUI(root)
    root.mainloop()