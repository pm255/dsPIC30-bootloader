#ifndef __PACKETTRANSIVER_H_INCLUDED_
#define __PACKETTRANSIVER_H_INCLUDED_

#include "SerialPort.h"

enum RXState
{
    RX_STATE_HEADER,
    RX_STATE_LENGTH,
    RX_STATE_DATA,
    RX_STATE_CRC_LSB,
    RX_STATE_CRC_MSB
};

class PacketTransiver
{
public:

	PacketTransiver(const std::shared_ptr<SerialPort> &serialPort);
	~PacketTransiver();

    bool pool();
    const std::vector<uint8_t> &receivedPacket() const;

    void sendPacket(const std::vector<uint8_t> &data);

private:

    std::shared_ptr<SerialPort> _serialPort;

    std::vector<uint8_t> _receivedPacket;
    RXState _rxState = RX_STATE_HEADER;
    bool _rxAD = false;
    size_t _rxSize = 0;

    static uint16_t crc16(const std::vector<uint8_t> &data);
    static void pushByteAD(uint8_t data, std::vector<uint8_t> *buffer);

};

#endif // !__PACKETTRANSIVER_H_INCLUDED_
