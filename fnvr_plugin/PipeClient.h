#pragma once

#include <windows.h>
#include <string>
#include "VRDataPacket.h" // For VRDataPacket

class PipeClient
{
public:
    PipeClient(const std::string& pipeName);
    ~PipeClient();

    bool Connect();
    void Disconnect();
    bool IsConnected() const;
    bool Read(VRDataPacket& packet);

private:
    std::string m_pipeName;
    HANDLE m_pipeHandle;
    bool m_isConnected;
}; 