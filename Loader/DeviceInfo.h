#ifndef __DEVICEINFO_H_INCLUDED_
#define __DEVICEINFO_H_INCLUDED_

struct DeviceInfo
{
    uint32_t deviceId;
    const char *name;
    bool supported;
    uint32_t programMemorySize;
    uint32_t dataEEPROMSize;
    uint32_t configMemorySize;
    const uint32_t *configMemoryMasks;
};

const DeviceInfo *getDeviceInfo(uint32_t deviceId); // null if the device is not found

#endif // !__DEVICEINFO_H_INCLUDED_
