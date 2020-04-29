#include "Stable.h"
#include "CommandLineParser.h"
#include "DeviceConnection.h"
#include "FirmwareImage.h"
#include "HexFileLoad.h"
#include "HexFileWriter.h"
#include "ErrorExit.h"
#include "Help.h"

static void loadDeviceMemoryRange(
    const std::shared_ptr<DeviceConnection> &connection,
    const MemoryRange &range,
    std::vector<uint32_t> *memory
)
{
    memory->clear();
    memory->reserve(range.size / 2);
    for (uint32_t address = range.address; address < range.address + range.size; )
    {
        std::vector<uint32_t> row = connection->readRow(address);

        for (unsigned i = 0; i < ROW_SIZE_PROGRAM / 2; ++i)
        {
            memory->push_back(row[i]);
            address += 2;
            if (address == range.address + range.size) break;
        }
        if (address % 1024 == 0) printf(".");
    }
}

static void checkFirmwareImage(
    const MemoryLayout &memoryLayout,
    const FirmwareImage &firmwareImage,
    const std::shared_ptr<DeviceConnection> &connection)
{
    const BootloaderParams &bootloaderParams = connection->bootloaderParams();
    for (uint32_t address = bootloaderParams.address; address < bootloaderParams.address + bootloaderParams.size; address += 2)
    {
        if (firmwareImage.getData(address) != UNDEFINED_WORD)
        {
            errorExit("The target firmware overwrites the bootloader area 0x%06X-0x%06X",
                (unsigned)bootloaderParams.address,
                (unsigned)(bootloaderParams.address + bootloaderParams.size));
        }
    }

    uint32_t data0 = firmwareImage.getData(0);
    uint32_t data2 = firmwareImage.getData(2);
    if (((data0 & 0xFFFF0001) != 0x00040000) || ((data2 & 0xFFFFFF80) != 0))
    {
        errorExit("The instruction at address 0x000000 in the target firmware must be GOTO");
    }

    const MemoryRange &configMemoryRange = memoryLayout.memoryRange(MEMORY_TYPE_CONFIG);
    std::vector<uint32_t> configMemory;
    printf("Reading config memory");
    loadDeviceMemoryRange(connection, configMemoryRange, &configMemory);
    printf("\n");

    for (unsigned i = 0; i < configMemoryRange.size / 2; ++i)
    {
        uint32_t address = configMemoryRange.address + i * 2;
        uint32_t firmwareWord = firmwareImage.getData(address);
        if (firmwareWord == UNDEFINED_WORD) continue;
        firmwareWord &= memoryLayout.configFuseMasks()[i];
        uint32_t deviceWord = configMemory[i];
        if (((firmwareWord ^ deviceWord) & WORD_MASK_CONFIG) != 0)
        {
            errorExit("The config word at address 0x%06X is defferent in the device (0x%04X) and in the target firmware(0x%04X)",
                (unsigned)address, (unsigned)deviceWord, (unsigned)firmwareWord);
        }
    }
}

static void patchFirmwareImage(const BootloaderParams &bootloaderParams, FirmwareImage *firmwareImage)
{
    uint32_t address = bootloaderParams.address;

    // copy first GOTO instruction
    firmwareImage->setData(address, firmwareImage->getData(0));
    firmwareImage->setData(0, UNDEFINED_WORD);
    address += 2;
    firmwareImage->setData(address, firmwareImage->getData(2));
    firmwareImage->setData(2, UNDEFINED_WORD);
    address += 2 + 4;

    // create a jump table
    for (uint32_t i = 4; i < ROW_SIZE_PROGRAM; i += 2)
    {
        uint32_t data = firmwareImage->getData(i);
        firmwareImage->setData(i, UNDEFINED_WORD);
        firmwareImage->setData(address, 0x00040000 | (data & 0x00FFFE));
        address += 2;
        firmwareImage->setData(address, (data >> 16) & 0x00007F);
        address += 2;
    }
}

