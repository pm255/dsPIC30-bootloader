#ifndef __SERIALPORT_H_INCLUDED_
#define __SERIALPORT_H_INCLUDED_

class SerialPort
{
public:

    SerialPort();
    ~SerialPort();

    void open(const std::string &portName);
    void close();

    // return false if timeout
    bool read(uint8_t *data);
    void purge();

    void write(const void *buffer, size_t size);
    void flush();

private:

    std::string _portName;
    HANDLE _handle = INVALID_HANDLE_VALUE; // INVALID_HANDLE_VALUE if the port in not open

};

#endif // !__SERIALPORT_H_INCLUDED_
