#include "platform/common/iplatform.h"

#if defined(_WIN32)
#include "platform/windows/platform.h"
IPlatform* CreatePlatform() { return new WindowsPlatform(); }
#else
#include "platform/linux/platform.h"
IPlatform* CreatePlatform() { return new LinuxPlatform(); }
#endif
