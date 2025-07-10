// fnvr_plugin/PipeClient.cpp
#include "internal/prefix.h"  // JIP-LN SDK prefix - temel tipler için
#include "PipeClient.h"
#include "FirstPersonBodyFix.h"

// Basit log makrosu
#ifndef _MESSAGE
#define _MESSAGE(fmt, ...) ((void)0)
#endif

PipeClient::PipeClient(const std::string& pipeName)
    : m_pipeName(pipeName), m_pipeHandle(INVALID_HANDLE_VALUE), m_isConnected(false) {}

PipeClient::~PipeClient()
{
    Disconnect();
}

bool PipeClient::Connect()
{
    if (m_isConnected) {
        return true;
    }

    m_pipeHandle = CreateFileA(
        m_pipeName.c_str(),
        GENERIC_READ,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);

    if (m_pipeHandle != INVALID_HANDLE_VALUE)
    {
        _MESSAGE("FNVR | Connected to named pipe: %s", m_pipeName.c_str());
        m_isConnected = true;
        return true;
    }

    DWORD error = GetLastError();
    if (error != ERROR_PIPE_BUSY && error != ERROR_FILE_NOT_FOUND) {
        _MESSAGE("FNVR | Error connecting to pipe: %d", error);
    }
    
    return false;
}

void PipeClient::Disconnect()
{
    if (m_isConnected && m_pipeHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_pipeHandle);
        m_pipeHandle = INVALID_HANDLE_VALUE;
        m_isConnected = false;
        _MESSAGE("FNVR | Disconnected from named pipe.");
    }
}

bool PipeClient::IsConnected() const
{
    return m_isConnected;
}

bool PipeClient::Read(VRDataPacket& packet)
{
    if (!m_isConnected) {
        return false;
    }

    // First read header to determine packet size
    struct {
        uint32_t version;
        uint32_t flags;
    } header;

    DWORD bytesRead = 0;
    BOOL result = ReadFile(
        m_pipeHandle,
        &header,
        sizeof(header),
        &bytesRead,
        NULL);

    if (!result || bytesRead != sizeof(header)) {
        return false;
    }

    // Version kontrolü
    if (header.version != 2) {
        _MESSAGE("FNVR | Warning: Unexpected version %d (expected 2)", header.version);
    }

    // Calculate expected packet size based on flags
    size_t expectedSize = GetPacketSize(header.flags);

    // Read the rest of the packet (we already have the header)
    result = ReadFile(
        m_pipeHandle,
        reinterpret_cast<char*>(&packet) + sizeof(header),
        expectedSize - sizeof(header),
        &bytesRead,
        NULL);

    if (result && bytesRead == expectedSize - sizeof(header))
    {
        // Copy header back
        packet.version = header.version;
        packet.flags = header.flags;

        // Basit veri doğrulama - quaternion'lar normalize edilmiş mi?
        float hmd_qlen = packet.hmd.qw*packet.hmd.qw + packet.hmd.qx*packet.hmd.qx + 
                        packet.hmd.qy*packet.hmd.qy + packet.hmd.qz*packet.hmd.qz;
        if (hmd_qlen < 0.9f || hmd_qlen > 1.1f) {
            _MESSAGE("FNVR | Warning: HMD quaternion not normalized: %.3f", hmd_qlen);
        }

        // Ensure left controller data if not present
        if (!(packet.flags & VR_FLAG_LEFT_CONTROLLER)) {
            // Mirror right controller to left
            packet.extended.left_qw = packet.rightController.qw;
            packet.extended.left_qx = -packet.rightController.qx;
            packet.extended.left_qy = packet.rightController.qy;
            packet.extended.left_qz = packet.rightController.qz;

            packet.extended.left_px = -packet.rightController.px;
            packet.extended.left_py = packet.rightController.py;
            packet.extended.left_pz = packet.rightController.pz;
        }

        // Her 60 frame'de bir skeleton visibility kontrolü
        static int frameCount = 0;
        if (++frameCount % 60 == 0) {
            FNVR::FirstPersonBodyFix::EnsureSkeletonVisible();
        }
        return true;
    } else {
        // Connection lost or invalid size
        _MESSAGE("FNVR | Pipe read error: expected %zu, got %lu", expectedSize - sizeof(header), bytesRead);
        Disconnect();
        return false;
    }
} 