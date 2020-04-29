#include "Stable.h"
#include "SerialPort.h"
#include "ErrorExit.h"

SerialPort::SerialPort()
{
}

SerialPort::~SerialPort()
{
    close();
}

void SerialPort::open(const std::string &portName)
{
    assert(_handle == INVALID_HANDLE_VALUE);

    _portName = portName;
    _handle = CreateFile(("\\\\.\\" + _portName).c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (_handle == INVALID_HANDLE_VALUE)
    {
        errorExit("Serial port open error (%s)", _portName.c_str());
    }

    DCB dcb;
    if (!GetCommState(_handle, &dcb))
    {
        errorExit("Serial port setup error (%s)", _portName.c_str());
    }
    dcb.DCBlength = sizeof dcb;
    dcb.BaudRate = CBR_115200;
    dcb.fBinary = 1;
    dcb.fParity = 0;
    dcb.fOutxCtsFlow = 0;
    dcb.fOutxDsrFlow = 0;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fDsrSensitivity = 0;
    dcb.fTXContinueOnXoff = 0;
    dcb.fOutX = 0;
    dcb.fInX = 0;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fAbortOnError = 0;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    if (!SetCommState(_handle, &dcb))
    {
        errorExit("Serial port setup error (%s)", _portName.c_str());
    }

    if (!SetupComm(_handle, 0x010000, 0x10000))
    {
        errorExit("Serial port setup error (%s)", _portName.c_str());
    }

    COMMTIMEOUTS ct;
    ct.ReadIntervalTimeout = MAXDWORD;
    ct.ReadTotalTimeoutMultiplier = MAXDWORD;
    ct.ReadTotalTimeoutConstant = 10;
    ct.WriteTotalTimeoutMultiplier = 0;
    ct.WriteTotalTimeoutConstant = 500;
    if (!SetCommTimeouts(_handle, &ct))
    {
        errorExit("Serial port setup error (%s)", _portName.c_str());
    }
}

void SerialPort::close()
{
    if (_handle == INVALID_HANDLE_VALUE) return;

    if (!CloseHandle(_handle))
    {
        errorExit("Serial port close error (%s)", _portName.c_str());
    }

    _handle = INVALID_HANDLE_VALUE;
}

bool SerialPort::read(uint8_t *data)
{
    assert(_handle != INVALID_HANDLE_VALUE);

    DWORD length;
    if (!ReadFile(_handle, data, 1, &length, NULL))
    {
        errorExit("Serial port read error (%s)", _portName.c_str());
    }

    return length == 1;
}

void SerialPort::purge()
{
    assert(_handle != INVALID_HANDLE_VALUE);

    if (!PurgeComm(_handle, PURGE_RXCLEAR))
    {
        errorExit("Serial port purge error (%s)", _portName.c_str());
    }
}

void SerialPort::write(const void *buffer, size_t size)
{
    assert(_handle != INVALID_HANDLE_VALUE);

    DWORD length;
    if (!WriteFile(_handle, buffer, size, &length, NULL) || (size != length))
    {
        errorExit("Serial port write error (%s)", _portName.c_str());
    }
}

void SerialPort::flush()
{
    assert(_handle != INVALID_HANDLE_VALUE);

    if (!FlushFileBuffers(_handle))
    {
        errorExit("Serial port write error (%s)", _portName.c_str());
    }
}
