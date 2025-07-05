// fnvr_plugin/PluginMain.cpp
#include "internal/prefix.h"  // JIP-LN SDK prefix - temel tipler için
#include "PluginAPI.h"
#include "PipeClient.h"
#include "Globals.h"
#include "VRSystem.h"
#include "NVCSSkeleton.h"
#include "FirstPersonBodyFix.h"

// NVSE version tanımları (JIP-LN SDK'da eksikse)
#ifndef NVSE_VERSION_INTEGER
#define NVSE_VERSION_INTEGER 6
#endif

#ifndef RUNTIME_VERSION_1_4_0_525
#define RUNTIME_VERSION_1_4_0_525 0x01040525
#endif

// --- Additions for Skeleton Manipulation ---
#include "nvse/GameObjects.h" // For PlayerCharacter, *g_thePlayer
#include "internal/netimmerse.h"   // For NiNode, NiAVObject (JIP-LN SDK)
#include "internal/Ni_types.h"     // For NiPoint3, NiMatrix34 etc. (JIP-LN SDK)
// -----------------------------------------

// Global plugin handles and interfaces
PluginHandle g_pluginHandle = 0;
NVSEMessagingInterface* g_messagingInterface = nullptr;

// Pipe client
PipeClient* g_pipeClient = nullptr;
std::string g_pipeName = "\\\\.\\pipe\\FNVRTracker";
const int g_pluginVersion = PLUGIN_VERSION; // From Globals.h

// Provide implementation for assertion failures
void __cdecl _AssertionFailed(const char* file, unsigned long line, const char* desc)
{
    // _ERROR makrosu tanımlı değilse, basit bir çözüm
}

void MessageHandler(NVSEMessagingInterface::Message* msg)
{
    switch (msg->type)
    {
        case NVSEMessagingInterface::kMessage_DeferredInit:
        {
            _MESSAGE("FNVR | DeferredInit: Initializing...");
            TESGlobals::InitGlobals();
            
            // NVCS Skeleton sistemini başlat
            FNVR::NVCSSkeleton::Manager::GetSingleton().Initialize();
            
            // 1st person body fix'lerini uygula
            FNVR::FirstPersonBodyFix::ApplyFixes();
            break;
        }

        case NVSEMessagingInterface::kMessage_MainGameLoop:
        {
            if (g_pipeClient)
            {
                if (!g_pipeClient->IsConnected())
                {
                    g_pipeClient->Connect();
                }

                if (g_pipeClient->IsConnected())
                {
                    VRDataPacket packet;
                    if (g_pipeClient->Read(packet))
                    {
                        if (packet.version != g_pluginVersion) {
                            TESGlobals::ResetGlobals();
                            SAFE_SET_VALUE(TESGlobals::FNVRStatus, 2.0f); // Version Mismatch
                            // Optional: Disconnect if version is wrong
                            // g_pipeClient->Disconnect(); 
                        } else {
                            TESGlobals::UpdateGlobals(packet);
                            SAFE_SET_VALUE(TESGlobals::FNVRStatus, 1.0f); // Connected

                            // --- SKELETON MANIPULATION LOGIC (disabled) ---
#if 0
                            PlayerCharacter* player = *g_thePlayer;
                            if (player && player->firstPersonSkeleton)
                            {
                                NiNode* skeletonRoot = player->firstPersonSkeleton;
                                NiAVObject* weaponNode = skeletonRoot->GetObjectByName("Weapon");
                                if (weaponNode)
                                {
                                    // Manipulation code
                                }
                            }
#endif
                            // --- END SKELETON MANIPULATION LOGIC ---
                        }
                    }
                    else
                    {
                        // Read failed, probably disconnected
                        TESGlobals::ResetGlobals();
                        SAFE_SET_VALUE(TESGlobals::FNVRStatus, 0.0f); // Disconnected
                    }
                }
            }
            // Her frame 1st-person skeleton görünürlük güncellemesi
            FNVR::FirstPersonBodyFix::UpdateFrame();
            break;
        }
        
        case NVSEMessagingInterface::kMessage_PreLoadGame:
        case NVSEMessagingInterface::kMessage_NewGame:
        {
            _MESSAGE("FNVR | PreLoadGame/NewGame: Resetting state.");
            if (g_pipeClient)
            {
                g_pipeClient->Disconnect();
            }
            TESGlobals::ResetGlobals();
            break;
        }

        case NVSEMessagingInterface::kMessage_ExitGame:
        {
             _MESSAGE("FNVR | ExitGame: Cleaning up.");
            if (g_pipeClient)
            {
                g_pipeClient->Disconnect();
                delete g_pipeClient;
                g_pipeClient = nullptr;
            }
            
            // NVCS Skeleton sistemini kapat (şimdilik boş)
            break;
        }
    }
}

extern "C" {

BOOL WINAPI DllMain(HANDLE hDllHandle, DWORD dwReason, LPVOID lpreserved)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        g_pluginHandle = (PluginHandle)hDllHandle;
        g_messagingInterface = nullptr;
        g_pipeClient = nullptr;
    }
    return TRUE;
}

bool NVSEPlugin_Query(const NVSEInterface* nvse, PluginInfo* info)
{
    _MESSAGE("FNVR | query");

    // Obtain the unique handle assigned by NVSE for proper message routing
    g_pluginHandle = nvse->GetPluginHandle();

    // Fill out plugin info
    info->infoVersion = PluginInfo::kInfoVersion;
    info->name = "FNVR Plugin";
    info->version = g_pluginVersion;

    // Version checks
    if (nvse->nvseVersion < NVSE_VERSION_INTEGER)
    {
        _MESSAGE("FNVR | NVSE version too old (got %08X expected at least %08X)", nvse->nvseVersion, NVSE_VERSION_INTEGER);
        return false;
    }

    if (!nvse->isEditor)
    {
        if (nvse->runtimeVersion < RUNTIME_VERSION_1_4_0_525)
        {
            _MESSAGE("FNVR | Incorrect runtime version (got %08X need %08X)", nvse->runtimeVersion, RUNTIME_VERSION_1_4_0_525);
            return false;
        }
    }

    return true;
}

bool NVSEPlugin_Load(const NVSEInterface* nvse)
{
    _MESSAGE("FNVR | load");

    g_messagingInterface = (NVSEMessagingInterface*)nvse->QueryInterface(kInterface_Messaging);
    if (!g_messagingInterface)
    {
        _MESSAGE("FNVR | Error: couldn't get messaging interface");
        return false;
    }

    g_messagingInterface->RegisterListener(g_pluginHandle, "NVSE", MessageHandler);
    _MESSAGE("FNVR | Registered message listener");

    // Immediately create pipe client so connection can begin without waiting for DeferredInit
    g_pipeClient = new PipeClient(g_pipeName);

    return true;
}

}; // extern "C"
