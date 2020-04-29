#include "Stable.h"
#include "HexFileLoad.h"
#include "ErrorExit.h"

struct HexRecord
{
    uint32_t offset;
    uint8_t type;
    std::vector<uint8_t> data;
};

static std::string trim(const std::string &str)
{
    auto p1 = std::find_if_not(str.begin(), str.end(), iswspace);
    auto p2 = std::find_if_not(str.rbegin(), str.rend(), iswspace).base();
    return p1 >= p2 ? std::string() : std::string(p1, p2);
}

static std::string readString(FILE *file)
{
    char buffer[1024];
    char *str = fgets(buffer, sizeof buffer, file);
    if (str == nullptr) return std::string();

    return trim(std::string(str));
}

static bool charToNumeral(char chr, uint8_t *numeral)
{
    if ((chr >= '0') && (chr <= '9')) *numeral = chr - '0';
    else if ((chr >= 'A') && (chr <= 'F')) *numeral = chr - 'A' + 10;
    else if ((chr >= 'a') && (chr <= 'f')) *numeral = chr - 'a' + 10;
    else return false;
   
    return true;
}

static bool parseHexByte(const std::string &str, uint8_t *byte)
{
    if (str.size() != 2) return false;

    uint8_t numeral1;
    if (!charToNumeral(str[0], &numeral1)) return false;
    uint8_t numeral2;
    if (!charToNumeral(str[1], &numeral2)) return false;

    *byte = (numeral1 << 4) | numeral2;
    return true;
}

static std::vector<uint8_t> parseString(const std::string &str)
{
    const char *p = str.c_str();
    if (*(p++) != ':')
    {
        errorExit("Cannot parse string (no marker): %s", str.c_str());
    }

    std::vector<uint8_t> bytes;
    while (*p != 0x00)
    {
        uint8_t byte;
        if ((*(p + 1) == 0x00) || (!parseHexByte(std::string(p, 2), &byte)))
        {
            errorExit("Cannot parse string (hex): %s", str.c_str());
        }
        bytes.push_back(byte);
        p += 2;
    }

    return bytes;
}

static bool parseHexRecord(const std::vector<uint8_t> bytes, HexRecord *hexRecord)
{
    if (bytes.size() < 5) return false;
    
    uint8_t crc = 0;
    for (size_t i = 0; i < bytes.size(); ++i) crc += bytes[i];
    if (crc != 0) return false;

    size_t dataLength = bytes[0];
    if (dataLength + 5 != bytes.size()) return false;

    hexRecord->offset = ((uint32_t)bytes[1] << 8) | (uint32_t)bytes[2];
    hexRecord->type = bytes[3];
    hexRecord->data = std::vector<uint8_t>(bytes.data() + 4, bytes.data() + 4 + dataLength);

    return true;
}

void hexFileLoad(const std::string &filePath, FirmwareImage *firmwareImage) 
{
    FILE *file = fopen(filePath.c_str(), "rt");
    if (file == nullptr)
    {
        errorExit("File open error: %s", filePath.c_str());
    }

    uint32_t baseAddress = 0;
    bool endRecord = false;

    while (!feof(file) && !endRecord)
    {
        std::string str = readString(file);
        if (ferror(file))
        {
            errorExit("File read error: %s", filePath.c_str());
        }
        if (str.empty()) continue;

        std::vector<uint8_t> bytes = parseString(str);

        HexRecord hexRecord;
        if (!parseHexRecord(bytes, &hexRecord))
        {
            errorExit("String crc error: %s", str.c_str());
        }

        switch (hexRecord.type)
        {
        case 0x00:
            {
                if ((hexRecord.offset & 3) != 0)
                {
                    errorExit("Offset error: %s", str.c_str());
                }
                uint32_t address = baseAddress + hexRecord.offset / 2;

                if ((hexRecord.data.size() & 3) != 0)
                {
                    errorExit("Data length error: %s", str.c_str());
                }

                size_t count = hexRecord.data.size() / 4;
                const uint8_t *p = hexRecord.data.data();
                for (size_t i = 0; i < count; ++i)
                {
                    uint32_t data = (uint32_t)*(p++);
                    data |= (uint32_t)*(p++) << 8;
                    data |= (uint32_t)*(p++) << 16;
                    data |= (uint32_t)*(p++) << 24;
                    firmwareImage->setData(address, data);
                    address += 2;
                }
            }
            break;
        case 0x01:
            endRecord = true;
            break;
        case 0x04:
            if ((hexRecord.offset != 0) || (hexRecord.data.size() != 2))
            {
                errorExit("String format error: %s", str.c_str());
            }
            baseAddress = (((uint32_t)hexRecord.data[0] << 8) | (uint32_t)hexRecord.data[1]) << 15;
            break;
        default:
            errorExit("Unknown record type: %s", str.c_str());
        }
    }

    if (!endRecord)
    {
        errorExit("End of file marker is not found: %s", filePath.c_str());
    }

    fclose(file);
}
