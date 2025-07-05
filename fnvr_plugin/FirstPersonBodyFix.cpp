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

    NiNode* asNode = DYNAMIC_CAST(node, NiAVObject, NiNode);
    if (asNode) {
        // JIP-LN SDK'da NiTArray farklı tanımlı
        for (UInt16 i = 0; i < asNode->m_children.numObjs; i++) {
            if (asNode->m_children.data && i < asNode->m_children.capacity) {
                 ForceVisible(asNode->m_children.data[i]);
            }
        }
    }
}

// Her frame çağrılacak ana güncelleme fonksiyonu (JIP SDK ile güncellendi)
void FirstPersonBodyFix::UpdateFrame()
{
    if (g_thePlayer)
    {
        if (!g_thePlayer->GetDead() && g_thePlayer->parentCell)
        {
            if (!g_thePlayer->IsThirdPerson() && g_thePlayer->node1stPerson)
            {
                ForceVisible(g_thePlayer->node1stPerson);
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