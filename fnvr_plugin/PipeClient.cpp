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

    // Read the raw packet data from Python (88 bytes)
    VRDataPacketV2 rawPacket;
    DWORD bytesRead = 0;
    BOOL result = ReadFile(
        m_pipeHandle,
        &rawPacket,
        sizeof(VRDataPacketV2),  // 88 bytes
        &bytesRead,
        NULL);

    if (!result || bytesRead != sizeof(VRDataPacketV2)) {
        if (result) {
            _MESSAGE("FNVR | Pipe read error: expected %zu bytes, got %lu", sizeof(VRDataPacketV2), bytesRead);
        } else {
            DWORD error = GetLastError();
            if (error != ERROR_BROKEN_PIPE) {
                _MESSAGE("FNVR | Pipe read failed: error %lu", error);
            }
        }
        Disconnect();
        return false;
    }

    // Version check
    if (rawPacket.version != 2) {
        _MESSAGE("FNVR | Warning: Unexpected packet version %u (expected 2)", rawPacket.version);
    }

    // Convert from wire format to game format
    ConvertV2ToFull(rawPacket, packet);

    // Validate quaternions are normalized
    float hmd_qlen = packet.hmd_qw*packet.hmd_qw + packet.hmd_qx*packet.hmd_qx + 
                    packet.hmd_qy*packet.hmd_qy + packet.hmd_qz*packet.hmd_qz;
    if (hmd_qlen < 0.9f || hmd_qlen > 1.1f) {
        _MESSAGE("FNVR | Warning: HMD quaternion not normalized: %.3f", hmd_qlen);
    }

    // Check skeleton visibility periodically
    static int frameCount = 0;
    if (++frameCount % 60 == 0) {
        FNVR::FirstPersonBodyFix::EnsureSkeletonVisible();
    }
    
    return true;
} 