static void unpatchFirmwareImage(const BootloaderParams &bootloaderParams, FirmwareImage *firmwareImage)
{
    uint32_t address = bootloaderParams.address;

    // copy first GOTO instruction
    firmwareImage->setData(0, firmwareImage->getData(address));
    firmwareImage->setData(address, UNDEFINED_WORD);
    address += 2;
    firmwareImage->setData(2, firmwareImage->getData(address));
    firmwareImage->setData(address, UNDEFINED_WORD);
    address += 2 + 4;

    // parse a jump table
    for (uint32_t i = 4; i < ROW_SIZE_PROGRAM; i += 2)
    {
        uint32_t data = firmwareImage->getData(address) & 0x00FFFE;
        firmwareImage->setData(address, UNDEFINED_WORD);
        address += 2;
        data |= (firmwareImage->getData(address) & 0x00007F) << 16;
        firmwareImage->setData(address, UNDEFINED_WORD);
        address += 2;

        firmwareImage->setData(i, data);
    }
}

static std::vector<uint32_t> getRow(const FirmwareImage &firmwareImage, uint32_t address, uint32_t size)
{
    std::vector<uint32_t> row;
    row.reserve(size / 2);
    while (size != 0)
    {
        row.push_back(firmwareImage.getData(address));
        address += 2;
        size -= 2;
    }

    return row;
}

static bool isRowUndefined(const std::vector<uint32_t> &row)
{
    for (size_t i = 0; i < row.size(); ++i)
    {
        if (row[i] != UNDEFINED_WORD) return false;
    }

    return true;
}

static bool isRowErased(const std::vector<uint32_t> &row, uint32_t mask)
{
    for (size_t i = 0; i < row.size(); ++i)
    {
        if ((row[i] & mask) != mask) return false;
    }

    return true;
}

static bool isRowEquals(const std::vector<uint32_t> &source, const std::vector<uint32_t> &dest, uint32_t mask)
{
    assert(source.size() == dest.size());

    for (size_t i = 0; i < source.size(); ++i)
    {
        uint32_t s = source[i];
        uint32_t d = dest[i];
        if (s == UNDEFINED_WORD) continue;
        if (((s ^ d) & mask) != 0) return false;
    }

    return true;
}

static void errorExitIncompatibleOptions()
{
    errorExit("Incompatible options (use -h to show all available options)");
}

std::shared_ptr<DeviceConnection> connectToDevice(const CommandLineParams &params, const DeviceInfo **deviceInfo)
{
    std::shared_ptr<SerialPort> serialPort = std::make_shared<SerialPort>();
    serialPort->open(params.args[0]);

    printf("Connecting to device...\n");
    std::shared_ptr<DeviceConnection> connection = std::make_shared<DeviceConnection>(serialPort);
    connection->startCommunication(params.timeout);

    const BootloaderParams &bootloaderParams = connection->bootloaderParams();
    printf("Bootloader: address = 0x%06X, size = 0x%X\n", bootloaderParams.address, bootloaderParams.size);

    uint32_t deviceId = connection->readRow(0xFF0000)[0];

    const DeviceInfo *info = getDeviceInfo(deviceId);
    if (info == nullptr)
    {
        errorExit("Unknow device ID (0x%04X)", (unsigned)deviceId);
    }

    printf("Device: %s\n", info->name);

    if (!info->supported)
    {
        errorExit("Device is not supported");
    }

    if (((params.optionMask & OPTION_MASK_MODEL) != 0)
        && (stricmp(info->name, params.model.c_str()) != 0))
    {
        errorExit("Wrong device model");
    }

    *deviceInfo = info;

    return connection;
}

static void printOperationTime(const std::shared_ptr<DeviceConnection> &connection)
{
    unsigned time = connection->connectionTime();
    printf("Total time: %u.%01u sec\n", time / 1000, (time % 1000) / 100);
}

