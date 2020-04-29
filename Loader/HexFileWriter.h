#ifndef __HEXFILEWRITER_H_INCLUDED_
#define __HEXFILEWRITER_H_INCLUDED_

#include "FirmwareImage.h"

class HexFileWriter
{
public:

	HexFileWriter(const std::string &filePath);
	~HexFileWriter();

    void writeImage(const FirmwareImage &image, const MemoryLayout &memoryLayout);

private:

    std::string _filePath;
    FILE *_file = nullptr;

    uint32_t _bufferHighAddress = 0; // hex file address
    uint32_t _bufferOffset = 0; // hex file offset
    std::vector<uint8_t> _buffer;

    void writeMemoryRange(const FirmwareImage &image, const MemoryRange &memoryRange);
    void writeData(uint32_t address, const uint8_t *buffer, size_t size);
    void writeData(uint32_t hexAddress, uint8_t data);
    void flushBuffer();
    void writeRecord(unsigned type, unsigned offset, const uint8_t *buffer, size_t size);

};

#endif // !__HEXFILEWRITER_H_INCLUDED_
