// ========== Configuration bits start ==========
// FOSC
#pragma config FOSFPR = FRC             // Oscillator (Internal Fast RC (No change to Primary Osc Mode bits))
#pragma config FCKSMEN = CSW_FSCM_OFF   // Clock Switching and Monitor (Sw Disabled, Mon Disabled)

// FWDT
#pragma config FWPSB = WDTPSB_1        // WDT Prescaler B (1:1)
#pragma config FWPSA = WDTPSA_1        // WDT Prescaler A (1:1)
#pragma config WDT = WDT_ON            // Watchdog Timer (Enabled)

// FBORPOR
#pragma config FPWRT = PWRT_64          // POR Timer Value (64ms)
#pragma config BODENV = BORV_27         // Brown Out Voltage (2.7V)
#pragma config BOREN = PBOR_OFF         // PBOR Enable (Disabled)
#pragma config MCLRE = MCLR_EN          // Master Clear Enable (Enabled)

// FBS
#pragma config BWRP = WR_PROTECT_BOOT_OFF// Boot Segment Program Memory Write Protect (Boot Segment Program Memory may be written)
#pragma config BSS = NO_BOOT_CODE       // Boot Segment Program Flash Memory Code Protection (No Boot Segment)
#pragma config EBS = NO_BOOT_EEPROM     // Boot Segment Data EEPROM Protection (No Boot EEPROM)
#pragma config RBS = NO_BOOT_RAM        // Boot Segment Data RAM Protection (No Boot RAM)

// FSS
#pragma config SWRP = WR_PROT_SEC_OFF   // Secure Segment Program Write Protect (Disabled)
#pragma config SSS = NO_SEC_CODE        // Secure Segment Program Flash Memory Code Protection (No Secure Segment)
#pragma config ESS = NO_SEC_EEPROM      // Secure Segment Data EEPROM Protection (No Segment Data EEPROM)
#pragma config RSS = NO_SEC_RAM         // Secure Segment Data RAM Protection (No Secure RAM)

// FGS
#pragma config GWRP = GWRP_OFF          // General Code Segment Write Protect (Disabled)
#pragma config GCP = GSS_OFF            // General Segment Code Protection (Disabled)

// FICD
#pragma config ICS = ICS_PGD            // Comm Channel Select (Use PGC/EMUC and PGD/EMUD)
// ========== Configuration bits end ==========

// === FCY ===
// FCY = FOSC / 4
#define FCY (7370000UL / 4) // Internal Fast RC Oscillator, FOSC = 7.37 MHz

// === UART ===
// The UART port number and the pin selection shall be defined.
// #define BOOT_UART UART1
#define BOOT_UART UART2
// #define BOOT_UART_ALTIO

// === Watchdog timer ===
// If the watchdog timer is enabled in the FUSES, the WDT_ENABLED shall be defined.
#define WDT_ENABLED

// === Led ===
// If LED_USED is defined the led blinks during startup:
// 300 ms - ON
// 100 ms - OFF
// 100 ms - ON
// 100 ms - OFF
// ...
#define LED_USED
#define LED_TRIS TRISBbits.TRISB0
#define LED_LAT LATBbits.LATB0
#define LED_ON 1
#define LED_OFF 0

// === Waiting time at startup ===
// The bootloader firmware waits a connection on the serial port during this period (in milliseconds).
// After the period is expired and no connection is obtained the target firmware is started.
#define WAIT_DELAY_MS 5000 // 5 seconds
