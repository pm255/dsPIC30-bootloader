#include "Stable.h"
#include "MemoryLayout.h"
#include "ErrorExit.h"

MemoryLayout::MemoryLayout(const DeviceInfo &deviceInfo)
{
    _memoryRanges[MEMORY_TYPE_PROGRAM].address = 0x000000;
    _memoryRanges[MEMORY_TYPE_PROGRAM].size = deviceInfo.programMemorySize;
    _memoryRanges[MEMORY_TYPE_DATA].address = 0x800000 - deviceInfo.dataEEPROMSize;
    _memoryRanges[MEMORY_TYPE_DATA].size = deviceInfo.dataEEPROMSize;
    _memoryRanges[MEMORY_TYPE_CONFIG].address = 0xF80000;
    _memoryRanges[MEMORY_TYPE_CONFIG].size = deviceInfo.configMemorySize;

    _configFuseMasks = std::vector<uint32_t>(deviceInfo.configMemoryMasks, deviceInfo.configMemoryMasks + deviceInfo.configMemorySize / 2);
}

MemoryLayout::~MemoryLayout()
{
}

const MemoryRange &MemoryLayout::memoryRange(unsigned memoryType) const
{
    return _memoryRanges[memoryType];
}

unsigned MemoryLayout::memoryTypeByAddress(uint32_t address) const
{
    for (unsigned memoryType = 0; memoryType < MEMORY_TYPE_COUNT; ++memoryType)
    {
        const MemoryRange &range = _memoryRanges[memoryType];
        if ((address >= range.address) && (address < range.address + range.size)) return memoryType;
    }

    errorExit("Unknown memory type at address 0x%06X", (unsigned)address);

    return 0;
}

const std::vector<uint32_t> &MemoryLayout::configFuseMasks() const
{
    return _configFuseMasks;
}