static void printOperationStatistic(const std::shared_ptr<DeviceConnection> &connection)
{
    const DeviceConnectionStatistic &statistic = connection->connectionStatistic();
    printf("Program memory: erase = %u, program = %u\n", statistic.programMemoryEraseCount, statistic.programMemoryProgramCount);
    printf("Data EEPROM: erase = %u, program = %u\n", statistic.dataEEPROMEraseCount, statistic.dataEEPROMProgramCount);
}

static void commandInfo(const CommandLineParams &params)
{
    if ((params.optionMask & ~(OPTION_MASK_INFO | OPTION_MASK_TIMEOUT | OPTION_MASK_MODEL)) != 0)
    {
        errorExitIncompatibleOptions();
    }

    const DeviceInfo *deviceInfo;
    connectToDevice(params, &deviceInfo);
}

static void commandProgram(const CommandLineParams &params)
{
    if ((params.optionMask & ~(OPTION_MASK_PROGRAM | OPTION_MASK_TIMEOUT | OPTION_MASK_ERASE | OPTION_MASK_NO_RUN | OPTION_MASK_FORCE | OPTION_MASK_MODEL)) != 0)
    {
        errorExitIncompatibleOptions();
    }

    const DeviceInfo *deviceInfo;
    std::shared_ptr<DeviceConnection> connection = connectToDevice(params, &deviceInfo);

    MemoryLayout memoryLayout(*deviceInfo);

    printf("Loading hex file...\n");
    FirmwareImage firmwareImage(&memoryLayout);
    hexFileLoad(params.args[1], &firmwareImage);

    checkFirmwareImage(memoryLayout, firmwareImage, connection);
    patchFirmwareImage(connection->bootloaderParams(), &firmwareImage);

    printf("Erasing the jump table");
    bool force = ((params.optionMask & OPTION_MASK_FORCE) != 0);
    bool erase = ((params.optionMask & OPTION_MASK_ERASE) != 0);
    connection->writeProgramMemory(connection->bootloaderParams().address, std::vector<uint32_t>(), false, force);
    printf("\n");

    printf("Programing program memory");
    const MemoryRange &programMemoryRange = memoryLayout.memoryRange(MEMORY_TYPE_PROGRAM);
    const BootloaderParams &bootloaderParams = connection->bootloaderParams();
    for (uint32_t address = programMemoryRange.address; address < programMemoryRange.address + programMemoryRange.size; address += ROW_SIZE_PROGRAM)
    {
        std::vector<uint32_t> firmwareRow = getRow(firmwareImage, address, ROW_SIZE_PROGRAM);
        if ((address != 0x000000) // skip the zero row
            && (address != bootloaderParams.address) // skip the jump table first row
            && ((address < bootloaderParams.address + 2 * ROW_SIZE_PROGRAM) // skip the bootloader image (without the jump table)
                || (address >= bootloaderParams.address + bootloaderParams.size)))
        {
            if (erase || !isRowUndefined(firmwareRow))
            {
                connection->writeProgramMemory(address, firmwareRow, !isRowErased(firmwareRow, WORD_MASK_PROGRAM), force);
            }
        }
        if (address % 1024 == 0) printf(".");
    }
    printf("\n");

    printf("Programing data EEPROM");
    const MemoryRange &dataMemoryRange = memoryLayout.memoryRange(MEMORY_TYPE_DATA);
    for (uint32_t address = dataMemoryRange.address; address < dataMemoryRange.address + dataMemoryRange.size; address += ROW_SIZE_DATA)
    {
        std::vector<uint32_t> firmwareRow = getRow(firmwareImage, address, ROW_SIZE_DATA);
        if (erase || !isRowUndefined(firmwareRow))
        {
            connection->writeDataEEPROM(address, firmwareRow, !isRowErased(firmwareRow, WORD_MASK_DATA), force);
        }
        if (address % 1024 == 0) printf(".");
    }
    printf("\n");

    printf("Programing the jump table");
    connection->writeProgramMemory(connection->bootloaderParams().address,
        getRow(firmwareImage, connection->bootloaderParams().address, ROW_SIZE_PROGRAM),
        true, false /* not force*/);
    printf("\n");

    if ((params.optionMask & OPTION_MASK_NO_RUN) == 0)
    {
        printf("Starting the target firmware\n");
        connection->startFirmware();
    }

    printOperationTime(connection);
    printOperationStatistic(connection);
    printf("Operation has been complete\n");
}

