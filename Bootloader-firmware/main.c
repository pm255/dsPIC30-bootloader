#include <xc.h>
#include <stdlib.h>
#include <stdbool.h>

#ifndef CONFIG_FILE
#error CONFIG_FILE shall be defined
#endif

#include CONFIG_FILE

#ifndef BOOT_UART
#error BOOT_UART port shall be defined in the config file
#endif

#ifdef BOOT_UART_ALTIO
#define UART_ALTIO (1 << 10)
#else
#define UART_ALTIO 0
#endif

#ifdef LED_USED

#ifndef LED_TRIS
#error LED_TRIS shall be defined in the config file if LED_USED is defined
#endif

#ifndef LED_LAT
#error LED_LAT shall be defined in the config file if LED_USED is defined
#endif

#ifndef LED_ON
#error LED_ON shall be defined in the config file if LED_USED is defined
#endif

#ifndef LED_OFF
#error LED_OFF shall be defined in the config file if LED_USED is defined
#endif

#endif // LED_USED

#ifndef WAIT_DELAY_MS
#error WAIT_DELAY_MS period shall be defined in the config file
#endif

#define BUFFER_SIZE 128

#define PROGRAM_MEMORY_ROW_SIZE 64
#define DATA_EEPROM_ROW_SIZE 32

enum RXState
{
    RX_STATE_HEADER,
    RX_STATE_LENGTH,
    RX_STATE_DATA,
    RX_STATE_CRC_LSB,
    RX_STATE_CRC_MSB
};

#define REQUEST_MASK_FORCE 0x01
#define REQUEST_MASK_PROGRAM 0x02
#define REQUEST_MASK_PROGRAM_MEMORY 0x08
#define REQUEST_MASK_DATA_EEPROM 0x10

#define MODIFY_STATUS_MASK_ERASE_DONE 0x01
#define MODIFY_STATUS_MASK_ERROR_ERASE 0x02
#define MODIFY_STATUS_MASK_PROGRAM_DONE 0x04
#define MODIFY_STATUS_MASK_ERROR_PROGRAM 0x08

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

static enum RXState rxState = RX_STATE_HEADER;
static bool rxAD = false; // if the previous byte is 0xAD
static unsigned rxSize; 

static union
{
    uint8_t bytes[BUFFER_SIZE];
    struct StartCommunicationResponse startCommunicationResponse;
    struct ReadFlashMemoryRequest readFlashMemoryReqest;
    struct ReadFlashMemoryResponse readFlashMemoryResponse;
    struct ModifyFlashMemoryRequest modifyFlashMemoryRequest;
    struct ModifyFlashMemoryResponse modifyFlashMemoryResponse;
} buffer;
static unsigned bufferSize;

static uint16_t crc;

static bool communicationStarted = false;

extern void BOOTLOADER_BASE_ADDRESS(void);
extern void BOOTLOADER_SIZE(void);

static uint32_t bootloaderBaseAddress;
static uint16_t bootloaderSize;

static inline void crc_init(void)
{
    crc = 0xFFFF;
}

