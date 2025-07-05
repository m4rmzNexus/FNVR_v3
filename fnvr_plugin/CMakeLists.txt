cmake_minimum_required(VERSION 3.20)
project(FNVRPlugin)

# --- Platform Ayarı (32-bit zorunlu) ---
set(CMAKE_GENERATOR_PLATFORM Win32)

# --- Proje Yolları ---
# NVSE SDK yolunu yeni NVSE-6.3.10 dizinine ayarla
set(NVSE_SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/SDK/NVSE-6.3.10)

# JohnnyGuitarNVSE ve JIP-LN-NVSE opsiyonel olarak aynı kalıyor
set(JG_SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/SDK/JohnnyGuitarNVSE-5.00)
set(JIP_SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/SDK/JIP-LN-NVSE-main)

# --- Derlenecek Kaynak Dosyaları ---
# Önce hedefi oluşturmalıyız.
add_library(${PROJECT_NAME} SHARED
    # Bizim eklentimizin kendi kaynakları
    PluginMain.cpp
    Globals.cpp
    PipeClient.cpp
    VRSystem.cpp
    NVCSSkeleton.cpp
    FirstPersonBodyFix.cpp

    # NVSE Örnek Projesinden Gerekli Görülen Temel Kaynaklar
    "${NVSE_SDK_PATH}/nvse/nvse/GameAPI.cpp"
    "${NVSE_SDK_PATH}/nvse/nvse/GameForms.cpp"
    "${NVSE_SDK_PATH}/nvse/nvse/GameObjects.cpp"
    "${NVSE_SDK_PATH}/nvse/nvse/GameTypes.cpp"
    "${NVSE_SDK_PATH}/nvse/nvse/GameData.cpp"
    "${NVSE_SDK_PATH}/nvse/nvse/GameScript.cpp"
    "${NVSE_SDK_PATH}/nvse/nvse/GameRTTI.cpp"    # For RTTI symbols
    "${NVSE_SDK_PATH}/nvse/nvse/GameExtraData.cpp" # For GetExtraScript
    "${NVSE_SDK_PATH}/nvse/nvse/GameBSExtraData.cpp" # For extra data handling
    "${NVSE_SDK_PATH}/nvse/nvse/SafeWrite.cpp"
    "${NVSE_SDK_PATH}/nvse/nvse/Utilities.cpp"
    "${NVSE_SDK_PATH}/nvse/nvse/utility.cpp"     # For Pool_Alloc, etc.
    "${NVSE_SDK_PATH}/nvse/nvse/containers.cpp"  # For UnorderedMap
    "${NVSE_SDK_PATH}/nvse/nvse/NiTypes.cpp"     # For Point3Distance
    "${NVSE_SDK_PATH}/nvse/nvse/GameProcess.cpp" # For ModelLoader
    "${NVSE_SDK_PATH}/nvse/nvse/GameTasks.cpp"   # For ModelLoader implementation
    "${NVSE_SDK_PATH}/common/IDebugLog.cpp"      # For IDebugLog implementation
    "${NVSE_SDK_PATH}/common/ITypes.cpp"         # For StrCompare, StrLen etc.
)

# --- Dahil Edilecek Klasör Yolları ---
target_include_directories(${PROJECT_NAME} PRIVATE
    "${NVSE_SDK_PATH}"
    "${NVSE_SDK_PATH}/nvse"
    "${NVSE_SDK_PATH}/common"
)

# JohnnyGuitarNVSE ve JIP-LN-NVSE başlıklarını sadece dizin mevcutsa ekle
if(EXISTS "${JG_SDK_PATH}")
    target_include_directories(${PROJECT_NAME} PRIVATE "${JG_SDK_PATH}")
endif()
if(EXISTS "${JIP_SDK_PATH}")
    target_include_directories(${PROJECT_NAME} PRIVATE "${JIP_SDK_PATH}")
endif()

# --- Derleyici Ayarları ---
# xNVSE örnek projesine uygun derleyici ayarları
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE 
        "/std:c++20"                               # C++20 standardı (örnek proje stdcpplatest kullanıyor)
        "/FI${NVSE_SDK_PATH}/nvse/nvse/prefix.h"  # Force include prefix.h
        "/MP"                                      # Multi-processor compilation
        "/MD$<$<CONFIG:Debug>:d>"                  # Multi-threaded DLL runtime (Debug'da MDd)
        "/Zc:__cplusplus"                          # Correct __cplusplus macro
        "/Zc:twoPhase-"                            # Disable two-phase name lookup
        "/permissive"                              # Relaxed conformance mode (permissive- yerine)
        "/Zc:threadSafeInit-"                      # Disable thread-safe statics
        "/EHa-"                                    # Disable C++ exceptions
        "/Gy"                                      # Enable function-level linking
    )
endif()

# Gerekli ön işlemci tanımları (xNVSE örnek projesinden)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    VC_EXTRALEAN
    _CRT_SECURE_NO_WARNINGS
    _HAS_EXCEPTIONS=0
    _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
    RUNTIME
    RUNTIME_VERSION=0x040020D0                     # Örnek projeden alınan değer
    _MBCS                                          # Multi-Byte Character Set
)

# --- Çıktı ve Kurulum Ayarları ---
if(DEFINED ENV{FalloutNVPath})
    set(OUTPUT_PATH "$ENV{FalloutNVPath}/Data/NVSE/Plugins")
    file(MAKE_DIRECTORY ${OUTPUT_PATH})
    message(STATUS "FalloutNVPath bulundu. DLL şu konuma kopyalanacak: ${OUTPUT_PATH}")
else()
    set(OUTPUT_PATH "${CMAKE_BINARY_DIR}")
    message(WARNING "FalloutNVPath ortam değişkeni tanımlanmamış. DLL, build dizinine oluşturulacak.")
endif()

# .def dosyasını kullanarak fonksiyonları dışa aktar
if(MSVC)
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/DEF:\"${CMAKE_CURRENT_SOURCE_DIR}/fnvr_plugin.def\"")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_NAME "FNVR"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_PATH}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_PATH}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${OUTPUT_PATH}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${OUTPUT_PATH}"
    CXX_STANDARD 20                                # C++20 standardı
    CXX_STANDARD_REQUIRED YES
)

# C++ standardını ayarla
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) 