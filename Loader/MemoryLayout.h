#ifndef __MEMORYLAYOUT_H_INCLUDED_
#define __MEMORYLAYOUT_H_INCLUDED_

#include "DeviceInfo.h"

const unsigned
    MEMORY_TYPE_PROGRAM = 0,
    MEMORY_TYPE_DATA = 1,
    MEMORY_TYPE_CONFIG = 2,
    MEMORY_TYPE_COUNT = 3;

const uint32_t
    ROW_SIZE_PROGRAM = 64,
    ROW_SIZE_DATA = 32,
    ROW_SIZE_CONFIG = 2;

const uint32_t
    WORD_MASK_PROGRAM = 0x00FFFFFF,
    WORD_MASK_DATA = 0x0000FFFF,
    WORD_MASK_CONFIG = 0x0000FFFF;

struct MemoryRange
{
    uint32_t address;
    uint32_t size;
};

class MemoryLayout
{
public:

	MemoryLayout(const DeviceInfo &deviceInfo);
	~MemoryLayout();

    const MemoryRange &memoryRange(unsigned memoryType) const;
    unsigned memoryTypeByAddress(uint32_t address) const;
    const std::vector<uint32_t> &configFuseMasks() const;

private:

    MemoryRange _memoryRanges[MEMORY_TYPE_COUNT];
    std::vector<uint32_t> _configFuseMasks;

};

#endif // !__MEMORYLAYOUT_H_INCLUDED_