static void crcAppendByte(uint8_t byte)
{
    unsigned i;
    crc ^= byte;
    for (i = 0; i < 8; ++i)
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

static void uartWrite(uint8_t data)
{
    while ((BOOT_UART.uxsta & (1 << 9)) != 0) // UTXBF
    {
        // an empty loop
#ifdef WDT_ENABLED
        __builtin_clrwdt();
#endif
    }
    
    BOOT_UART.uxtxreg = data;
}

static void uartWriteWithAD(uint8_t data)
{
    if (data == 0xAD)
    {
        uartWrite(0xAD);
        uartWrite(0x00);
    }
    else if (data == 0xAE)
    {
        uartWrite(0xAD);
        uartWrite(0x01);
    }
    else
    {
        uartWrite(data);
    }
}

static void writeResponse(void)
{
    unsigned i;
    crc_init();
    
    uartWrite(0xAE);
    uartWriteWithAD(bufferSize);
    
    for (i = 0; i < bufferSize; ++i)
    {
        crcAppendByte(buffer.bytes[i]);
        uartWriteWithAD(buffer.bytes[i]);
    }
    
    uartWriteWithAD(crc);
    uartWriteWithAD(crc >> 8);
}

// TBLPAG should be set
static bool testErasedProgramMemory(void)
{
    unsigned i;
    uint16_t offset = buffer.modifyFlashMemoryRequest.offset;
            
    for (i = 0; i < PROGRAM_MEMORY_ROW_SIZE / 2; ++i)
    {
        if ((__builtin_tblrdl(offset) != 0xFFFF)
            || (__builtin_tblrdh(offset) != 0x00FF))
        {
            return false;
        }
        offset += 2;
    }
    
    return true;
}

// TBLPAG should be set
static bool testProgramMemory(void)
{
    uint16_t data;
    unsigned i;
    uint16_t offset = buffer.modifyFlashMemoryRequest.offset;
    uint8_t *p = buffer.modifyFlashMemoryRequest.data;

    for (i = 0; i < PROGRAM_MEMORY_ROW_SIZE / 2; ++i)
    {
        data = *(p++);
        data |= (uint16_t)*(p++) << 8;
        if ((__builtin_tblrdl(offset) != data)
            || (__builtin_tblrdh(offset) != *(p++)))
        {
            return false;
        }
        offset += 2;
    }
    
    return true;
}

// TBLPAG should be set
static bool testErasedDataEEPROM(void)
{
    unsigned i;
    uint16_t offset = buffer.modifyFlashMemoryRequest.offset;
            
    for (i = 0; i < DATA_EEPROM_ROW_SIZE / 2; ++i)
    {
        if (__builtin_tblrdl(offset) != 0xFFFF) return false;
        offset += 2;
    }
    
    return true;
}

// TBLPAG should be set
static bool testDataEEPROM(void)
{
    uint16_t data;
    unsigned i;
    uint16_t offset = buffer.modifyFlashMemoryRequest.offset;
    uint16_t *p = (uint16_t*)buffer.modifyFlashMemoryRequest.data;

    for (i = 0; i < DATA_EEPROM_ROW_SIZE / 2; ++i)
    {
        data = *(p++);
        if (__builtin_tblrdl(offset) != data) return false;
        offset += 2;
    }
    
    return true;
}

static void startCommunicationPacket(void)
{
    buffer.startCommunicationResponse.responseId = 0xFF;
    buffer.startCommunicationResponse.protocolVersion = 1;
    buffer.startCommunicationResponse.signature[0] = 'd';
    buffer.startCommunicationResponse.signature[1] = 's';
    buffer.startCommunicationResponse.signature[2] = 'P';
    buffer.startCommunicationResponse.signature[3] = 'I';
    buffer.startCommunicationResponse.signature[4] = 'C';
    buffer.startCommunicationResponse.signature[5] = '3';
    buffer.startCommunicationResponse.signature[6] = '0';
    buffer.startCommunicationResponse.signature[7] = 'F';
    buffer.startCommunicationResponse.bootloaderSize = bootloaderSize;
    buffer.startCommunicationResponse.bootloaderBaseAddress = bootloaderBaseAddress;
    
    bufferSize = sizeof buffer.startCommunicationResponse;  
    writeResponse();
    
    communicationStarted = true;
}

static void readFlashMemoryPacket(void)
{
    uint16_t offset = buffer.readFlashMemoryReqest.offset;
    uint16_t data;
    unsigned i;
    
    TBLPAG = buffer.readFlashMemoryReqest.tblpag;

    buffer.readFlashMemoryResponse.responseId = 0xFE;
    uint8_t *p = buffer.readFlashMemoryResponse.data;
    for (i = 0; i < PROGRAM_MEMORY_ROW_SIZE / 2; ++i)
    {
        data = __builtin_tblrdl(offset);
        *(p++) = data;
        *(p++) = (data >> 8);
        data = __builtin_tblrdh(offset);
        *(p++) = data;
        offset += 2;
    }
    
    bufferSize = sizeof buffer.readFlashMemoryResponse;
    writeResponse();
}

static void startFirmware(void)
{
    // restore peripheral register values
    BOOT_UART.uxsta = 0x0000;
    BOOT_UART.uxmode = 0x0000;
    BOOT_UART.uxbrg = 0x0000;
    T1CON = 0x0000;
    PR1 = 0x0000;
	
#ifdef LED_USED
    LED_TRIS = 1;
	LED_LAT = 0;
#endif

    ADPCFG = 0x0000;
    
    // clear interrupt flags
    IFS0bits.T1IF = 0;
    
#ifdef WDT_ENABLED
        __builtin_clrwdt();
#endif

    asm volatile(
        "mov #800, w15\n" // restore default SP value
        "goto _BOOTLOADER_BASE_ADDRESS\n"
    );    
}

static void startFirmwarePacket(void)
{
    buffer.bytes[0] = 0xFC;
    bufferSize = 1;
    writeResponse();

    // flush UART
    while ((BOOT_UART.uxsta & (1 << 8)) == 0) // test TRMT
    {
        // an empty loop
#ifdef WDT_ENABLED
        __builtin_clrwdt();
#endif
    }
    
    startFirmware();
}

static uint8_t modifyProgramMemoryInternal(void)
{
    uint16_t offset;
    uint8_t *p;
    uint16_t data;
    unsigned i;
    unsigned status = 0;
      
    TBLPAG = buffer.modifyFlashMemoryRequest.tblpag;
    
    if (((buffer.modifyFlashMemoryRequest.requestId & REQUEST_MASK_PROGRAM) != 0)
        && ((buffer.modifyFlashMemoryRequest.requestId & REQUEST_MASK_FORCE) == 0)
        && testProgramMemory())
    {
        return status;
    }
    
    if (((buffer.modifyFlashMemoryRequest.requestId & REQUEST_MASK_FORCE) != 0) 
        || !testErasedProgramMemory())
    {
        NVMCON = 0x4041;
        NVMADRU = buffer.modifyFlashMemoryRequest.tblpag;
        NVMADR = buffer.modifyFlashMemoryRequest.offset;
#ifdef WDT_ENABLED
        __builtin_clrwdt();
#endif
        __builtin_write_NVM();
#ifdef WDT_ENABLED
        __builtin_clrwdt();
#endif
        status |= MODIFY_STATUS_MASK_ERASE_DONE;
        if (!testErasedProgramMemory())
        {
            return status | MODIFY_STATUS_MASK_ERROR_ERASE;
        }
    }

    if ((buffer.modifyFlashMemoryRequest.requestId & REQUEST_MASK_PROGRAM) != 0)
    {
        if (((buffer.modifyFlashMemoryRequest.requestId & REQUEST_MASK_FORCE) != 0) 
            || !testProgramMemory())
        {
            NVMCON = 0x4001;
            offset = buffer.modifyFlashMemoryRequest.offset;
            p = buffer.modifyFlashMemoryRequest.data;
            for (i = 0; i < PROGRAM_MEMORY_ROW_SIZE / 2; ++i)
            {
                data = *(p++);
                data |= (uint16_t)*(p++) << 8;
                __builtin_tblwtl(offset, data);
                data = *(p++);
                __builtin_tblwth(offset, data);
                offset += 2;
            }
#ifdef WDT_ENABLED
            __builtin_clrwdt();
#endif
            __builtin_write_NVM();
#ifdef WDT_ENABLED
            __builtin_clrwdt();
#endif
            status |= MODIFY_STATUS_MASK_PROGRAM_DONE;
            if (!testProgramMemory())
            {
                return status | MODIFY_STATUS_MASK_ERROR_PROGRAM;
            }
        }
    }

    return status;
}

static void modifyProgramMemoryPacket(void)
{
    uint8_t status = modifyProgramMemoryInternal();
    buffer.modifyFlashMemoryResponse.responseId = 0xFF - buffer.bytes[0];
    buffer.modifyFlashMemoryResponse.status = status;
    
    bufferSize = sizeof buffer.modifyFlashMemoryResponse;
    writeResponse();
}

static uint8_t modifyDataEEPROMInternal(void)
{
    uint16_t offset;
    uint16_t *p;
    uint16_t data;
    unsigned i;
    unsigned status = 0;
    
    TBLPAG = buffer.modifyFlashMemoryRequest.tblpag;
    
    if (((buffer.modifyFlashMemoryRequest.requestId & REQUEST_MASK_PROGRAM) != 0)
        && ((buffer.modifyFlashMemoryRequest.requestId & REQUEST_MASK_FORCE) == 0)
        && testDataEEPROM())
    {
        return status;
    }
    
    if (((buffer.modifyFlashMemoryRequest.requestId & REQUEST_MASK_FORCE) != 0) 
        || !testErasedDataEEPROM())
    {
        NVMCON = 0x4045;
        NVMADRU = buffer.modifyFlashMemoryRequest.tblpag;
        NVMADR = buffer.modifyFlashMemoryRequest.offset;
#ifdef WDT_ENABLED
        __builtin_clrwdt();
#endif
        __builtin_write_NVM();
        while (NVMCONbits.WR)
        {
            // an empty loop
#ifdef WDT_ENABLED
            __builtin_clrwdt();
#endif
        }
        status |= MODIFY_STATUS_MASK_ERASE_DONE;
        if (!testErasedDataEEPROM())
        {
            return status | MODIFY_STATUS_MASK_ERROR_ERASE;
        }
    }

    if ((buffer.modifyFlashMemoryRequest.requestId & REQUEST_MASK_PROGRAM) != 0)
    {
        if (((buffer.modifyFlashMemoryRequest.requestId & REQUEST_MASK_FORCE) != 0) 
            || !testDataEEPROM())
        {
            NVMCON = 0x4005;
            offset = buffer.modifyFlashMemoryRequest.offset;
            p = (uint16_t*)buffer.modifyFlashMemoryRequest.data;
            for (i = 0; i < DATA_EEPROM_ROW_SIZE / 2; ++i)
            {
                data = *(p++);
                __builtin_tblwtl(offset, data);
                offset += 2;
            }
#ifdef WDT_ENABLED
            __builtin_clrwdt();
#endif
            __builtin_write_NVM();
            while (NVMCONbits.WR)
            {
                // an empty loop
#ifdef WDT_ENABLED
                __builtin_clrwdt();
#endif
            }
            status |= MODIFY_STATUS_MASK_PROGRAM_DONE;
            if (!testDataEEPROM())
            {
                return status | MODIFY_STATUS_MASK_ERROR_PROGRAM;
            }
        }
    }

    return status;
}

static void modifyDataEEPROMPacket(void)
{
    uint8_t status = modifyDataEEPROMInternal();
    buffer.modifyFlashMemoryResponse.responseId = 0xFF - buffer.bytes[0];
    buffer.modifyFlashMemoryResponse.status = status;
    
    bufferSize = sizeof buffer.modifyFlashMemoryResponse;
    writeResponse();
}

static void processInputPacket(void)
{
    if (buffer.bytes[0] == 0x00) startCommunicationPacket();
    else if (communicationStarted)
    {
        if (buffer.bytes[0] == 0x01) readFlashMemoryPacket();
        else if (buffer.bytes[0] == 0x03) startFirmwarePacket();
        else if ((buffer.bytes[0] & REQUEST_MASK_PROGRAM_MEMORY) != 0)
        {
            modifyProgramMemoryPacket();
        }
        else if ((buffer.bytes[0] & REQUEST_MASK_DATA_EEPROM) != 0)
        {
            modifyDataEEPROMPacket();
        }
    }
}

static void rxTask(void)
{
    unsigned data;
    if ((BOOT_UART.uxsta & (1 << 0)) == 0) return; // test URXDA
    data = BOOT_UART.uxrxreg;
    BOOT_UART.uxsta &= ~(1 << 1); // clear receive buffer overrun error bit (OERR)
    
    if (data == 0xAE)
    {
        rxState = RX_STATE_LENGTH;
        rxAD = false;
        return;
    }
    
    if (rxState == RX_STATE_HEADER) return;
    
    if (rxAD)
    {
        rxAD = false;
        switch (data)
        {
            case 0x00:
                data = 0xAD;
                break;
            case 0x01:
                data = 0xAE;
                break;
            default:
                rxState = RX_STATE_HEADER;
                return;
        }
    }
    else if (data == 0xAD)
    {
        rxAD = true;
        return;
    }
    
    switch (rxState)
    {
        case RX_STATE_HEADER:
            break;
        case RX_STATE_LENGTH:
            rxState = RX_STATE_DATA;
            rxSize = data;
            bufferSize = 0;
            crc_init();
            if ((rxSize == 0) || (rxSize > BUFFER_SIZE)) rxState = RX_STATE_HEADER;
            break;
        case RX_STATE_DATA:
            buffer.bytes[bufferSize++] = data;
            crcAppendByte(data);
            if (bufferSize == rxSize) rxState = RX_STATE_CRC_LSB;
            break;
        case RX_STATE_CRC_LSB:
            rxState = RX_STATE_CRC_MSB;
            crcAppendByte(data);
            break;
        case RX_STATE_CRC_MSB:
            rxState = RX_STATE_HEADER;
            crcAppendByte(data);
            if (crc == 0) processInputPacket();
            break;
    }
}

int main(void)
{
    unsigned tickCount = 0;

    ADPCFG = 0xFFFF; // no ADC pins
    
#ifdef LED_USED
    LED_LAT = LED_ON;
    LED_TRIS = 0;
#endif
    
    // Timer 1 initialization
    PR1 = (FCY + (10 * 256) / 2) / (10 * 256) - 1; // period = 100 ms
    T1CON = (3 << 4) | (1 << 15); // TCKPS = 3, TON = 1

    // UART initialization
    BOOT_UART.uxbrg = (FCY  + (16 * 115200UL) / 2) / (16 * 115200UL) - 1;
    BOOT_UART.uxmode = (1 << 15) | UART_ALTIO; // UARTEN = 1, ALTIO
    BOOT_UART.uxsta = (1 << 10); // UTXEN = 1
    
    bootloaderBaseAddress = __builtin_tbladdress(BOOTLOADER_BASE_ADDRESS);
    bootloaderSize = __builtin_tbladdress(BOOTLOADER_SIZE);
	
    TBLPAG = (bootloaderBaseAddress >> 16);
    communicationStarted =
        ((__builtin_tblrdl((uint16_t)bootloaderBaseAddress) == 0xFFFF)
        && (__builtin_tblrdh((uint16_t)bootloaderBaseAddress) == 0x00FF));
    
    while (true)
    {
#ifdef WDT_ENABLED
        __builtin_clrwdt();
#endif

        if (IFS0bits.T1IF != 0)
        {
			++tickCount;
            IFS0bits.T1IF = 0;
			
#ifdef LED_USED
			if (tickCount >= 3) LED_LAT ^= 1;
#endif
			
            if (!communicationStarted && (tickCount == (WAIT_DELAY_MS / 100)))
            {
                startFirmware();
            }
        }
        
        rxTask();
    }
    
    return 0;
}
