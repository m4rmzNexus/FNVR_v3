// FNVR Plugin - Integrated Version
// Based on PluginMainComplete with module integration

#include <windows.h>
#include <cstdio>
#include <thread>
#include <atomic>
#include <cstring>
#include <vector>
#include <string>
#include <map>
#include <chrono>
#include <fstream>
#include <cmath>

// Include FNVR modules
#include "VRDataPacket.h"
#include "PipeClient.h"
#include "VRSystem.h"
#include "NVCSSkeleton.h"
#include "FirstPersonBodyFix.h"
#include "Globals.h"

// NVSE includes
#include "nvse/PluginAPI.h"
#include "nvse/CommandTable.h"
#include "nvse/GameAPI.h"
#include "nvse/GameObjects.h"
#include "nvse/GameRTTI.h"
#include "nvse/GameUI.h"

// Forward declarations
bool NVSEPlugin_Query(const NVSEInterface* nvse, PluginInfo* info);
bool NVSEPlugin_Load(const NVSEInterface* nvse);

// Global variables
static const char* g_pluginName = "FNVR";
static const UInt32 g_pluginVersion = 3;
static IDebugLog gLog("FNVR.log");
static NVSEMessagingInterface* g_messaging = nullptr;
static NVSEScriptInterface* g_script = nullptr;
static const NVSEInterface* g_nvse = nullptr;

// Thread control
static std::atomic<bool> g_shouldStop(false);
static std::thread* g_pipeThread = nullptr;
static std::thread* g_updateThread = nullptr;

// VR Data with thread safety
static VRDataPacket g_currentVRData = {};
static std::atomic<bool> g_hasNewData(false);
static CRITICAL_SECTION g_dataLock;

// Pipe connection state
static bool g_isPipeConnected = false;
static int g_pipeReconnectAttempts = 0;

// Bone cache for performance
struct BoneCache {
    NiNode* node;
    std::string name;
    NiTransform originalTransform;
};
static std::map<std::string, BoneCache> g_boneCache;
static bool g_boneCacheValid = false;

// Cached VorpX data
static HmdVector3_t g_cachedVorpxHeadPos = {0, 0, 0};

// Timing
static auto g_lastUpdateTime = std::chrono::steady_clock::now();
static const int UPDATE_INTERVAL_MS = 16; // ~60 FPS

// Global configuration variables
static float g_positionScale = 50.0f;
static bool g_enableHeadTracking = true;
static bool g_enableHandTracking = true;
static bool g_enableLogging = true;

// VorpX-specific params
static float g_vorpxScaleFactor = 1.0f;
static float g_vorpxLatencyOffset = 0.0f;
static HmdVector3_t g_poleVector = {0, 0, -1};

// Load configuration from INI
void LoadConfig() {
    char iniPath[MAX_PATH];
    GetModuleFileNameA(NULL, iniPath, MAX_PATH);
    strcpy(strrchr(iniPath, '\\'), "\\Data\\NVSE\\Plugins\\FNVR.ini");

    g_positionScale = (float)GetPrivateProfileIntA("General", "PositionScale", 50, iniPath);
    g_enableHeadTracking = GetPrivateProfileIntA("General", "EnableHeadTracking", 1, iniPath) != 0;
    g_enableHandTracking = GetPrivateProfileIntA("General", "EnableHandTracking", 1, iniPath) != 0;
    g_enableLogging = GetPrivateProfileIntA("General", "EnableLogging", 1, iniPath) != 0;

    // VorpX-specific params
    g_vorpxScaleFactor = (float)GetPrivateProfileIntA("VorpX", "ScaleFactor", 1, iniPath);
    g_vorpxLatencyOffset = (float)GetPrivateProfileIntA("VorpX", "LatencyOffset", 0, iniPath);
    g_poleVector.v[0] = (float)GetPrivateProfileIntA("IK", "PoleVectorX", 0, iniPath);
    g_poleVector.v[1] = (float)GetPrivateProfileIntA("IK", "PoleVectorY", 0, iniPath);
    g_poleVector.v[2] = (float)GetPrivateProfileIntA("IK", "PoleVectorZ", -1, iniPath);

    Log("Config loaded: PositionScale=%.1f, HeadTracking=%d, HandTracking=%d, Logging=%d, VorpXScale=%.1f, LatencyOffset=%.1f",
        g_positionScale, g_enableHeadTracking, g_enableHandTracking, g_enableLogging, g_vorpxScaleFactor, g_vorpxLatencyOffset);
}

