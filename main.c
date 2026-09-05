/**
 * PIC18F458 Temperature Sensor Project
 * Author: Temperature Monitoring System
 * Description: Complete temperature monitoring system with 4x4 keypad,
 *              I2C LCD display, and LED indicators (Low/Medium/High)
 * 
 * Hardware:
 * - Microcontroller: PIC18F458
 * - Temperature Sensor: LM35 (Analog Input RA0/AN0)
 * - LCD: 16x2 I2C module
 * - Keypad: 4x4 Matrix (4 rows, 4 columns)
 * - LEDs: 3 (Low=Green, Medium=Yellow, High=Red)
 */

#include <xc.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// Configuration bits for PIC18F458
#pragma config OSC = HS         // High-speed oscillator
#pragma config FCMEN = OFF      // Fail-Safe Clock Monitor disabled
#pragma config IESO = OFF       // Internal/External Oscillator Switchover Mode disabled
#pragma config PWRT = ON        // Power-up Timer enabled
#pragma config BOREN = ON       // Brown-out Reset enabled
#pragma config BORV = 2         // Brown-out Reset voltage = 2.7V
#pragma config WDT = OFF        // Watchdog Timer disabled
#pragma config WDTPS = 32768    // WDT Postscaler
#pragma config CCP2MX = ON      // CCP2 on RC1
#pragma config STVREN = ON      // Stack Full/Underflow Reset enabled
#pragma config LVP = ON         // Low Voltage Programming enabled
#pragma config ICPRT = OFF      // ICP enabled
#pragma config DEBUG = OFF      // Debug mode disabled

#define _XTAL_FREQ 20000000     // 20MHz Crystal Oscillator

// ============ I2C LCD Definitions ============
#define LCD_I2C_ADDR 0x27       // I2C address of LCD module (typical 0x27 or 0x3F)
#define LCD_COLS 16
#define LCD_ROWS 2

// LCD Control Commands
#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT 0x10
#define LCD_FUNCTIONSET 0x20
#define LCD_SETCGRAMADDR 0x40
#define LCD_SETDDRAMADDR 0x80

// Entry Mode
#define LCD_ENTRYRIGHT 0x00
#define LCD_ENTRYLEFT 0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

// Display Control
#define LCD_DISPLAYON 0x04
#define LCD_DISPLAYOFF 0x00
#define LCD_CURSORON 0x02
#define LCD_CURSOROFF 0x00
#define LCD_BLINKON 0x01
#define LCD_BLINKOFF 0x00

// Backlight Control
#define LCD_BACKLIGHT 0x08
#define LCD_NOBACKLIGHT 0x00

// ============ LED Pin Definitions ============
#define LED_LOW   PORTDbits.RD0   // Green LED - Low Temperature
#define LED_MED   PORTDbits.RD1   // Yellow LED - Medium Temperature
#define LED_HIGH  PORTDbits.RD2   // Red LED - High Temperature

#define TRIS_LED_LOW   TRISDbits.TRISD0
#define TRIS_LED_MED   TRISDbits.TRISD1
#define TRIS_LED_HIGH  TRISDbits.TRISD2

// ============ Keypad Definitions ============
// Rows: RB0-RB3 (outputs)
// Columns: RB4-RB7 (inputs)
#define ROW_PORT PORTB
#define ROW_TRIS TRISB
#define COL_PORT PORTB
#define COL_TRIS TRISB

#define SET_ROW_OUTPUT() { TRISBbits.TRISB0 = 0; TRISBbits.TRISB1 = 0; \
                           TRISBbits.TRISB2 = 0; TRISBbits.TRISB3 = 0; }
#define SET_COL_INPUT()  { TRISBbits.TRISB4 = 1; TRISBbits.TRISB5 = 1; \
                           TRISBbits.TRISB6 = 1; TRISBbits.TRISB7 = 1; }

