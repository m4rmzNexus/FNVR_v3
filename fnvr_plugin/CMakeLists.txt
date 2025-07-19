cmake_minimum_required(VERSION 3.10)
project(FNVRPlugin)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find NVSE SDK paths
set(NVSE_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/SDK/NVSE-6.3.10")
set(JG_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/SDK/JohnnyGuitarNVSE-5.00/nvse")

# Include directories
include_directories(
    ${NVSE_SDK_PATH}
    ${NVSE_SDK_PATH}/nvse
    ${JG_SDK_PATH}
    ${JG_SDK_PATH}/nvse
)

# Source files
set(SOURCES
    PluginMain.cpp
    VRSystem.cpp
    PipeClient.cpp
    NVCSSkeleton.cpp
    FirstPersonBodyFix.cpp
    Globals.cpp
)

# Create DLL
add_library(FNVR SHARED ${SOURCES})

# Set output name
set_target_properties(FNVR PROPERTIES 
    PREFIX ""
    OUTPUT_NAME "FNVR"
)

# Compiler flags
if(MSVC)
    target_compile_options(FNVR PRIVATE 
        /O2 
        /MT 
        /W3 
        /EHsc
        /D_CRT_SECURE_NO_WARNINGS
    )
    
    # Linker flags
    set_target_properties(FNVR PROPERTIES 
        LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/fnvr_plugin.def"
    )
endif()

# Link libraries
target_link_libraries(FNVR 
    kernel32
    user32
)

# Install to game directory (use CMAKE_INSTALL_PREFIX or copy manually)
# install(TARGETS FNVR 
#     RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/Data/NVSE/Plugins"
# )