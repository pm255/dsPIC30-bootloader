#ifndef __FIRMWAREIMAGE_H_INCLUDED_
#define __FIRMWAREIMAGE_H_INCLUDED_

#include "MemoryLayout.h"

const uint32_t UNDEFINED_WORD = 0xFFFFFFFF;

class FirmwareImage
{
public:

	FirmwareImage(const MemoryLayout *memoryLayout);
	~FirmwareImage();

    uint32_t getData(uint32_t address) const;
    void setData(uint32_t address, uint32_t data);

    const std::vector<uint32_t> &rawData(unsigned memoryType) const;

private:

    const MemoryLayout *_memoryLayout;
    std::vector<uint32_t> _memoryData[MEMORY_TYPE_COUNT];

};

#endif // !__FIRMWAREIMAGE_H_INCLUDED_
