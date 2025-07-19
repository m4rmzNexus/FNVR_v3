#pragma once

#include <windows.h>
#include <string>
#include "VRDataPacket.h" // Includes both VRDataPacketV2 and VRDataPacket definitions

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