// Keypad Matrix Layout
const char keypad[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

// ============ Temperature Thresholds ============
#define TEMP_LOW_THRESHOLD   20  // °C
#define TEMP_MED_THRESHOLD   30  // °C
#define TEMP_HIGH_THRESHOLD  40  // °C

// ============ Global Variables ============
unsigned int adc_value = 0;
float temperature = 0.0;
char lcd_buffer[17];
unsigned int temp_setpoint = 25;  // Default setpoint
unsigned char mode = 0;           // 0=Display Mode, 1=Set Temp Mode

// ============ Function Prototypes ============
void initSystem(void);
void initADC(void);
void initI2C(void);
void initKeypad(void);
void initLED(void);

void I2C_Write(unsigned char data);
void I2C_WriteAddr(unsigned char addr, unsigned char rw);
void I2C_Start(void);
void I2C_Stop(void);
void I2C_Wait(void);
void I2C_Ack(void);

void LCD_Init(void);
void LCD_SendCommand(unsigned char cmd);
void LCD_SendData(unsigned char data);
void LCD_SetCursor(unsigned char row, unsigned char col);
void LCD_Print(const char *str);
void LCD_Clear(void);
void LCD_WriteCommand(unsigned char cmd);
void LCD_WriteByte(unsigned char data, unsigned char rs);

unsigned int ADC_Read(void);
float ReadTemperature(void);
void UpdateLED(float temp);
char ScanKeypad(void);
void ProcessKeypad(char key);
void DisplayTemperature(void);

void DelayMs(unsigned int ms);
void DelayUs(unsigned int us);

// ============ Main Program ============
void main(void) {
    initSystem();
    
    LCD_Init();
    LCD_Clear();
    LCD_SetCursor(0, 0);
    LCD_Print("Temp Monitor");
    LCD_SetCursor(1, 0);
    LCD_Print("System Ready");
    DelayMs(2000);
    
    while(1) {
        temperature = ReadTemperature();
        UpdateLED(temperature);
        
        DisplayTemperature();
        
        // Scan keypad
        char key = ScanKeypad();
        if(key != '\0') {
            ProcessKeypad(key);
        }
        
        DelayMs(300);
    }
}

// ============ System Initialization ============
void initSystem(void) {
    // Configure Port A: RA0 = ADC input
    TRISA = 0xFF;    // All inputs
    PORTA = 0x00;
    
    // Configure Port B: RB0-RB3 = Keypad rows (output), RB4-RB7 = Columns (input)
    TRISB = 0xF0;    // RB0-RB3 output, RB4-RB7 input
    PORTB = 0x00;
    
    // Configure Port C: RC3,RC4 = I2C (SDA, SCL)
    TRISC = 0x18;    // RC3, RC4 as inputs (I2C)
    PORTC = 0x00;
    
    // Configure Port D: RD0-RD2 = LED outputs
    TRISD = 0x00;    // All outputs
    PORTD = 0x00;
    
    // Configure Port E if needed
    TRISE = 0x00;
    PORTE = 0x00;
    
    initADC();
    initI2C();
    initLED();
    initKeypad();
}

// ============ ADC Initialization ============
void initADC(void) {
    // ADCON0: ADC Configuration Register 0
    ADCON0 = 0x00;  // ADC Off initially
    
    // ADCON1: ADC Configuration Register 1
    ADCON1 = 0x0E;  // RA0 = Analog input (AN0), all others digital
    
    // ADCON2: ADC Configuration Register 2
    ADCON2 = 0xBD;  // Right justified, Fosc/64, 12 TAD acquisition time
    
    // Select AN0 channel and enable ADC
    ADCON0bits.CHS = 0;      // Select AN0 channel
    ADCON0bits.ADON = 1;     // Enable ADC module
}

// ============ I2C Initialization ============
void initI2C(void) {
    // I2C Configuration for Master Mode at 100kHz
    // With 20MHz clock: SSPADD = (20MHz/(4*100kHz)) - 1 = 49
    SSPADD = 49;    // Baud rate for 100kHz I2C
    
    SSPCON1 = 0x28; // I2C Master mode
    SSPCON2 = 0x00; // No repeated START or PEN
    SSPSTAT = 0x00; // Disable slew rate control for master mode
    
    // Enable SSP interrupt if needed
    PIE1bits.SSPIE = 0;  // Disable SSP interrupt for now
}

// ============ LED Initialization ============
void initLED(void) {
    TRIS_LED_LOW = 0;   // Output
    TRIS_LED_MED = 0;   // Output
    TRIS_LED_HIGH = 0;  // Output
    
    LED_LOW = 0;   // Turn off
    LED_MED = 0;   // Turn off
    LED_HIGH = 0;  // Turn off
}

// ============ Keypad Initialization ============
void initKeypad(void) {
    SET_ROW_OUTPUT();
    SET_COL_INPUT();
    PORTB = 0x0F;  // Set all rows high initially
}

// ============ I2C Functions ============
void I2C_Wait(void) {
    while((SSPCON2 & 0x1F) || (SSPSTATbits.RW));
}

void I2C_Start(void) {
    SSPCON2bits.SEN = 1;  // Initiate START condition
    I2C_Wait();
}

void I2C_Stop(void) {
    SSPCON2bits.PEN = 1;  // Initiate STOP condition
    I2C_Wait();
}

void I2C_Ack(void) {
    SSPCON2bits.ACKDT = 0;  // Acknowledge
    SSPCON2bits.ACKEN = 1;
    I2C_Wait();
}

void I2C_WriteAddr(unsigned char addr, unsigned char rw) {
    SSPBUF = (addr << 1) | rw;
    I2C_Wait();
    
    // Check for ACK from slave
    if(SSPSTATbits.ACKSTAT) {
        // No ACK received
        I2C_Stop();
    }
}

void I2C_Write(unsigned char data) {
    SSPBUF = data;
    I2C_Wait();
    
    if(SSPSTATbits.ACKSTAT) {
        // No ACK from slave
        I2C_Stop();
    }
}

// ============ LCD Functions ============
void LCD_Init(void) {
    DelayMs(20);
    
    // Initialize LCD in 4-bit mode
    LCD_WriteCommand(0x33);  // Initialize
    DelayMs(5);
    LCD_WriteCommand(0x32);  // Set to 4-bit mode
    DelayMs(1);
    
    LCD_WriteCommand(0x28);  // 4-bit mode, 2 lines, 5x8 font
    DelayMs(1);
    LCD_WriteCommand(0x0C);  // Display ON, Cursor OFF, Blink OFF
    DelayMs(1);
    LCD_WriteCommand(0x01);  // Clear display
    DelayMs(2);
    LCD_WriteCommand(0x06);  // Entry mode: Increment, No shift
    DelayMs(1);
}

void LCD_WriteCommand(unsigned char cmd) {
    // RS = 0 (Command mode)
    LCD_WriteByte(cmd, 0);
}

void LCD_WriteByte(unsigned char data, unsigned char rs) {
    I2C_Start();
    I2C_WriteAddr(LCD_I2C_ADDR, 0);  // 0 = Write mode
    
    unsigned char control_byte;
    unsigned char high_nibble = (data & 0xF0);
    unsigned char low_nibble = ((data << 4) & 0xF0);
    
    // Send high nibble
    control_byte = high_nibble | LCD_BACKLIGHT | (rs ? 0x01 : 0x00);
    I2C_Write(control_byte);
    DelayUs(5);
    
    // Set Enable pin high
    control_byte |= 0x04;  // EN = 1
    I2C_Write(control_byte);
    DelayUs(5);
    
    // Set Enable pin low
    control_byte &= 0xFB;  // EN = 0
    I2C_Write(control_byte);
    DelayUs(100);
    
    // Send low nibble
    control_byte = low_nibble | LCD_BACKLIGHT | (rs ? 0x01 : 0x00);
    I2C_Write(control_byte);
    DelayUs(5);
    
    // Set Enable pin high
    control_byte |= 0x04;  // EN = 1
    I2C_Write(control_byte);
    DelayUs(5);
    
    // Set Enable pin low
    control_byte &= 0xFB;  // EN = 0
    I2C_Write(control_byte);
    DelayUs(100);
    
    I2C_Stop();
}

void LCD_SendCommand(unsigned char cmd) {
    LCD_WriteCommand(cmd);
    DelayMs(1);
}

void LCD_SendData(unsigned char data) {
    // RS = 1 (Data mode)
    LCD_WriteByte(data, 1);
    DelayUs(100);
}

void LCD_SetCursor(unsigned char row, unsigned char col) {
    unsigned char address;
    if(row == 0) {
        address = col;
    } else {
        address = 0x40 + col;
    }
    LCD_SendCommand(0x80 | address);
}

void LCD_Print(const char *str) {
    while(*str) {
        LCD_SendData(*str++);
    }
}

void LCD_Clear(void) {
    LCD_SendCommand(0x01);
    DelayMs(2);
}

// ============ ADC and Temperature Functions ============
unsigned int ADC_Read(void) {
    ADCON0bits.GO = 1;        // Start conversion
    while(ADCON0bits.GO);      // Wait for conversion to complete
    
    // Return 10-bit result
    return ((ADRESH << 8) | ADRESL);
}

float ReadTemperature(void) {
    adc_value = ADC_Read();
    
    // LM35: Output = 10mV per °C
    // At 5V reference and 10-bit ADC:
    // Temperature (°C) = (ADC_value * 5V / 1024) / 0.01V
    // Temperature (°C) = ADC_value * 0.4883
    
    temperature = (float)adc_value * 0.4883;
    
    return temperature;
}

// ============ LED Control ============
void UpdateLED(float temp) {
    // Turn off all LEDs first
    LED_LOW = 0;
    LED_MED = 0;
    LED_HIGH = 0;
    
    // Update LEDs based on temperature
    if(temp < TEMP_LOW_THRESHOLD) {
        LED_LOW = 1;   // Green LED - Low temperature
    }
    else if(temp < TEMP_MED_THRESHOLD) {
        LED_MED = 1;   // Yellow LED - Medium temperature
    }
    else {
        LED_HIGH = 1;  // Red LED - High temperature
    }
}

// ============ Display Temperature Function ============
void DisplayTemperature(void) {
    LCD_Clear();
    
    LCD_SetCursor(0, 0);
    LCD_Print("Temp:");
    
    // Display current temperature
    sprintf(lcd_buffer, "%.1f", temperature);
    LCD_SetCursor(0, 6);
    LCD_Print(lcd_buffer);
    LCD_SendData(0xDF);  // Degree symbol
    LCD_Print("C");
    
    LCD_SetCursor(1, 0);
    LCD_Print("Set:");
    
    // Display setpoint
    sprintf(lcd_buffer, "%d", temp_setpoint);
    LCD_SetCursor(1, 5);
    LCD_Print(lcd_buffer);
    LCD_SendData(0xDF);  // Degree symbol
    LCD_Print("C");
}

// ============ Keypad Functions ============
char ScanKeypad(void) {
    unsigned char row, col;
    
    for(row = 0; row < 4; row++) {
        // Set current row to low, others high
        unsigned char row_pattern = 0x0F & ~(1 << row);  // Only current row is 0
        PORTB = (PORTB & 0xF0) | row_pattern;
        
        DelayMs(5);  // Debounce
        
        // Read column pins (RB4-RB7)
        unsigned char col_port = PORTB >> 4;
        
        for(col = 0; col < 4; col++) {
            if(!(col_port & (1 << col))) {  // Column is LOW
                DelayMs(20);  // Debounce
                
                // Check again
                col_port = PORTB >> 4;
                if(!(col_port & (1 << col))) {
                    // Valid key press - wait for release
                    while(!(PORTB & (1 << (col + 4))));
                    DelayMs(20);  // Debounce key release
                    
                    PORTB = (PORTB & 0xF0) | 0x0F;  // Set all rows high
                    return keypad[row][col];
                }
            }
        }
    }
    
    PORTB = (PORTB & 0xF0) | 0x0F;  // Set all rows high
    return '\0';  // No key pressed
}

void ProcessKeypad(char key) {
    if(key >= '0' && key <= '9') {
        // Number key pressed
        if(mode == 1) {  // In set mode
            if(temp_setpoint == 0) {
                temp_setpoint = (key - '0');
            } else {
                temp_setpoint = (temp_setpoint * 10) + (key - '0');
            }
            
            // Limit to 99
            if(temp_setpoint > 99) {
                temp_setpoint = (key - '0');
            }
            
            // Update display
            LCD_Clear();
            LCD_SetCursor(0, 0);
            LCD_Print("Set Temp:");
            LCD_SetCursor(1, 0);
            sprintf(lcd_buffer, "%d", temp_setpoint);
            LCD_Print(lcd_buffer);
            DelayMs(500);
        }
    }
    else if(key == 'A') {
        // Enter set mode
        mode = 1;
        temp_setpoint = 0;
        LCD_Clear();
        LCD_SetCursor(0, 0);
        LCD_Print("Set Temp:");
        LCD_SetCursor(1, 0);
        LCD_Print("(0-99)");
        DelayMs(1000);
    }
    else if(key == 'B') {
        // Confirm and exit set mode
        mode = 0;
        LCD_Clear();
        LCD_SetCursor(0, 0);
        LCD_Print("Set to:");
        sprintf(lcd_buffer, "%d", temp_setpoint);
        LCD_SetCursor(1, 0);
        LCD_Print(lcd_buffer);
        DelayMs(2000);
    }
    else if(key == 'C') {
        // Clear/Reset
        temp_setpoint = 0;
        mode = 0;
        LCD_Clear();
        LCD_SetCursor(0, 0);
        LCD_Print("Cleared");
        DelayMs(1000);
    }
    else if(key == 'D') {
        // Reset to default
        temp_setpoint = 25;
        mode = 0;
        LCD_Clear();
        LCD_SetCursor(0, 0);
        LCD_Print("Reset to 25");
        DelayMs(1500);
    }
    else if(key == '*') {
        // Escape/Exit set mode
        mode = 0;
        LCD_Clear();
        LCD_SetCursor(0, 0);
        LCD_Print("Exited");
        DelayMs(1000);
    }
}

// ============ Delay Functions ============
void DelayMs(unsigned int ms) {
    unsigned int i, j;
    for(i = 0; i < ms; i++)
        for(j = 0; j < 123; j++);  // Approximately 1ms at 20MHz
}

void DelayUs(unsigned int us) {
    while(us--) {
        asm("nop");
        asm("nop");
    }
}
