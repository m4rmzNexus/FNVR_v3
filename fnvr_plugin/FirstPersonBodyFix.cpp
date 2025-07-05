#include "FirstPersonBodyFix.h"
#include "nvse/PluginAPI.h"
#include "nvse/GameObjects.h"
#include "nvse/GameRTTI.h"
#include "nvse/NiObjects.h"  // NiAVObject tanımı için

namespace FNVR {

// NiAVObject'in private üyelerine erişim için helper
static UInt32& GetNodeFlags(NiAVObject* node) {
    // m_flags offset'i 0x030 (unk0030)
    return *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(node) + 0x030);
}

void FirstPersonBodyFix::ApplyFixes() {
    _MESSAGE("FNVR | Applying 1st person body fixes...");
    
    // Enhanced Camera sorunlarını düzelt
    FixEnhancedCameraIssues();
    
    // Skeleton görünürlüğünü zorla
    EnsureSkeletonVisible();
    
    // VorpX modunda özel düzeltmeler
    ApplyVorpXFixes();
}

void FirstPersonBodyFix::EnsureSkeletonVisible() {
    PlayerCharacter* player = PlayerCharacter::GetSingleton();
    if (!player || !player->playerNode) {
        return;
    }
    
    // Player node'un kendisini visible yap
    NiNode* playerNode = player->playerNode;
    if (playerNode) {
        // NiNode, NiAVObject'ten türediği için flag'lere erişebiliriz
        GetNodeFlags(playerNode) &= ~NiAVObject::kFlag_AppCulled;
        
        // Tüm child'ları da visible yap
        for (UInt16 i = 0; i < playerNode->m_children.numObjs; i++) {
            NiAVObject* child = playerNode->m_children.data[i];
            if (child) {
                GetNodeFlags(child) &= ~NiAVObject::kFlag_AppCulled;
            }
        }
        
        _MESSAGE("FNVR | 1st person skeleton visibility fixed");
    }
}

void FirstPersonBodyFix::FixEnhancedCameraIssues() {
    // Enhanced Camera'nın skeleton'ı gizleme kodunu override et
    PlayerCharacter* player = PlayerCharacter::GetSingleton();
    if (!player) return;
    
    // Enhanced Camera ground sink fix için player position'ı ayarla
    if (player->baseProcess) {
        // Player'ın yere batmasını önle
        if (player->posZ < 0) {
            player->posZ = 0;
        }
    }
}

void FirstPersonBodyFix::ApplyVorpXFixes() {
    // VorpX modunda özel camera offset'leri
    char iniPath[MAX_PATH];
    GetModuleFileNameA(NULL, iniPath, MAX_PATH);
    strcpy(strrchr(iniPath, '\\'), "\\Data\\NVSE\\Plugins\\FNVR.ini");
    
    int vorpxMode = GetPrivateProfileIntA("General", "VorpXMode", 0, iniPath);
    if (!vorpxMode) return;
    
    PlayerCharacter* player = PlayerCharacter::GetSingleton();
    if (!player || !player->playerNode) return;
    
    // VorpX modunda player node scale'ini kontrol et
    NiNode* playerNode = player->playerNode;
    if (playerNode) {
        // Sadece AppCulled bayrağını düzelt
        ForceVisible(playerNode);
    }
}

void FirstPersonBodyFix::Fix1stPersonSkeleton() {
    PlayerCharacter* player = PlayerCharacter::GetSingleton();
    if (!player) return;
    
    // 1st person skeleton'ı visible tut
    if (player->playerNode) {
        GetNodeFlags(player->playerNode) &= ~NiAVObject::kFlag_AppCulled;
    }
}

void FirstPersonBodyFix::FixCameraOffsets() {
    // Enhanced Camera ground sink fix
    PlayerCharacter* player = PlayerCharacter::GetSingleton();
    if (!player) return;
    
    // Player'ın z pozisyonunu kontrol et
    float groundZ = player->posZ;
    
    // Minimum yüksekliği ayarla
    if (groundZ < 0) {
        player->posZ = 0;
    }
}

void FirstPersonBodyFix::ForceSkeletonVisibility() {
    PlayerCharacter* player = PlayerCharacter::GetSingleton();
    if (!player || !player->playerNode) return;
    
    // Tüm child node'ları visible yap
    NiNode* playerNode = player->playerNode;
    for (UInt16 i = 0; i < playerNode->m_children.numObjs; i++) {
        NiAVObject* child = playerNode->m_children.data[i];
        if (child) {
            // Hidden flag'ini kaldır
            GetNodeFlags(child) &= ~NiAVObject::kFlag_AppCulled;
            
            // NiNode ise recursive olarak devam et
            NiNode* childNode = DYNAMIC_CAST(child, NiAVObject, NiNode);
            if (childNode) {
                for (UInt16 j = 0; j < childNode->m_children.numObjs; j++) {
                    NiAVObject* subChild = childNode->m_children.data[j];
                    if (subChild) {
                        GetNodeFlags(subChild) &= ~NiAVObject::kFlag_AppCulled;
                    }
                }
            }
        }
    }
}

// === Yardımcı: Bayrakları doğru kombinasyonla aç ===
void FirstPersonBodyFix::ForceVisible(NiAVObject* node)
{
    if (!node) return;
    UInt32& fl = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(node) + 0x30);
    fl &= ~NiAVObject::kFlag_AppCulled;                 // render cull kapalı
    fl |=  NiAVObject::kFlag_SelUpdate;                 // selective update aktif
    fl |=  NiAVObject::kFlag_SelUpdateTransforms;       // transform güncelle
}

void FirstPersonBodyFix::UpdateFrame()
{
    PlayerCharacter* pc = PlayerCharacter::GetSingleton();
    if (!pc || !pc->playerNode) return;
    // Ana düğüm ve tüm çocuklara uygula
    ForceVisible(pc->playerNode);
    NiNode* root = pc->playerNode;
    for (UInt16 i = 0; i < root->m_children.numObjs; i++)
        ForceVisible(root->m_children.data[i]);
}

} // namespace FNVR 