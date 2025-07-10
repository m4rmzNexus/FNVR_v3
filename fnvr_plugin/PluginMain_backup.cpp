// FNVR Plugin - Safe minimal version for debugging
#include <windows.h>
#include <cstdio>
#include <cstring>

// NVSE types
typedef unsigned long UInt32;
typedef unsigned short UInt16;
typedef unsigned char UInt8;

// Plugin structures
struct PluginInfo {
    UInt32 infoVersion;
    const char* name;
    UInt32 version;
};

struct NVSEInterface {
    UInt32 nvseVersion;
    UInt32 runtimeVersion;
    UInt32 editorVersion;
    UInt32 isEditor;
    void* (*QueryInterface)(UInt32 id);
    UInt32 (*GetPluginHandle)(void);
};

struct NVSEScriptInterface {
    UInt32 version;
    bool (*RunScriptLine)(const char* text);
};

// VR Data packet
#pragma pack(push, 1)
struct VRDataPacket {
    int version;
    float hmd_qw, hmd_qx, hmd_qy, hmd_qz;
    float hmd_px, hmd_py, hmd_pz;
    float ctl_qw, ctl_qx, ctl_qy, ctl_qz;
    float ctl_px, ctl_py, ctl_pz;
    float rel_px, rel_py, rel_pz;
    double timestamp;
};
#pragma pack(pop)

// Globals
static FILE* g_logFile = NULL;
static HANDLE g_pipeThread = NULL;
static bool g_shouldStop = false;
static NVSEScriptInterface* g_script = NULL;
static const char* g_pipeName = "\\\\.\\pipe\\FNVRTracker";

// Player singleton address
static void** g_playerSingleton = (void**)0x11DEA3C;

