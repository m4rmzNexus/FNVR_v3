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

// VR Data
static VRDataPacket g_currentVRData = {};
static std::atomic<bool> g_hasNewData(false);
static CRITICAL_SECTION g_dataLock;

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
    
    // Search children
    for (UInt32 i = 0; i < root->m_children.m_size; i++) {
        NiAVObject* child = root->m_children.m_data[i];
        if (child && child->GetNiRTTI()->IsKindOf(NiRTTI_NiNode)) {
            NiNode* found = FindBone(static_cast<NiNode*>(child), boneName);
            if (found) return found;
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

// Pipe reading thread
void PipeThreadFunc() {
    Log("Pipe thread started");
    PipeClient pipeClient;
    
    while (!g_shouldStop) {
        if (!pipeClient.IsConnected()) {
            if (!pipeClient.Connect()) {
                Sleep(1000);
                continue;
            }
            Log("Pipe connected");
        }
        
        VRDataPacket data;
        if (pipeClient.Read(data)) {
            EnterCriticalSection(&g_dataLock);
            g_currentVRData = data;
            g_hasNewData = true;
            LeaveCriticalSection(&g_dataLock);
        } else {
            pipeClient.Disconnect();
            Sleep(100);
        }
    }
    
    Log("Pipe thread stopped");
}

// Apply VR data to skeleton
void ApplyVRDataToSkeleton() {
    PlayerCharacter* player = PlayerCharacter::GetSingleton();
    if (!player) return;
    
    // Get skeleton root
    NiNode* skeletonRoot = nullptr;
    if (!player->IsThirdPerson()) {
        // First person
        skeletonRoot = player->firstPerson->rootNode;
    } else {
        // Third person
        skeletonRoot = player->niNode;
    }
    
    if (!skeletonRoot) return;
    
    // Ensure bone cache is valid
    if (!g_boneCacheValid) {
        BuildBoneCache(skeletonRoot);
    }
    
    // Copy VR data
    VRDataPacket vrData;
    bool hasData = false;
    
    EnterCriticalSection(&g_dataLock);
    if (g_hasNewData) {
        vrData = g_currentVRData;
        hasData = true;
        g_hasNewData = false;
    }
    LeaveCriticalSection(&g_dataLock);
    
    if (!hasData) return;
    
    // Apply head tracking
    if (g_enableHeadTracking) {
        NiNode* headBone = FindBone(skeletonRoot, "Bip01 Head");
        if (headBone) {
            // Convert VR coordinates to game coordinates
            float gameX = vrData.hmd_pos.x * g_positionScale;
            float gameY = -vrData.hmd_pos.z * g_positionScale; // VR Z -> Game -Y
            float gameZ = vrData.hmd_pos.y * g_positionScale;  // VR Y -> Game Z
            
            // Apply position
            headBone->m_localTransform.pos.x = gameX;
            headBone->m_localTransform.pos.y = gameY;
            headBone->m_localTransform.pos.z = gameZ;
            
            // Apply rotation (quaternion to matrix)
            // TODO: Implement quaternion to matrix conversion
            
            // Update transforms
            headBone->Update(0.0f);
        }
    }
    
    // Apply hand tracking
    if (g_enableHandTracking) {
        // Right hand
        NiNode* rightHand = FindBone(skeletonRoot, "Bip01 R Hand");
        if (rightHand) {
            float gameX = vrData.controller_pos.x * g_positionScale;
            float gameY = -vrData.controller_pos.z * g_positionScale;
            float gameZ = vrData.controller_pos.y * g_positionScale;
            
            rightHand->m_localTransform.pos.x = gameX;
            rightHand->m_localTransform.pos.y = gameY;
            rightHand->m_localTransform.pos.z = gameZ;
            
            rightHand->Update(0.0f);
        }
        
        // Update weapon node if exists
        NiNode* weaponNode = FindBone(skeletonRoot, "Weapon");
        if (weaponNode) {
            weaponNode->Update(0.0f);
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