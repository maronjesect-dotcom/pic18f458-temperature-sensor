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

// Configuration bits
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
// Rows: RB0-RB3
// Columns: RB4-RB7
#define ROW_PORT PORTB
#define ROW_TRIS TRISB
#define COL_PORT PORTB
#define COL_TRIS TRISB

#define SET_ROW_OUTPUT() { ROW_TRIS = 0x00; }
#define SET_COL_INPUT()  { COL_TRIS = 0xF0; }

// Keypad Matrix
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
void initTimer(void);

void I2C_Write(unsigned char data);
void I2C_WriteAddr(unsigned char addr, unsigned char rw);
void I2C_Start(void);
void I2C_Stop(void);
void I2C_Wait(void);

void LCD_Init(void);
void LCD_SendCommand(unsigned char cmd);
void LCD_SendData(unsigned char data);
void LCD_SetCursor(unsigned char row, unsigned char col);
void LCD_Print(char *str);
void LCD_Clear(void);
void LCD_WriteCommand(unsigned char cmd);

unsigned int ADC_Read(void);
float ReadTemperature(void);
void UpdateLED(float temp);
char ScanKeypad(void);
void ProcessKeypad(char key);

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
        
        LCD_Clear();
        LCD_SetCursor(0, 0);
        LCD_Print("Temp:");
        
        // Display temperature
        sprintf(lcd_buffer, "%.1fC", temperature);
        LCD_SetCursor(0, 6);
        LCD_Print(lcd_buffer);
        
        LCD_SetCursor(1, 0);
        LCD_Print("Set:");
        sprintf(lcd_buffer, "%d", temp_setpoint);
        LCD_SetCursor(1, 5);
        LCD_Print(lcd_buffer);
        
        // Scan keypad
        char key = ScanKeypad();
        if(key != '\0') {
            ProcessKeypad(key);
        }
        
        DelayMs(500);
    }
}

// ============ System Initialization ============
void initSystem(void) {
    // Set all ports as outputs initially
    TRISA = 0x01;  // RA0 = ADC input (Temperature sensor)
    TRISB = 0xF0;  // RB0-RB3 = Keypad rows (output), RB4-RB7 = Columns (input)
    TRISC = 0x18;  // RC3,RC4 = I2C (SDA, SCL)
    TRISD = 0x00;  // RD0-RD2 = LED outputs
    
    initADC();
    initI2C();
    initLED();
    initKeypad();
    initTimer();
}

// ============ ADC Initialization ============
void initADC(void) {
    ADCON0 = 0x00;  // ADC Off initially
    ADCON1 = 0x0E;  // RA0 = Analog input, others digital
    ADCON2 = 0xBD;  // Right justified, Fosc/64, 12 TAD
    
    ADCON0bits.CHS = 0;      // Select AN0 channel
    ADCON0bits.ADON = 1;     // Enable ADC
}

// ============ I2C Initialization ============
void initI2C(void) {
    // I2C configuration
    SSPADD = 99;    // Baud rate for 20MHz and 100kHz I2C
    SSPCON1 = 0x28; // I2C Master mode
    SSPCON2 = 0x00;
    SSPSTAT = 0x00;
}

// ============ LED Initialization ============
void initLED(void) {
    TRIS_LED_LOW = 0;
    TRIS_LED_MED = 0;
    TRIS_LED_HIGH = 0;
    
    LED_LOW = 0;
    LED_MED = 0;
    LED_HIGH = 0;
}

// ============ Keypad Initialization ============
void initKeypad(void) {
    SET_ROW_OUTPUT();
    SET_COL_INPUT();
    ROW_PORT = 0x0F;  // Set all rows high
}

// ============ Timer Initialization ============
void initTimer(void) {
    T0CON = 0x87;   // Timer0, 8-bit, internal clock, 1:256 prescaler
    TMR0IE = 1;     // Enable Timer0 interrupt
    INTCON = 0xA0;  // Enable global and peripheral interrupts
}

// ============ I2C Functions ============
void I2C_Wait(void) {
    while((SSPCON2 & 0x1F) | (SSPSTATbits.RW));
}

void I2C_Start(void) {
    SSPCON2bits.SEN = 1;  // Initiate START condition
    I2C_Wait();
}

void I2C_Stop(void) {
    SSPCON2bits.PEN = 1;  // Initiate STOP condition
    I2C_Wait();
}

void I2C_WriteAddr(unsigned char addr, unsigned char rw) {
    SSPBUF = (addr << 1) | rw;
    I2C_Wait();
}

void I2C_Write(unsigned char data) {
    SSPBUF = data;
    I2C_Wait();
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
    LCD_WriteCommand(0x0C);  // Display ON, Cursor OFF
    LCD_WriteCommand(0x01);  // Clear display
    DelayMs(2);
    LCD_WriteCommand(0x06);  // Entry mode
}

