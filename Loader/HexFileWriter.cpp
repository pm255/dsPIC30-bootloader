#include "Stable.h"
#include "HexFileWriter.h"
#include "ErrorExit.h"

HexFileWriter::HexFileWriter(const std::string &filePath):
    _filePath(filePath)
{
}

HexFileWriter::~HexFileWriter()
{
}

void HexFileWriter::writeImage(const FirmwareImage &image, const MemoryLayout &memoryLayout)
{
    _file = fopen(_filePath.c_str(), "wt");
    if (_file == nullptr)
    {
        errorExit("File open error: %s", _filePath.c_str());
    }

    _bufferHighAddress = 0xFFFFFFFF;

    writeMemoryRange(image, memoryLayout.memoryRange(MEMORY_TYPE_PROGRAM));
    writeMemoryRange(image, memoryLayout.memoryRange(MEMORY_TYPE_DATA));
    writeMemoryRange(image, memoryLayout.memoryRange(MEMORY_TYPE_CONFIG));

    flushBuffer();
    writeRecord(0x01, 0x0000, nullptr, 0);

    if ((ferror(_file) != 0) || (fclose(_file) != 0))
    {
        errorExit("File write error: %s", _filePath.c_str());
    }
}

void HexFileWriter::writeMemoryRange(const FirmwareImage &image, const MemoryRange &memoryRange)
{
    for (uint32_t address = memoryRange.address; address < memoryRange.address + memoryRange.size; address += 2)
    {
        uint32_t data = image.getData(address);
        if (data == UNDEFINED_WORD) continue;

        uint32_t hexAddress = address * 2;
        writeData(hexAddress++, (uint8_t)(data >> 0));
        writeData(hexAddress++, (uint8_t)(data >> 8));
        writeData(hexAddress++, (uint8_t)(data >> 16));
        writeData(hexAddress++, (uint8_t)(data >> 24));
    }
}

void HexFileWriter::writeData(uint32_t hexAddress, uint8_t data)
{
    uint32_t hexHighAddress = hexAddress & 0xFFFF0000;
    if (hexHighAddress != _bufferHighAddress)
    {
        flushBuffer();

        uint8_t buffer[2];
        buffer[0] = (uint8_t)(hexHighAddress >> 24);
        buffer[1] = (uint8_t)(hexHighAddress >> 16);
        writeRecord(0x04, 0x0000, buffer, 2);

        _bufferHighAddress = hexHighAddress;
    }

    unsigned hexOffset = hexAddress & 0x0000FFFF;
    if (!_buffer.empty() && (_bufferOffset + _buffer.size() != hexOffset))
    {
        flushBuffer();
    }

    if (_buffer.empty()) _bufferOffset = hexOffset;
    _buffer.push_back(data);

    if (_buffer.size() == 16) flushBuffer();
}

void HexFileWriter::flushBuffer()
{
    if (!_buffer.empty())
    {
        writeRecord(0x00, _bufferOffset, _buffer.data(), _buffer.size());
        _buffer.clear();
    }
}

void HexFileWriter::writeRecord(unsigned type, unsigned offset, const uint8_t *buffer, size_t size)
{
    fprintf(_file, ":%02X%02X%02X%02X", (unsigned)size, (offset >> 8) & 0xFF, offset & 0xFF, type);
    uint8_t crc = (uint8_t)((unsigned)size + (offset >> 8) + offset + type);

    for (size_t i = 0; i < size; ++i)
    {
        fprintf(_file, "%02X", (unsigned)*buffer);
        crc += *(buffer++);
    }

    fprintf(_file, "%02X\n", (unsigned)(-crc & 0xFF));
}
