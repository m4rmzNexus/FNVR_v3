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

    DWORD bytesRead = 0;
    BOOL result = ReadFile(
        m_pipeHandle,
        &packet,
        sizeof(VRDataPacket),
        &bytesRead,
        NULL);

    if (result && bytesRead == sizeof(VRDataPacket))
    {
        // Her 60 frame'de bir skeleton visibility kontrolü
        static int frameCount = 0;
        if (++frameCount % 60 == 0) {
            FNVR::FirstPersonBodyFix::EnsureSkeletonVisible();
        }
        return true;
    }
    else
    {
        // Connection lost
        _MESSAGE("FNVR | Pipe connection lost.");
        Disconnect();
        return false;
    }
} 