void LCD_WriteCommand(unsigned char cmd) {
    I2C_Start();
    I2C_WriteAddr(LCD_I2C_ADDR, 0);
    
    // Send high nibble
    I2C_Write((cmd & 0xF0) | LCD_BACKLIGHT);
    I2C_Write((cmd & 0xF0) | 0x04 | LCD_BACKLIGHT);  // EN=1
    DelayUs(1);
    I2C_Write((cmd & 0xF0) | LCD_BACKLIGHT);        // EN=0
    DelayUs(100);
    
    // Send low nibble
    I2C_Write(((cmd << 4) & 0xF0) | LCD_BACKLIGHT);
    I2C_Write(((cmd << 4) & 0xF0) | 0x04 | LCD_BACKLIGHT);  // EN=1
    DelayUs(1);
    I2C_Write(((cmd << 4) & 0xF0) | LCD_BACKLIGHT);        // EN=0
    DelayUs(100);
    
    I2C_Stop();
}

void LCD_SendCommand(unsigned char cmd) {
    LCD_WriteCommand(cmd);
}

void LCD_SendData(unsigned char data) {
    I2C_Start();
    I2C_WriteAddr(LCD_I2C_ADDR, 0);
    
    // Send high nibble with RS=1 (data)
    I2C_Write((data & 0xF0) | 0x01 | LCD_BACKLIGHT);
    I2C_Write((data & 0xF0) | 0x05 | LCD_BACKLIGHT);  // EN=1, RS=1
    DelayUs(1);
    I2C_Write((data & 0xF0) | 0x01 | LCD_BACKLIGHT);  // EN=0
    DelayUs(100);
    
    // Send low nibble with RS=1 (data)
    I2C_Write(((data << 4) & 0xF0) | 0x01 | LCD_BACKLIGHT);
    I2C_Write(((data << 4) & 0xF0) | 0x05 | LCD_BACKLIGHT);  // EN=1, RS=1
    DelayUs(1);
    I2C_Write(((data << 4) & 0xF0) | 0x01 | LCD_BACKLIGHT);  // EN=0
    DelayUs(100);
    
    I2C_Stop();
}

void LCD_SetCursor(unsigned char row, unsigned char col) {
    unsigned char address = (row == 0) ? col : (0x40 + col);
    LCD_SendCommand(0x80 | address);
    DelayMs(1);
}

void LCD_Print(char *str) {
    while(*str) {
        LCD_SendData(*str++);
        DelayUs(100);
    }
}

void LCD_Clear(void) {
    LCD_SendCommand(0x01);
    DelayMs(2);
}

// ============ ADC and Temperature Functions ============
unsigned int ADC_Read(void) {
    ADCON0bits.GO = 1;        // Start conversion
    while(ADCON0bits.GO);      // Wait for conversion
    return ((ADRESH << 8) | ADRESL);
}

float ReadTemperature(void) {
    adc_value = ADC_Read();
    
    // LM35: Output = 10mV per °C
    // At 5V reference: Temperature = (ADC_value * 5 / 1024) / 0.01
    // Simplified: Temperature = ADC_value * 0.4883
    temperature = adc_value * 0.4883;
    
    return temperature;
}

// ============ LED Control ============
void UpdateLED(float temp) {
    LED_LOW = 0;
    LED_MED = 0;
    LED_HIGH = 0;
    
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

// ============ Keypad Functions ============
char ScanKeypad(void) {
    unsigned char row, col;
    
    for(row = 0; row < 4; row++) {
        // Set current row to low, others high
        ROW_PORT = ~(1 << row) & 0x0F;
        DelayMs(5);
        
        // Read columns
        for(col = 0; col < 4; col++) {
            if(!(COL_PORT & (1 << (col + 4)))) {
                DelayMs(20);  // Debounce
                if(!(COL_PORT & (1 << (col + 4)))) {
                    // Wait for key release
                    while(!(COL_PORT & (1 << (col + 4))));
                    DelayMs(20);
                    return keypad[row][col];
                }
            }
        }
    }
    
    ROW_PORT = 0x0F;  // Set all rows high
    return '\0';
}

void ProcessKeypad(char key) {
    if(key >= '0' && key <= '9') {
        // Number key pressed
        if(mode == 1) {  // In set mode
            temp_setpoint = (temp_setpoint * 10) + (key - '0');
            if(temp_setpoint > 99) {
                temp_setpoint = key - '0';
            }
        }
    }
    else if(key == 'A') {
        // Enter set mode
        mode = 1;
        temp_setpoint = 0;
        LCD_Clear();
        LCD_SetCursor(0, 0);
        LCD_Print("Set Temp:");
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
        // Clear
        temp_setpoint = 0;
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
}

// ============ Delay Functions ============
void DelayMs(unsigned int ms) {
    for(unsigned int i = 0; i < ms; i++)
        DelayUs(1000);
}

void DelayUs(unsigned int us) {
    while(us--) {
        __delay_us(1);
    }
}