static void commandVerify(const CommandLineParams &params)
{
    if ((params.optionMask & ~(OPTION_MASK_VERIFY | OPTION_MASK_TIMEOUT | OPTION_MASK_MODEL)) != 0)
    {
        errorExitIncompatibleOptions();
    }

    const DeviceInfo *deviceInfo;
    std::shared_ptr<DeviceConnection> connection = connectToDevice(params, &deviceInfo);

    MemoryLayout memoryLayout(*deviceInfo);

    printf("Loading hex file...\n");
    FirmwareImage firmwareImage(&memoryLayout);
    hexFileLoad(params.args[1], &firmwareImage);

    checkFirmwareImage(memoryLayout, firmwareImage, connection);
    patchFirmwareImage(connection->bootloaderParams(), &firmwareImage);

    printf("Verifing program memory");
    const MemoryRange &programMemoryRange = memoryLayout.memoryRange(MEMORY_TYPE_PROGRAM);
    for (uint32_t address = programMemoryRange.address; address < programMemoryRange.address + programMemoryRange.size; address += ROW_SIZE_PROGRAM)
    {
        std::vector<uint32_t> firmwareRow = getRow(firmwareImage, address, ROW_SIZE_PROGRAM);
        if (!isRowUndefined(firmwareRow))
        {
            std::vector<uint32_t> targetRow = connection->readRow(address);
            if (!isRowEquals(firmwareRow, targetRow, WORD_MASK_PROGRAM))
            {
                errorExit("The row at address 0x%06X has different values", address);
            }
        }
        if (address % 1024 == 0) printf(".");
    }
    printf("\n");

    printf("Verifing data EEPROM");
    const MemoryRange &dataMemoryRange = memoryLayout.memoryRange(MEMORY_TYPE_DATA);
    for (uint32_t address = dataMemoryRange.address; address < dataMemoryRange.address + dataMemoryRange.size; address += ROW_SIZE_PROGRAM /* not ROW_SIZE_DATA*/)
    {
        std::vector<uint32_t> firmwareRow = getRow(firmwareImage, address, ROW_SIZE_PROGRAM /* not ROW_SIZE_DATA*/);
        if (!isRowUndefined(firmwareRow))
        {
            std::vector<uint32_t> targetRow = connection->readRow(address);
            if (!isRowEquals(firmwareRow, targetRow, WORD_MASK_DATA))
            {
                errorExit("The row at address 0x%06X has different values", address);
            }
        }
        if (address % 1024 == 0) printf(".");
    }
    printf("\n");

    printOperationTime(connection);
    printf("Verification passed\n");
}