// Safe memory access functions
template<typename T>
bool SafeRead(uintptr_t addr, T& out) {
    __try {
        out = *reinterpret_cast<T*>(addr);
        return true;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

template<typename T>
bool SafeWrite(uintptr_t addr, const T& value) {
    __try {
        *reinterpret_cast<T*>(addr) = value;
        return true;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Logging functions
void Log(const char* fmt, ...) {
    if (!g_enableLogging) return;
    
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    // Add timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    char timeStr[64];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", localtime(&time_t));
    
    // Write to file
    std::ofstream logFile("Data\\NVSE\\Plugins\\FNVR_debug.log", std::ios::app);
    if (logFile.is_open()) {
        logFile << "[" << timeStr << "." << ms.count() << "] " << buffer << std::endl;
        logFile.close();
    }
    
    _MESSAGE("%s", buffer);
}

// Find bone recursively with cache
NiNode* FindBone(NiNode* root, const char* boneName) {
    if (!root || !boneName) return nullptr;
    
    // Check cache first
    auto it = g_boneCache.find(boneName);
    if (it != g_boneCache.end() && g_boneCacheValid) {
        return it->second.node;
    }
    
    // Check current node
    if (root->m_pcName && _stricmp(root->m_pcName, boneName) == 0) {
        // Add to cache
        BoneCache cache;
        cache.node = root;
        cache.name = boneName;
        cache.originalTransform = root->m_localTransform;
        g_boneCache[boneName] = cache;
        return root;
    }
    
    // Search children with safety checks
    if (root->m_children.m_data) {
        for (UInt32 i = 0; i < root->m_children.m_size; i++) {
            if (i >= root->m_children.m_uiMaxSize) {
                Log("FindBone: Array bounds warning (i=%d, max=%d)", i, root->m_children.m_uiMaxSize);
                break;
            }
            
            NiAVObject* child = root->m_children.m_data[i];
            if (child && child->GetNiRTTI() && child->GetNiRTTI()->IsKindOf(NiRTTI_NiNode)) {
                NiNode* found = FindBone(static_cast<NiNode*>(child), boneName);
                if (found) return found;
            }
        }
    }
    
    return nullptr;
}

// Clear bone cache
void ClearBoneCache() {
    g_boneCache.clear();
    g_boneCacheValid = false;
}

// Build bone cache for all NVCS bones
void BuildBoneCache(NiNode* root) {
    if (!root) return;
    
    Log("Building bone cache...");
    ClearBoneCache();
    
    const char* importantBones[] = {
        "Bip01", "Bip01 Head", "Bip01 Neck", "Bip01 Neck1",
        "Bip01 R Hand", "Bip01 L Hand", 
        "Bip01 R Forearm", "Bip01 L Forearm",
        "Bip01 R UpperArm", "Bip01 L UpperArm",
        "Weapon", "ProjectileNode"
    };
    
    for (const char* boneName : importantBones) {
        FindBone(root, boneName);
    }
    
    g_boneCacheValid = true;
    Log("Bone cache built with %d bones", g_boneCache.size());
}

// Thread-safe pipe reading thread with error handling
void PipeThreadFunc() {
    Log("Pipe thread started");
    PipeClient pipeClient("\\\\.\\pipe\\FNVRTracker");
    
    while (!g_shouldStop) {
        // Connection management with retry logic
        if (!pipeClient.IsConnected()) {
            g_isPipeConnected = false;
            
            if (!pipeClient.Connect()) {
                g_pipeReconnectAttempts++;
                
                // Log every 10 attempts to avoid spam
                if (g_pipeReconnectAttempts % 10 == 1) {
                    Log("Pipe connection attempt %d failed, retrying...", g_pipeReconnectAttempts);
                }
                
                Sleep(1000); // Wait 1 second before retry
                continue;
            }
            
            // Connection successful
            g_isPipeConnected = true;
            g_pipeReconnectAttempts = 0;
            Log("Pipe connected successfully");
        }
        
        // Read data with error handling
        VRDataPacket data;
        if (pipeClient.Read(data)) {
            // Validate data before storing
            bool dataValid = true;
            
            // Basic validation: check if quaternions are normalized
            float hmdQLen = data.hmd_qw*data.hmd_qw + data.hmd_qx*data.hmd_qx + 
                           data.hmd_qy*data.hmd_qy + data.hmd_qz*data.hmd_qz;
            if (hmdQLen < 0.9f || hmdQLen > 1.1f) {
                Log("Warning: Invalid HMD quaternion length: %.3f", hmdQLen);
                dataValid = false;
            }
            
            if (dataValid) {
                // Thread-safe data update
                EnterCriticalSection(&g_dataLock);
                g_currentVRData = data;
                g_hasNewData = true;
                LeaveCriticalSection(&g_dataLock);
            }
        } else {
            // Read failed - connection lost
            Log("Pipe read failed, disconnecting");
            pipeClient.Disconnect();
            g_isPipeConnected = false;
            Sleep(100); // Brief pause before reconnection attempt
        }
    }
    
    // Cleanup
    pipeClient.Disconnect();
    g_isPipeConnected = false;
    Log("Pipe thread stopped");
}

// Check if game is in a safe state for VR updates
bool IsGameStateValid() {
    // Check if we're in the main game world, not in menus or loading
    
    // Method 1: Check common menu states
    InterfaceManager* im = InterfaceManager::GetSingleton();
    if (!im) {
        Log("Game state check: InterfaceManager not available");
        return false;
    }
    
    // Check for main menu
    if (im->menuMode) {
        Log("Game state check: Menu mode active (menuMode=%d)", im->menuMode);
        return false;
    }
    
    // Additional safety: check if any menu is open
    if (im->activeMenu) {
        Log("Game state check: Active menu detected");
        return false;
    }
    
    // Check console state
    if (ConsoleManager::GetSingleton() && ConsoleManager::GetSingleton()->IsConsoleOpen()) {
        Log("Game state check: Console is open");
        return false;
    }
    
    return true;
}

// Apply VR data to skeleton with comprehensive safety checks
void ApplyVRDataToSkeleton() {
    // === STAGE 1: Game State Validation ===
    if (!IsGameStateValid()) {
        return; // Not safe to update
    }
    
    // === STAGE 2: Player Validation ===
    PlayerCharacter* player = PlayerCharacter::GetSingleton();
    if (!player) {
        Log("Safety check failed: player is null");
        return;
    }
    
    // Check if player is dead
    if (player->GetDead()) {
        Log("Safety check: player is dead, skipping updates");
        return;
    }
    
    // Check if player has a valid parent cell (is in world)
    if (!player->parentCell) {
        Log("Safety check failed: player has no parent cell");
        return;
    }
    
    // Check if player has process data
    if (!player->process) {
        Log("Safety check failed: player->process is null");
        return;
    }
    
    // === STAGE 3: Get Skeleton Root with Safety ===
    NiNode* skeletonRoot = nullptr;
    
    if (!player->IsThirdPerson()) {
        // First person - check if firstPerson exists
        if (player->firstPerson && player->firstPerson->rootNode) {
            skeletonRoot = player->firstPerson->rootNode;
        } else {
            Log("Safety check: firstPerson or its rootNode is null");
            return;
        }
    } else {
        // Third person
        if (player->niNode) {
            skeletonRoot = player->niNode;
        } else {
            Log("Safety check: player->niNode is null");
            return;
        }
    }
    
    if (!skeletonRoot) {
        Log("Safety check failed: skeletonRoot is null after checks");
        return;
    }
    
    // Ensure bone cache is valid
    if (!g_boneCacheValid) {
        BuildBoneCache(skeletonRoot);
    }
    
    // === STAGE 4: Copy VR Data Thread-Safely ===
    VRDataPacket vrData;
    bool hasData = false;
    
    EnterCriticalSection(&g_dataLock);
    if (g_hasNewData) {
        vrData = g_currentVRData;
        hasData = true;
        g_hasNewData = false;
        
        // Debug: Log data reception occasionally
        static int dataFrameCount = 0;
        if (++dataFrameCount % 300 == 0) { // Every 5 seconds at 60fps
            Log("VR data received: HMD pos(%.2f,%.2f,%.2f) rot(%.2f,%.2f,%.2f,%.2f)",
                vrData.hmd_px, vrData.hmd_py, vrData.hmd_pz,
                vrData.hmd_qw, vrData.hmd_qx, vrData.hmd_qy, vrData.hmd_qz);
        }
    }
    LeaveCriticalSection(&g_dataLock);
    
    if (!hasData) {
        static int noDataCount = 0;
        if (++noDataCount % 600 == 0) { // Log every 10 seconds
            Log("Warning: No new VR data available (pipe connected: %s)", 
                g_isPipeConnected ? "yes" : "no");
        }
        return;
    }
    
    // Apply head tracking with safety checks
    if (g_enableHeadTracking) {
        NiNode* headBone = FindBone(skeletonRoot, "Bip01 Head");
        if (!headBone) {
            Log("Warning: Could not find Bip01 Head bone");
        } else {
            // Additional validation before modifying
            if (!headBone->m_parent) {
                Log("Warning: Head bone has no parent, skipping");
                return;
            }
            // Convert VR coordinates to game coordinates using proper transformation
            // OpenVR: Right-handed, Y-up, -Z forward (meters)
            // Gamebryo: Left-handed, Z-up, Y forward (game units)
            
            // Position conversion
            float gameX =  vrData.hmd_px * g_positionScale;  // X -> X
            float gameY = -vrData.hmd_pz * g_positionScale;  // -Z -> Y
            float gameZ =  vrData.hmd_py * g_positionScale;  // Y -> Z
            
            // Apply position offsets from config
            headBone->m_localTransform.pos.x = gameX + g_positionOffsetX;
            headBone->m_localTransform.pos.y = gameY + g_positionOffsetY;
            headBone->m_localTransform.pos.z = gameZ + g_positionOffsetZ;
            
            // Quaternion conversion for rotation
            // OpenVR to Gamebryo requires -90 degree rotation around X axis
            const float r_sqrt2_inv = 0.7071067811865476f; // 1/sqrt(2)
            
            float game_qw = (vrData.hmd_qw + vrData.hmd_qx) * r_sqrt2_inv;
            float game_qx = (vrData.hmd_qx - vrData.hmd_qw) * r_sqrt2_inv;
            float game_qy = (vrData.hmd_qy + vrData.hmd_qz) * r_sqrt2_inv;
            float game_qz = (vrData.hmd_qz - vrData.hmd_qy) * r_sqrt2_inv;
            
            // Normalize quaternion
            float mag = sqrtf(game_qw*game_qw + game_qx*game_qx + game_qy*game_qy + game_qz*game_qz);
            if (mag > 0.0f) {
                float invMag = 1.0f / mag;
                game_qw *= invMag;
                game_qx *= invMag;
                game_qy *= invMag;
                game_qz *= invMag;
            }
            
            // Convert quaternion to rotation matrix for NiTransform
            // Using standard quaternion to matrix conversion
            float xx = game_qx * game_qx;
            float yy = game_qy * game_qy;
            float zz = game_qz * game_qz;
            float xy = game_qx * game_qy;
            float xz = game_qx * game_qz;
            float yz = game_qy * game_qz;
            float wx = game_qw * game_qx;
            float wy = game_qw * game_qy;
            float wz = game_qw * game_qz;
            
            headBone->m_localTransform.rot.data[0][0] = 1.0f - 2.0f * (yy + zz);
            headBone->m_localTransform.rot.data[0][1] = 2.0f * (xy - wz);
            headBone->m_localTransform.rot.data[0][2] = 2.0f * (xz + wy);
            
            headBone->m_localTransform.rot.data[1][0] = 2.0f * (xy + wz);
            headBone->m_localTransform.rot.data[1][1] = 1.0f - 2.0f * (xx + zz);
            headBone->m_localTransform.rot.data[1][2] = 2.0f * (yz - wx);
            
            headBone->m_localTransform.rot.data[2][0] = 2.0f * (xz - wy);
            headBone->m_localTransform.rot.data[2][1] = 2.0f * (yz + wx);
            headBone->m_localTransform.rot.data[2][2] = 1.0f - 2.0f * (xx + yy);
            
            // Update transforms
            headBone->Update(0.0f);
        }
    }
    
    // Apply hand tracking with safety checks
    if (g_enableHandTracking) {
        // Right hand with comprehensive validation
        NiNode* rightHand = FindBone(skeletonRoot, "Bip01 R Hand");
        if (!rightHand) {
            Log("Warning: Could not find Bip01 R Hand bone");
        } else {
            // Validate bone before manipulation
            if (!rightHand->m_parent) {
                Log("Warning: Right hand bone has no parent, skipping");
                return;
            }
            // Convert VR controller coordinates to game coordinates
            // Same transformation as HMD
            float gameX =  vrData.right_px * g_positionScale;  // X -> X
            float gameY = -vrData.right_pz * g_positionScale;  // -Z -> Y
            float gameZ =  vrData.right_py * g_positionScale;  // Y -> Z
            
            // Apply position with offsets
            rightHand->m_localTransform.pos.x = gameX + g_handOffsetX;
            rightHand->m_localTransform.pos.y = gameY + g_handOffsetY;
            rightHand->m_localTransform.pos.z = gameZ + g_handOffsetZ;
            
            // Quaternion conversion for rotation
            const float r_sqrt2_inv = 0.7071067811865476f;
            
            float game_qw = (vrData.right_qw + vrData.right_qx) * r_sqrt2_inv;
            float game_qx = (vrData.right_qx - vrData.right_qw) * r_sqrt2_inv;
            float game_qy = (vrData.right_qy + vrData.right_qz) * r_sqrt2_inv;
            float game_qz = (vrData.right_qz - vrData.right_qy) * r_sqrt2_inv;
            
            // Normalize
            float mag = sqrtf(game_qw*game_qw + game_qx*game_qx + game_qy*game_qy + game_qz*game_qz);
            if (mag > 0.0f) {
                float invMag = 1.0f / mag;
                game_qw *= invMag;
                game_qx *= invMag;
                game_qy *= invMag;
                game_qz *= invMag;
            }
            
            // Convert quaternion to rotation matrix
            float xx = game_qx * game_qx;
            float yy = game_qy * game_qy;
            float zz = game_qz * game_qz;
            float xy = game_qx * game_qy;
            float xz = game_qx * game_qz;
            float yz = game_qy * game_qz;
            float wx = game_qw * game_qx;
            float wy = game_qw * game_qy;
            float wz = game_qw * game_qz;
            
            rightHand->m_localTransform.rot.data[0][0] = 1.0f - 2.0f * (yy + zz);
            rightHand->m_localTransform.rot.data[0][1] = 2.0f * (xy - wz);
            rightHand->m_localTransform.rot.data[0][2] = 2.0f * (xz + wy);
            
            rightHand->m_localTransform.rot.data[1][0] = 2.0f * (xy + wz);
            rightHand->m_localTransform.rot.data[1][1] = 1.0f - 2.0f * (xx + zz);
            rightHand->m_localTransform.rot.data[1][2] = 2.0f * (yz - wx);
            
            rightHand->m_localTransform.rot.data[2][0] = 2.0f * (xz - wy);
            rightHand->m_localTransform.rot.data[2][1] = 2.0f * (yz + wx);
            rightHand->m_localTransform.rot.data[2][2] = 1.0f - 2.0f * (xx + yy);
            
            rightHand->Update(0.0f);
        }
        
        // Update weapon node with safety check
        NiNode* weaponNode = FindBone(skeletonRoot, "Weapon");
        if (weaponNode && weaponNode->m_parent) {
            weaponNode->Update(0.0f);
        } else if (weaponNode) {
            Log("Warning: Weapon node exists but has no parent");
        }
    }
    
    // Update global variables
    FNVR::Globals::UpdateGlobals(vrData);
}

// Update thread (60 FPS)
void UpdateThreadFunc() {
    Log("Update thread started");
    
    while (!g_shouldStop) {
        auto startTime = std::chrono::high_resolution_clock::now();
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_lastUpdateTime).count();
        
        if (elapsed >= UPDATE_INTERVAL_MS) {
            // VorpX mode check
            if (g_vorpxMode) {
                // Sync with assumed VorpX frame rate (e.g., 90 Hz)
                const int VORPX_INTERVAL_MS = 11;  // ~90 Hz
                if (elapsed < VORPX_INTERVAL_MS) continue;  // Skip if not time
                
                // Use cached VorpX head data (from memory scan in NVCSSkeleton)
                // Assume updated elsewhere; for PoC, simulate
                g_cachedVorpxHeadPos.v[0] += 0.1f;  // Dummy update
            }
            
            ApplyVRDataToSkeleton();
            
            // Fix first person body visibility
            FNVR::FirstPersonBodyFix::Update();
            
            g_lastUpdateTime = now;
        }
        
        // Profiling: Log duration if in debug mode
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
        if (g_enableLogging && duration > 5000) {  // Warn if >5ms
            Log("Update took %ld us", duration);
        }
        
        Sleep(1);
    }
    
    Log("Update thread stopped");
}

// Message handler
void MessageHandler(NVSEMessagingInterface::Message* msg) {
    switch (msg->type) {
        case NVSEMessagingInterface::kMessage_PostPostLoad:
            Log("PostPostLoad message received");
            // Initialize after all plugins loaded
            break;
            
        case NVSEMessagingInterface::kMessage_MainGameLoop:
            // Alternative to thread-based updates
            // ApplyVRDataToSkeleton();
            break;
            
        case NVSEMessagingInterface::kMessage_ExitGame:
            Log("Game exiting");
            g_shouldStop = true;
            break;
    }
}

// Plugin query
bool NVSEPlugin_Query(const NVSEInterface* nvse, PluginInfo* info) {
    // Fill out the plugin info structure
    info->infoVersion = PluginInfo::kInfoVersion;
    info->name = g_pluginName;
    info->version = g_pluginVersion;
    
    // Version check
    if (nvse->nvseVersion < NVSE_VERSION_INTEGER) {
        _ERROR("NVSE version too old (got %08X expected at least %08X)", nvse->nvseVersion, NVSE_VERSION_INTEGER);
        return false;
    }
    
    if (!nvse->isEditor) {
        if (nvse->runtimeVersion < RUNTIME_VERSION_1_4_0_525) {
            _ERROR("incorrect runtime version (got %08X need at least %08X)", nvse->runtimeVersion, RUNTIME_VERSION_1_4_0_525);
            return false;
        }
    }
    
    // Version checks passed
    return true;
}

// Plugin load
bool NVSEPlugin_Load(const NVSEInterface* nvse) {
    g_nvse = nvse;
    
    // Open log
    if (!nvse->isEditor) {
        gLog.Open("Data\\NVSE\\Plugins\\FNVR.log");
        Log("FNVR Plugin v%d loading...", g_pluginVersion);
    }
    
    // Load configuration
    LoadConfig();
    
    // Get interfaces
    g_messaging = (NVSEMessagingInterface*)nvse->QueryInterface(kInterface_Messaging);
    g_script = (NVSEScriptInterface*)nvse->QueryInterface(kInterface_Script);
    
    if (!g_messaging) {
        Log("ERROR: Couldn't get messaging interface");
        return false;
    }
    
    // Register message handler
    g_messaging->RegisterListener(nvse->GetPluginHandle(), "NVSE", MessageHandler);
    
    // Initialize critical section
    InitializeCriticalSection(&g_dataLock);
    
    // Initialize modules
    FNVR::VRSystem::Initialize();
    FNVR::Globals::Initialize();
    
    // Start threads
    g_shouldStop = false;
    g_pipeThread = new std::thread(PipeThreadFunc);
    g_updateThread = new std::thread(UpdateThreadFunc);
    
    Log("FNVR Plugin loaded successfully");
    return true;
}

// DLL entry point
extern "C" {
    BOOL WINAPI DllMain(HANDLE hDllHandle, DWORD dwReason, LPVOID lpreserved) {
        if (dwReason == DLL_PROCESS_DETACH) {
            // Cleanup
            g_shouldStop = true;
            
            if (g_pipeThread) {
                g_pipeThread->join();
                delete g_pipeThread;
            }
            
            if (g_updateThread) {
                g_updateThread->join();
                delete g_updateThread;
            }
            
            DeleteCriticalSection(&g_dataLock);
        }
        return TRUE;
    }
}