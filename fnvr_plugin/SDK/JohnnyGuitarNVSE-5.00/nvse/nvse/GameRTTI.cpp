#include "GameRTTI.h"

#if RUNTIME_VERSION == RUNTIME_VERSION_1_4_0_525
#include "GameRTTI_1_4_0_525.inc"
#elif EDITOR
#include "GameRTTI_EDITOR.inc"
#else
#error
#endif