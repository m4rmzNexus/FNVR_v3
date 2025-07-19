#pragma once

// This file just includes the actual packet definitions from FNVRGlobals
// The VRDataPacketV2 is the wire format (what Python sends)
// The VRDataPacketFlat/VRDataPacket is the game format (what the plugin uses internally)

#include "../FNVRGlobals/VRDataPacketV2.h"

// VRDataPacket is now defined in VRDataPacketV2.h as VRDataPacketFlat
// ConvertV2ToFull is also defined there as ConvertV2ToFlat