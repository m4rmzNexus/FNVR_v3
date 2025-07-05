cmake_minimum_required(VERSION 3.15)
project(FNVRPlugin)

# --- Platform Ayarı (32-bit zorunlu) ---
set(CMAKE_GENERATOR_PLATFORM Win32)

# --- Proje Yolları ---
# Projenin stabil olması için JIP LN NVSE SDK'sı gereklidir.
set(NVSE_SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/SDK/JIP-LN-NVSE-main)

# JohnnyGuitarNVSE ve JIP-LN-NVSE opsiyonel olarak aynı kalıyor
set(JG_SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/SDK/JohnnyGuitarNVSE-5.00)
set(JIP_SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/SDK/JIP-LN-NVSE-main)

# --- Derlenecek Kaynak Dosyaları ---
# Önce hedefi oluşturmalıyız.
add_library(${PROJECT_NAME} SHARED
    # Sadece bu projenin kendi kaynak dosyaları derlenecek.
    # JIP SDK dosyaları derlenmeyecek, sadece başlık dosyaları kullanılacak.
    PluginMain.cpp
    Globals.cpp
    PipeClient.cpp
    VRSystem.cpp
    NVCSSkeleton.cpp
    FirstPersonBodyFix.cpp
)

# --- Dahil Edilecek Klasör Yolları ---
# Derleyiciye JIP SDK'sının başlık dosyalarını nerede bulacağını söylüyoruz.
target_include_directories(${PROJECT_NAME} PRIVATE
    "${NVSE_SDK_PATH}"  # JIP-LN SDK root
    "${NVSE_SDK_PATH}/nvse"
    "${NVSE_SDK_PATH}/internal"
)

# JohnnyGuitarNVSE ve JIP-LN-NVSE başlıklarını sadece dizin mevcutsa ekle
if(EXISTS "${JG_SDK_PATH}")
    target_include_directories(${PROJECT_NAME} PRIVATE "${JG_SDK_PATH}")
endif()
if(EXISTS "${JIP_SDK_PATH}")
    target_include_directories(${PROJECT_NAME} PRIVATE "${JIP_SDK_PATH}")
endif()

# --- JIP LN UYUMLULUK AYARLARI ---
# JIP LN'in vcxproj dosyasından alınan, SDK'nın doğru derlenmesi için GEREKLİ olan ayarlar.
target_compile_definitions(${PROJECT_NAME} PRIVATE
    WIN32
    _WINDOWS
    _USRDLL
    _WINDLL
    _CRT_SECURE_NO_WARNINGS
    PROJECT_NAME="${PROJECT_NAME}"
    _HAS_EXCEPTIONS=0
    RUNTIME
    EDITOR_RUNTIME=0
    IS_ADDRESS_LIBRARY=0
    IS_AE=0
    NO_LAUNDER_CLASS_MEMBERS=0
)

if(MSVC)
    # JIP'in kullandığı C++20 standardı ve diğer MSVC ayarları
    target_compile_options(${PROJECT_NAME} PRIVATE
        /std:c++latest
        /MP
        /Zc:__cplusplus
        /permissive-
        /Zc:preprocessor
        /Zc:twoPhase-
        /EHsc-
        /Gy
        /Gw
        /bigobj
        /FS
    )
    # C4244 ve C4018 gibi yaygın uyarıları devre dışı bırak
    add_compile_options(
        -wd4244
        -wd4018
    )
endif()

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

# Sadece kendi projemizin kaynak dosyalarını derle
target_sources(${PROJECT_NAME} PRIVATE
    PluginMain.cpp
    Globals.cpp
    PipeClient.cpp
    VRSystem.cpp
    NVCSSkeleton.cpp
    FirstPersonBodyFix.cpp
    # Header dosyalar
    VRDataPacket.h
)

# Pre-compiled header şu an için devre dışı
# target_precompile_headers(${PROJECT_NAME} PRIVATE fnvr_plugin_pch.h)

# --- Post-Build Olayı ---
# Derlenen DLL'i otomatik olarak oyunun NVSE eklenti klasörüne kopyala
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:${PROJECT_NAME}>"
    "D:/SteamLibrary/steamapps/common/Fallout New Vegas/Data/NVSE/Plugins/"
    COMMENT "FNVR.dll D:/SteamLibrary/steamapps/common/Fallout New Vegas/Data/NVSE/Plugins/ konumuna kopyalandı"
) 