static void commandLoad(const CommandLineParams &params)
{
    if ((params.optionMask & ~(OPTION_MASK_LOAD | OPTION_MASK_TIMEOUT | OPTION_MASK_ALL | OPTION_MASK_NO_SMART | OPTION_MASK_MODEL)) != 0)
    {
        errorExitIncompatibleOptions();
    }

    const DeviceInfo *deviceInfo;
    std::shared_ptr<DeviceConnection> connection = connectToDevice(params, &deviceInfo);

    MemoryLayout memoryLayout(*deviceInfo);
    FirmwareImage firmwareImage(&memoryLayout);

    printf("Loading jump table\n");
    MemoryRange jumpTableRange;
    jumpTableRange.address = connection->bootloaderParams().address;
    jumpTableRange.size = ROW_SIZE_PROGRAM;
    std::vector<uint32_t> jumpTable;
    loadDeviceMemoryRange(connection, jumpTableRange, &jumpTable);

    if (jumpTable[0] == 0x00FFFFFF)
    {
        errorExit("The target has no valid firmware");
    }

    for (uint32_t address = jumpTableRange.address; address < jumpTableRange.address + jumpTableRange.size; address += 2)
    {
        firmwareImage.setData(address, jumpTable[(address - jumpTableRange.address) / 2]);
    }

    printf("Loading program memory");
    const MemoryRange &programMemoryRange = memoryLayout.memoryRange(MEMORY_TYPE_PROGRAM);
    const BootloaderParams &bootloaderParams = connection->bootloaderParams();
    for (uint32_t address = programMemoryRange.address; address < programMemoryRange.address + programMemoryRange.size; address += ROW_SIZE_PROGRAM)
    {
        if ((address != bootloaderParams.address) // already read
            && (((params.optionMask & OPTION_MASK_ALL) != 0) // skip if the bootloader image is not needed
                || ((address != 0x000000) // skip the zero row
                    && ((address < bootloaderParams.address + 2 * ROW_SIZE_PROGRAM) // skip the bootloader image (without the jump table)
                        || (address >= bootloaderParams.address + bootloaderParams.size)))))
        {
            std::vector<uint32_t> row = connection->readRow(address);
            for (size_t i = 0; i < row.size(); ++i)
            {
                firmwareImage.setData(address + i * 2, row[i]);
            }
        }

        if (address % 1024 == 0) printf(".");
    }
    printf("\n");

    printf("Loading data EEPROM");
    const MemoryRange &dataMemoryRange = memoryLayout.memoryRange(MEMORY_TYPE_DATA);
    for (uint32_t address = dataMemoryRange.address; address < dataMemoryRange.address + dataMemoryRange.size; address += ROW_SIZE_PROGRAM /* not ROW_SIZE_DATA*/)
    {
        std::vector<uint32_t> row = connection->readRow(address);
        for (size_t i = 0; i < row.size(); ++i)
        {
            firmwareImage.setData(address + i * 2, row[i]);
        }
        if (address % 1024 == 0) printf(".");
    }
    printf("\n");

    printf("Reading config memory");
    const MemoryRange &configMemoryRange = memoryLayout.memoryRange(MEMORY_TYPE_CONFIG);
    const uint32_t *configMasks = deviceInfo->configMemoryMasks;
    for (uint32_t address = configMemoryRange.address; address < configMemoryRange.address + configMemoryRange.size; address += ROW_SIZE_PROGRAM /* not ROW_SIZE_DATA*/)
    {
        std::vector<uint32_t> row = connection->readRow(address);
        for (size_t i = 0; i < row.size(); ++i)
        {
            if (i * 2 == configMemoryRange.size) break;
            firmwareImage.setData(address + i * 2, (row[i] | ~(*(configMasks++))) & WORD_MASK_CONFIG);
        }
    }
    printf("\n");

    if ((params.optionMask & OPTION_MASK_ALL) == 0)
    {
        unpatchFirmwareImage(connection->bootloaderParams(), &firmwareImage);
    }

    if ((params.optionMask & OPTION_MASK_NO_SMART) == 0)
    {
        for (uint32_t address = programMemoryRange.address; address < programMemoryRange.address + programMemoryRange.size; address += ROW_SIZE_PROGRAM)
        {
            std::vector<uint32_t> row = getRow(firmwareImage, address, ROW_SIZE_PROGRAM);
            if (isRowErased(row, WORD_MASK_PROGRAM))
            {
                for (uint32_t i = 0; i < ROW_SIZE_PROGRAM; i += 2)
                {
                    firmwareImage.setData(address + i, UNDEFINED_WORD);
                }
            }
        }
        for (uint32_t address = dataMemoryRange.address; address < dataMemoryRange.address + dataMemoryRange.size; address += ROW_SIZE_DATA)
        {
            std::vector<uint32_t> row = getRow(firmwareImage, address, ROW_SIZE_DATA);
            if (isRowErased(row, WORD_MASK_DATA))
            {
                for (uint32_t i = 0; i < ROW_SIZE_DATA; i += 2)
                {
                    firmwareImage.setData(address + i, UNDEFINED_WORD);
                }
            }
        }
    }

    HexFileWriter hexFileWrite(params.args[1]);
    hexFileWrite.writeImage(firmwareImage, memoryLayout);

    printOperationTime(connection);
    printf("Firmware image loaded\n");
}

