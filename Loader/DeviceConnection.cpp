#include "Stable.h"
#include "DeviceConnection.h"
#include "MemoryLayout.h"
#include "ErrorExit.h"

const uint8_t
    REQUEST_MASK_FORCE = 0x01,
    REQUEST_MASK_PROGRAM = 0x02,
    REQUEST_MASK_PROGRAM_MEMORY = 0x08,
    REQUEST_MASK_DATA_EEPROM = 0x10;
    
const uint8_t
    MODIFY_STATUS_MASK_ERASE_DONE = 0x01,
    MODIFY_STATUS_MASK_ERROR_ERASE = 0x02,
    MODIFY_STATUS_MASK_PROGRAM_DONE = 0x04,
    MODIFY_STATUS_MASK_ERROR_PROGRAM = 0x08;

struct StartCommunicationResponse
{
    uint8_t responseId; // 0xFF
    uint8_t protocolVersion; // 1
    uint8_t signature[8]; // dsPIC30F
    uint16_t bootloaderSize;
    uint32_t bootloaderBaseAddress;
};

struct ReadFlashMemoryRequest
{
    uint8_t requestId; // 0x01
    uint8_t tblpag;
    uint16_t offset;
};

struct ReadFlashMemoryResponse
{
    uint8_t responseId; // 0xFE
    uint8_t data[96];
};

struct StartFirmwareResponse
{
    uint8_t responseId; // 0xF0
};

struct ModifyFlashMemoryRequest
{
    uint8_t requestId; // REQUEST_MASK*
    uint8_t tblpag;
    uint16_t offset;
    uint8_t data[96];
};

struct ModifyFlashMemoryResponse
{
    uint8_t responseId; // 0xFF - WriteFlashMemoryRequest.requestId
    uint8_t status;
};

DeviceConnection::DeviceConnection(const std::shared_ptr<SerialPort> &serialPort):
    _serialPort(serialPort)
{
    _packetTransiver = std::make_shared<PacketTransiver>(_serialPort);
}

DeviceConnection::~DeviceConnection()
{
}

void DeviceConnection::startCommunication(unsigned timeout)
{
    std::vector<uint8_t> startCommunicationRequest;
    startCommunicationRequest.push_back(0x00);

    _serialPort->purge();

    unsigned startTime = GetTickCount();
    while ((timeout == 0) || (GetTickCount() - startTime < timeout * 1000))
    {
        _packetTransiver->sendPacket(startCommunicationRequest);

        if (_packetTransiver->pool())
        {
            if (_packetTransiver->receivedPacket().size() < sizeof(StartCommunicationResponse)) continue;
            StartCommunicationResponse *startCommunicationResponse = (StartCommunicationResponse*)_packetTransiver->receivedPacket().data();
            if (startCommunicationResponse->responseId != 0xFF) continue;
            if (startCommunicationResponse->protocolVersion != 1)
            {
                errorExit("Unsupported protocol version (%u)", (unsigned)startCommunicationResponse->protocolVersion);
            }
            if (memcmp(startCommunicationResponse->signature, "dsPIC30F", 8) != 0)
            {
                errorExit("Unsupported device serial");
            }

            _bootloaderParams.address = startCommunicationResponse->bootloaderBaseAddress;
            _bootloaderParams.size = startCommunicationResponse->bootloaderSize;
            _startTime = GetTickCount();

            return;
        }
    }

    errorExit("Connection timeout");
}

const BootloaderParams &DeviceConnection::bootloaderParams() const
{
    return _bootloaderParams;
}

unsigned DeviceConnection::connectionTime() const
{
    return GetTickCount() - _startTime;
}

const DeviceConnectionStatistic &DeviceConnection::connectionStatistic() const
{
    return _connectionStatistic;
}

std::vector<uint32_t> DeviceConnection::readRow(uint32_t address)
{
    assert((address & ((ROW_SIZE_PROGRAM - 1) | 0xFF000000)) == 0);

    ReadFlashMemoryRequest request;
    memset(&request, 0, sizeof request);
    request.requestId = 0x01;
    request.tblpag = (uint8_t)(address >> 16);
    request.offset = (uint16_t)address;

    requestResponse(&request, sizeof request, sizeof ReadFlashMemoryResponse);
    const ReadFlashMemoryResponse *readFlashMemoryResponse =
        (const ReadFlashMemoryResponse*)_packetTransiver->receivedPacket().data();

    std::vector<uint32_t> result(ROW_SIZE_PROGRAM / 2);
    const uint8_t *p = readFlashMemoryResponse->data;
    for (size_t i = 0; i < ROW_SIZE_PROGRAM / 2; ++i)
    {
        uint32_t x = *(p++);
        x |= (*(p++) << 8);
        x |= (*(p++) << 16);
        result[i] = x;
    }

    return result;
}

