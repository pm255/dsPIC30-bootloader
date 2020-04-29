#include "Stable.h"
#include "PacketTransiver.h"


PacketTransiver::PacketTransiver(const std::shared_ptr<SerialPort> &serialPort):
    _serialPort(serialPort)
{
}


PacketTransiver::~PacketTransiver()
{
}

bool PacketTransiver::pool()
{
    unsigned startTime = GetTickCount();

    while (GetTickCount() - startTime < 500)
    {
        uint8_t byte;
        if (!_serialPort->read(&byte)) return false;

        if (byte == 0xAE)
        {
            _rxState = RX_STATE_LENGTH;
            _rxAD = false;
            continue;
        }

        if (_rxState == RX_STATE_HEADER) continue;

        if (_rxAD)
        {
            _rxAD = false;
            switch (byte)
            {
            case 0x00:
                byte = 0xAD;
                break;
            case 0x01:
                byte = 0xAE;
                break;
            default:
                _rxState = RX_STATE_HEADER;
                continue;
            }
        }
        else if (byte == 0xAD)
        {
            _rxAD = true;
            continue;
        }

        switch (_rxState)
        {
        case RX_STATE_HEADER:
            break;
        case RX_STATE_LENGTH:
            _rxState = RX_STATE_DATA;
            _rxSize = byte;
            _receivedPacket.clear();
            if (_rxSize == 0) _rxState = RX_STATE_HEADER;
            break;
        case RX_STATE_DATA:
            _receivedPacket.push_back(byte);
            if (_receivedPacket.size() == _rxSize) _rxState = RX_STATE_CRC_LSB;
            break;
        case RX_STATE_CRC_LSB:
            _rxState = RX_STATE_CRC_MSB;
            _receivedPacket.push_back(byte);
            break;
        case RX_STATE_CRC_MSB:
            _rxState = RX_STATE_HEADER;
            _receivedPacket.push_back(byte);
            if (crc16(_receivedPacket) == 0)
            {
                _receivedPacket.resize(_rxSize);
                return true;
            }
            break;
        }
    }

    return false;
}

const std::vector<uint8_t> &PacketTransiver::receivedPacket() const
{
    return _receivedPacket;
}

void PacketTransiver::sendPacket(const std::vector<uint8_t> &data)
{
    std::vector<uint8_t> buffer;
    buffer.reserve(0x100);

    buffer.push_back(0xAE);
    pushByteAD((uint8_t)data.size(), &buffer);

    for (uint8_t byte : data)
    {
        pushByteAD(byte, &buffer);
    }

    uint16_t crc = crc16(data);
    pushByteAD((uint8_t)crc, &buffer);
    pushByteAD((uint8_t)(crc >> 8), &buffer);

    _serialPort->write(buffer.data(), buffer.size());
    _serialPort->flush();
}

uint16_t PacketTransiver::crc16(const std::vector<uint8_t> &data)
{
    uint16_t crc = 0xFFFF;

    for (uint8_t byte : data)
    {
        crc ^= byte;
        for (unsigned b = 0; b < 8; ++b)
        {
            if ((crc & 0x0001) != 0)
            {
                crc >>= 1;
                crc ^= 0x8408;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc;
}

void PacketTransiver::pushByteAD(uint8_t data, std::vector<uint8_t> *buffer)
{
    if (data == 0xAD)
    {
        buffer->push_back(0xAD);
        buffer->push_back(0x00);
    }
    else if (data == 0xAE)
    {
        buffer->push_back(0xAD);
        buffer->push_back(0x01);
    }
    else
    {
        buffer->push_back(data);
    }
}