static void commandErase(const CommandLineParams &params)
{
    if ((params.optionMask & ~(OPTION_MASK_ERASE | OPTION_MASK_TIMEOUT | OPTION_MASK_FORCE | OPTION_MASK_MODEL)) != 0)
    {
        errorExitIncompatibleOptions();
    }

    const DeviceInfo *deviceInfo;
    std::shared_ptr<DeviceConnection> connection = connectToDevice(params, &deviceInfo);

    MemoryLayout memoryLayout(*deviceInfo);

    printf("Erasing the jump table");
    bool force = ((params.optionMask & OPTION_MASK_FORCE) != 0);
    connection->writeProgramMemory(connection->bootloaderParams().address, std::vector<uint32_t>(), false, force);
    printf("\n");

    printf("Erasing program memory");
    const MemoryRange &programMemoryRange = memoryLayout.memoryRange(MEMORY_TYPE_PROGRAM);
    const BootloaderParams &bootloaderParams = connection->bootloaderParams();
    for (uint32_t address = programMemoryRange.address; address < programMemoryRange.address + programMemoryRange.size; address += ROW_SIZE_PROGRAM)
    {
        if ((address != 0x000000) // skip the zero row
            && (address != bootloaderParams.address) // skip the jump table first row (already erased)
            && ((address < bootloaderParams.address + 2 * ROW_SIZE_PROGRAM) // skip the bootloader image (without the jump table)
                || (address >= bootloaderParams.address + bootloaderParams.size)))
        {
            connection->writeProgramMemory(address, std::vector<uint32_t>(), false, force);
        }
        if (address % 1024 == 0) printf(".");
    }
    printf("\n");

    printf("Erasing data EEPROM");
    const MemoryRange &dataMemoryRange = memoryLayout.memoryRange(MEMORY_TYPE_DATA);
    for (uint32_t address = dataMemoryRange.address; address < dataMemoryRange.address + dataMemoryRange.size; address += ROW_SIZE_DATA)
    {
        connection->writeDataEEPROM(address, std::vector<uint32_t>(), false, force);
        if (address % 1024 == 0) printf(".");
    }
    printf("\n");

    printOperationTime(connection);
    printOperationStatistic(connection);
    printf("Operation has been complete\n");
}

int main(int argc, char *argv[])
{
    CommandLineParams params;
    commandLineParser(argc, argv, &params);

    if (params.args.empty())
    {
        if ((params.optionMask & ~OPTION_MASK_HELP)) errorExitIncompatibleOptions();
        printf(HELP_TEXT);
    }
    else if (params.args.size() == 1)
    {
        if ((params.optionMask & (OPTION_MASK_PROGRAM | OPTION_MASK_LOAD | OPTION_MASK_ERASE)) == 0)
        {
            commandInfo(params);
        }
        else if ((params.optionMask & OPTION_MASK_ERASE) != 0)
        {
            commandErase(params);
        }
        else errorExitIncompatibleOptions();
    }
    else if (params.args.size() == 2)
    {
        if ((params.optionMask & OPTION_MASK_PROGRAM) != 0)
        {
            commandProgram(params);
        }
        else if ((params.optionMask & OPTION_MASK_VERIFY) != 0)
        {
            commandVerify(params);
        }
        else if ((params.optionMask & OPTION_MASK_LOAD) != 0)
        {
            commandLoad(params);
        }
        else errorExitIncompatibleOptions();
    }
    else
    {
        errorExit("Too many arguments (use -h to show all available options)");
    }

	return 0;
}