void DeviceConnection::writeProgramMemory(uint32_t address, const std::vector<uint32_t> &row, bool program, bool force)
{
    assert((address & (ROW_SIZE_PROGRAM - 1)) == 0);
    assert(!program || (row.size() == ROW_SIZE_PROGRAM / 2));

    ModifyFlashMemoryRequest request;
    memset(&request, 0, sizeof request);
    request.requestId =
        REQUEST_MASK_PROGRAM_MEMORY
        | (program ? REQUEST_MASK_PROGRAM : 0x00)
        | (force ? REQUEST_MASK_FORCE : 0x00);
    request.tblpag = (uint8_t)(address >> 16);
    request.offset = (uint16_t)address;

    uint8_t *p = request.data;
    if (program)
    {
        for (uint32_t i = 0; i < ROW_SIZE_PROGRAM / 2; ++i)
        {
            uint32_t x = row[i];
            *(p++) = (uint8_t)(x >> 0);
            *(p++) = (uint8_t)(x >> 8);
            *(p++) = (uint8_t)(x >> 16);
        }
    }

    requestResponse(&request, p - (uint8_t*)&request, sizeof  ModifyFlashMemoryResponse);
    const ModifyFlashMemoryResponse *modifyFlashMemoryResponse =
        (const ModifyFlashMemoryResponse*)_packetTransiver->receivedPacket().data();

    if ((modifyFlashMemoryResponse->status & MODIFY_STATUS_MASK_ERASE_DONE) != 0)
    {
        ++_connectionStatistic.programMemoryEraseCount;
    }
    if ((modifyFlashMemoryResponse->status & MODIFY_STATUS_MASK_PROGRAM_DONE) != 0)
    {
        ++_connectionStatistic.programMemoryProgramCount;
    }

    if ((modifyFlashMemoryResponse->status & (MODIFY_STATUS_MASK_ERROR_ERASE | MODIFY_STATUS_MASK_ERROR_PROGRAM)) != 0)
    {
        errorExit("Error executing the program memory operation: %s", writeStatusErrorToString(modifyFlashMemoryResponse->status).c_str());
    }
}

void DeviceConnection::writeDataEEPROM(uint32_t address, const std::vector<uint32_t> &row, bool program, bool force)
{
    assert((address & (ROW_SIZE_DATA - 1)) == 0);
    assert(!program || (row.size() == ROW_SIZE_DATA / 2));

    ModifyFlashMemoryRequest request;
    memset(&request, 0, sizeof request);
    request.requestId =
        REQUEST_MASK_DATA_EEPROM
        | (program ? REQUEST_MASK_PROGRAM : 0x00)
        | (force ? REQUEST_MASK_FORCE : 0x00);
    request.tblpag = (uint8_t)(address >> 16);
    request.offset = (uint16_t)address;

    uint8_t *p = request.data;
    if (program)
    {
        for (uint32_t i = 0; i < ROW_SIZE_DATA / 2; ++i)
        {
            uint32_t x = row[i];
            *(p++) = (uint8_t)(x >> 0);
            *(p++) = (uint8_t)(x >> 8);
        }
    }

    requestResponse(&request, p - (uint8_t*)&request, sizeof ModifyFlashMemoryResponse);
    const ModifyFlashMemoryResponse *modifyFlashMemoryResponse =
        (const ModifyFlashMemoryResponse*)_packetTransiver->receivedPacket().data();

    if ((modifyFlashMemoryResponse->status & MODIFY_STATUS_MASK_ERASE_DONE) != 0)
    {
        ++_connectionStatistic.dataEEPROMEraseCount;
    }
    if ((modifyFlashMemoryResponse->status & MODIFY_STATUS_MASK_PROGRAM_DONE) != 0)
    {
        ++_connectionStatistic.dataEEPROMProgramCount;
    }

    if ((modifyFlashMemoryResponse->status & (MODIFY_STATUS_MASK_ERROR_ERASE | MODIFY_STATUS_MASK_ERROR_PROGRAM)) != 0)
    {
        errorExit("Error executing the data EEPROM operation: %s", writeStatusErrorToString(modifyFlashMemoryResponse->status).c_str());
    }
}

void DeviceConnection::startFirmware()
{
    uint8_t requestId = 0x03;
    requestResponse(&requestId, sizeof requestId, 1);
}

void DeviceConnection::requestResponse(const void *requestData, size_t requestSize, size_t responseSize)
{
    std::vector<uint8_t> request((const uint8_t*)requestData, (const uint8_t*)requestData + requestSize);

    for (unsigned i = 0; i < 3; ++i)
    {
        _packetTransiver->sendPacket(request);

        unsigned startTime = GetTickCount();
        while (GetTickCount() - startTime < 500)
        {
            if (_packetTransiver->pool())
            {
                if (_packetTransiver->receivedPacket()[0] != (~request[0] & 0xFF)) continue;
                if (_packetTransiver->receivedPacket().size() != responseSize)
                {
                    errorExit("Wrong size for response code 0x%02X", (unsigned)_packetTransiver->receivedPacket()[0]);
                }
                return;
            }
        }

        printf("*");
        Sleep(1000);
        _serialPort->purge();
    }

    errorExit("No answer from request code 0x%02X", (unsigned)request[0]);
}

std::string DeviceConnection::writeStatusErrorToString(uint8_t status)
{
    if (status & MODIFY_STATUS_MASK_ERROR_ERASE) return "erase error";
    else if (status & MODIFY_STATUS_MASK_ERROR_PROGRAM) return "program error";

    assert(false);

    return "unknown";
}
