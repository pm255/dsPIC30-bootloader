#ifndef __DEVICECONNECTION_H_INCLUDED_
#define __DEVICECONNECTION_H_INCLUDED_

#include "PacketTransiver.h"

struct BootloaderParams
{
    uint32_t address;
    uint32_t size;
};

struct DeviceConnectionStatistic
{
    unsigned programMemoryEraseCount = 0;
    unsigned programMemoryProgramCount = 0;
    unsigned dataEEPROMEraseCount = 0;
    unsigned dataEEPROMProgramCount = 0;
};

class DeviceConnection
{
public:

	DeviceConnection(const std::shared_ptr<SerialPort> &serialPort);
	~DeviceConnection();

    void startCommunication(unsigned timeout); // timeout in seconds (0 = infinite)
    const BootloaderParams &bootloaderParams() const;
    unsigned connectionTime() const; // ms
    const DeviceConnectionStatistic &connectionStatistic() const;

    // reads ROW_SIZE_PROGRAM row by address
    // address must be aligned by ROW_SIZE_PROGRAM
    std::vector<uint32_t> readRow(uint32_t address);

    void writeProgramMemory(uint32_t address, const std::vector<uint32_t> &row, bool program, bool force);
    void writeDataEEPROM(uint32_t address, const std::vector<uint32_t> &row, bool program, bool force);
    void startFirmware();

private:

    std::shared_ptr<SerialPort> _serialPort;
    std::shared_ptr<PacketTransiver> _packetTransiver;

    BootloaderParams _bootloaderParams;
    unsigned _startTime;
    DeviceConnectionStatistic _connectionStatistic;

    // received packet is in _packetTransiver
    void requestResponse(const void *requestData, size_t requestSize, size_t responseSize);

    static std::string writeStatusErrorToString(uint8_t status);

};

#endif // !__DEVICECONNECTION_H_INCLUDED_
