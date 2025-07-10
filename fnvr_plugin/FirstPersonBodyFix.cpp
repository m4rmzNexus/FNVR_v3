#include "internal/prefix.h"  // JIP-LN SDK prefix - temel tipler için
#include "FirstPersonBodyFix.h"
#include "nvse/GameObjects.h"
#include "nvse/GameRTTI.h"
#include "internal/netimmerse.h"  // NiAVObject tanımı için (JIP-LN SDK)

namespace FNVR {

void FirstPersonBodyFix::ForceVisible(NiAVObject* node)
{
    if (!node) return;
    
    // JIP-LN SDK'daki flag enum'larını kullan
    node->m_flags &= ~NiAVObject::kNiFlag_Culled;      // Culling'i kapat
    node->m_flags &= ~NiAVObject::kNiFlag_Hidden;      // Hidden flag'ini kapat
    node->m_flags |= NiAVObject::kNiFlag_ForceUpdate;  // Force update'i aç

    // Child node'ları için recursive çağrı yapmıyoruz - çok karmaşık
    // Sadece root node'u görünür yapıyoruz, bu genelde yeterli
}

// Her frame çağrılacak ana güncelleme fonksiyonu (JIP SDK ile güncellendi)
void FirstPersonBodyFix::UpdateFrame()
{
    // JIP-LN SDK'da g_thePlayer zaten PlayerCharacter* türünde
    PlayerCharacter* player = g_thePlayer;
    if (player)
    {
        if (!player->GetDead() && player->parentCell)
        {
            if (!player->IsThirdPerson() && player->node1stPerson)
            {
                ForceVisible(player->node1stPerson);
            }
        }
    }
}

// Bu eski fonksiyonlar artık kullanılmıyor.
void FirstPersonBodyFix::ApplyFixes() {}
void FirstPersonBodyFix::EnsureSkeletonVisible() {}
void FirstPersonBodyFix::FixEnhancedCameraIssues() {}
void FirstPersonBodyFix::ApplyVorpXFixes() {}
void FirstPersonBodyFix::Fix1stPersonSkeleton() {}
void FirstPersonBodyFix::FixCameraOffsets() {}
void FirstPersonBodyFix::ForceSkeletonVisibility() {}

} // namespace FNVR 