// Simple logging
void Log(const char* fmt, ...) {
    if (!g_logFile) return;
    
    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(g_logFile, "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    
    va_list args;
    va_start(args, fmt);
    vfprintf(g_logFile, fmt, args);
    va_end(args);
    
    fprintf(g_logFile, "\n");
    fflush(g_logFile);
}

// Test player access
void TestPlayerAccess() {
    Log("=== Testing Player Access ===");
    
    // Check player singleton pointer
    Log("Player singleton address: 0x%08X", (UInt32)g_playerSingleton);
    
    if (IsBadReadPtr(g_playerSingleton, sizeof(void*))) {
        Log("ERROR: Cannot read player singleton pointer!");
        return;
    }
    
    void* player = *g_playerSingleton;
    Log("Player pointer value: 0x%08X", (UInt32)player);
    
    if (!player) {
        Log("Player pointer is NULL");
        return;
    }
    
    if (IsBadReadPtr(player, 0x1000)) {
        Log("ERROR: Player pointer is invalid!");
        return;
    }
    
    // Try to read refID at offset 0x08
    UInt32* refIDPtr = (UInt32*)((char*)player + 0x08);
    if (!IsBadReadPtr(refIDPtr, sizeof(UInt32))) {
        UInt32 refID = *refIDPtr;
        Log("Player refID at offset 0x08: 0x%08X", refID);
    }
    
    // Try to read refID at offset 0x0C
    refIDPtr = (UInt32*)((char*)player + 0x0C);
    if (!IsBadReadPtr(refIDPtr, sizeof(UInt32))) {
        UInt32 refID = *refIDPtr;
        Log("Player refID at offset 0x0C: 0x%08X", refID);
    }
    
    // Test some other offsets
    Log("Testing render state offset 0x64...");
    void** renderStatePtr = (void**)((char*)player + 0x64);
    if (!IsBadReadPtr(renderStatePtr, sizeof(void*))) {
        void* renderState = *renderStatePtr;
        Log("RenderState pointer: 0x%08X", (UInt32)renderState);
        
        if (renderState && !IsBadReadPtr(renderState, 0x20)) {
            void** rootNodePtr = (void**)((char*)renderState + 0x14);
            if (!IsBadReadPtr(rootNodePtr, sizeof(void*))) {
                void* rootNode = *rootNodePtr;
                Log("RootNode pointer: 0x%08X", (UInt32)rootNode);
            }
        }
    }
    
    Log("Testing first person node offset 0x694...");
    void** fpNodePtr = (void**)((char*)player + 0x694);
    if (!IsBadReadPtr(fpNodePtr, sizeof(void*))) {
        void* fpNode = *fpNodePtr;
        Log("FirstPerson node pointer: 0x%08X", (UInt32)fpNode);
    }
    
    Log("=== End Player Test ===");
}

// Pipe thread
DWORD WINAPI PipeThreadProc(LPVOID lpParam) {
    Log("Pipe thread started");
    Sleep(5000); // 5 second startup delay
    
    int connectAttempts = 0;
    
    while (!g_shouldStop) {
        Log("Pipe connection attempt %d", ++connectAttempts);
        
        HANDLE hPipe = CreateFileA(
            g_pipeName,
            GENERIC_READ,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );
        
        if (hPipe != INVALID_HANDLE_VALUE) {
            Log("Connected to pipe!");
            
            int packetCount = 0;
            while (!g_shouldStop) {
                VRDataPacket data = {0};
                DWORD bytesRead = 0;
                
                if (!ReadFile(hPipe, &data, sizeof(VRDataPacket), &bytesRead, NULL) || 
                    bytesRead != sizeof(VRDataPacket)) {
                    Log("Pipe read failed");
                    break;
                }
                
                if (++packetCount <= 5 || packetCount % 100 == 0) {
                    Log("Packet %d: HMD pos=(%.2f,%.2f,%.2f) ctrl=(%.2f,%.2f,%.2f)", 
                        packetCount, data.hmd_px, data.hmd_py, data.hmd_pz,
                        data.ctl_px, data.ctl_py, data.ctl_pz);
                }
                
                // Test player access every 300 packets (5 seconds at 60fps)
                if (packetCount % 300 == 0) {
                    TestPlayerAccess();
                }
                
                // Use script interface to move something
                if (g_script && packetCount % 60 == 0) {
                    char cmd[256];
                    sprintf(cmd, "player.SetPos X %.2f", 1000.0f + data.hmd_px * 100.0f);
                    if (g_script->RunScriptLine(cmd)) {
                        Log("Script command executed");
                    } else {
                        Log("Script command failed");
                    }
                }
            }
            
            CloseHandle(hPipe);
            Log("Pipe disconnected");
        } else {
            Log("Failed to connect to pipe: %d", GetLastError());
        }
        
        if (!g_shouldStop) {
            Sleep(1000);
        }
    }
    
    Log("Pipe thread ending");
    return 0;
}

// NVSE exports
extern "C" {

bool NVSEPlugin_Query(const NVSEInterface* nvse, PluginInfo* info) {
    info->infoVersion = 1;
    info->name = "FNVR";
    info->version = 100;
    return true;
}

bool NVSEPlugin_Load(const NVSEInterface* nvse) {
    // Open log file
    char logPath[MAX_PATH];
    GetModuleFileNameA(NULL, logPath, MAX_PATH);
    char* lastSlash = strrchr(logPath, '\\');
    if (lastSlash) {
        strcpy(lastSlash + 1, "FNVR.log");
        g_logFile = fopen(logPath, "w");
    }
    
    Log("=== FNVR Plugin Loading ===");
    Log("NVSE version: %08X", nvse->nvseVersion);
    Log("Runtime version: %08X", nvse->runtimeVersion);
    Log("Plugin handle request...");
    
    // Get script interface
    if (nvse->QueryInterface) {
        g_script = (NVSEScriptInterface*)nvse->QueryInterface(4);
        if (g_script) {
            Log("Got script interface v%d", g_script->version);
        } else {
            Log("Failed to get script interface");
        }
    }
    
    // Test player immediately
    Log("Initial player test:");
    TestPlayerAccess();
    
    // Create pipe thread
    g_pipeThread = CreateThread(NULL, 0, PipeThreadProc, NULL, 0, NULL);
    if (g_pipeThread) {
        Log("Pipe thread created");
    } else {
        Log("Failed to create pipe thread: %d", GetLastError());
    }
    
    Log("=== Load Complete ===");
    return true;
}

} // extern "C"

// DLL entry point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
    } else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        g_shouldStop = true;
        if (g_pipeThread) {
            WaitForSingleObject(g_pipeThread, 5000);
            CloseHandle(g_pipeThread);
        }
        if (g_logFile) {
            Log("=== Plugin Unloading ===");
            fclose(g_logFile);
        }
    }
    return TRUE;
}