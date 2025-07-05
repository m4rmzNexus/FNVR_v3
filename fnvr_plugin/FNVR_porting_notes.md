# FNVR JIP-LN SDK Porting & Build Notları

## 1. Build ve SDK Uyumluluğu
- **CMake ve Platform:**
  - Yanlışlıkla x64 platform seçilmişti, Fallout NV için Win32 (x86) zorunlu.
  - CMake'de `-A Win32` ile düzeltildi.
- **C++20 ve NVSE:**
  - NVSE SDK bazı eski tipleri (ör. `byte`) kullanıyor, C++20 ile çakışıyor. `byte` → `UInt8` ile değiştirildi.
- **JIP-LN SDK:**
  - Sadece header-only kullanılmalı, source dosyaları doğrudan projeye eklenmemeli.
  - Temel tipler için `internal/prefix.h` mutlaka ilk include edilmeli.

## 2. Hook ve Per-Frame Update
- **Orijinal Hook:**
  - `WriteRelJump` ile yapılan doğrudan fonksiyon hook'u sonsuz döngüye sebep oluyordu.
  - NVSE'nin MainGameLoop mesajı zaten per-frame update için yeterli, ekstra hook gereksiz.
- **Çözüm:**
  - Tüm hook kodları kaldırıldı, per-frame update doğrudan MainGameLoop mesajında çağrılıyor.

## 3. NiAVObject ve ForceVisible
- **Struct Offset:**
  - Eski kodda `m_flags` için offset `0x14` kullanılıyordu, JIP-LN SDK'da doğru offset `0x30`.
  - JIP-LN SDK'da flag enum'ları mevcut, doğrudan kullanılmalı: `kNiFlag_Culled`, `kNiFlag_Hidden`, `kNiFlag_ForceUpdate`.
- **Child Traversal:**
  - NiTArray yapısı JIP-LN SDK'da farklı: `numObjs`, `capacity`, `data` kullanılmalı.

## 4. PlayerCharacter ve g_thePlayer
- **Pointer Tipi:**
  - JIP-LN SDK'da `g_thePlayer` doğrudan `PlayerCharacter*` (NVSE SDK'da `PlayerCharacter**` idi!).
  - Kodda `*g_thePlayer` yerine doğrudan `g_thePlayer` kullanılmalı.
- **firstPersonSkeleton:**
  - JIP-LN SDK'da `firstPersonSkeleton` yok, onun yerine `node1stPerson` kullanılmalı.
- **IsDead():**
  - JIP-LN SDK'da `IsDead()` yok, `GetDead()` fonksiyonu var.

## 5. VRDataPacket ve PipeClient
- **VRDataPacket:**
  - Yapı ayrı bir header'a (`VRDataPacket.h`) taşındı, circular dependency önlendi.
- **PipeClient:**
  - Artık sadece `#include "VRDataPacket.h"` ile derleniyor.

## 6. Logging ve _MESSAGE
- **_MESSAGE Makrosu:**
  - JIP-LN SDK'da `_MESSAGE` tanımlı değil, her dosyada basit bir stub makro eklendi.

## 7. LookupFormByID ve DataHandler
- **LookupFormByID:**
  - JIP-LN SDK'da yok, elle inline fonksiyon olarak eklendi (`0x4839C0` adresi).
- **DataHandler::GetSingleton():**
  - JIP-LN SDK'da mevcut, doğrudan kullanılabilir.
- **DataHandler::LookupModByName:**
  - JIP-LN SDK'da yok, elle inline fonksiyon olarak eklendi (`0x46F1C0` adresi).

## 8. Diğer Build Sorunları
- **IDebugLog ve gLog:**
  - JIP-LN SDK'da yok, tüm referanslar kaldırıldı.
- **common/IFileStream.h:**
  - Gerekli değil, stub fonksiyonlar kaldırıldı.
- **NVSE_VERSION_INTEGER ve RUNTIME_VERSION_1_4_0_525:**
  - JIP-LN SDK'da tanımlı değilse, PluginMain.cpp'de define edildi.

## 9. Kalan Uyarılar
- **DYNAMIC_CAST Makrosu:**
  - JIP-LN SDK'nın GameRTTI.h dosyasında macro tanımı MSVC ile uyarı veriyor ama derlemeyi engellemiyor.

---

Bu notlar, FNVR'nin JIP-LN SDK ile uyumlu, stabil ve modern bir build ortamında derlenmesi için yapılan tüm önemli değişiklikleri ve karşılaşılan sorunları özetler. 