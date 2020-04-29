#include "Stable.h"
#include "FirmwareImage.h"
#include "ErrorExit.h"

FirmwareImage::FirmwareImage(const MemoryLayout *memoryLayout):
    _memoryLayout(memoryLayout)
{
    for (unsigned memoryType = 0; memoryType < MEMORY_TYPE_COUNT; ++memoryType)
    {
        const MemoryRange &range = _memoryLayout->memoryRange(memoryType);
        _memoryData[memoryType].resize(range.size / 2, UNDEFINED_WORD);
    }
}

FirmwareImage::~FirmwareImage()
{
}

uint32_t FirmwareImage::getData(uint32_t address) const
{
    assert((address & 1) == 0);

    unsigned memoryType = _memoryLayout->memoryTypeByAddress(address);
    const MemoryRange &range = _memoryLayout->memoryRange(memoryType);

    return _memoryData[memoryType][(address - range.address) / 2];
}

void FirmwareImage::setData(uint32_t address, uint32_t data)
{
    assert((address & 1) == 0);

    unsigned memoryType = _memoryLayout->memoryTypeByAddress(address);
    const MemoryRange &range = _memoryLayout->memoryRange(memoryType);

    _memoryData[memoryType][(address - range.address) / 2] = data;
}

const std::vector<uint32_t> &FirmwareImage::rawData(unsigned memoryType) const
{
    return _memoryData[memoryType];
}
