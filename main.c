/**
 * PIC18F458 Temperature Sensor Project
 * Author: Temperature Monitoring System
 * Description: Complete temperature monitoring system with 4x4 keypad,
 *              I2C LCD display, and LED indicators (Low/Medium/High)
 * 
 * Hardware:
 * - Microcontroller: PIC18F458
 * - Temperature Sensor: LM35 (Analog Input RA0/AN0)
 * - LCD: 16x2 I2C module (PCF8574 backpack)
 * - Keypad: 4x4 Matrix (4 rows RB0-RB3, 4 columns RB4-RB7)
 * - LEDs: 3 (RD0=Green/Low, RD1=Yellow/Medium, RD2=Red/High)
 * - Crystal: 20MHz
 */

#include <xc.h>
#include <stdio.h>
#include <string.h>

// ============ Configuration Bits ============
#pragma config OSC = HS
#pragma config FCMEN = OFF
#pragma config IESO = OFF
#pragma config PWRT = ON
#pragma config BOREN = ON
#pragma config BORV = 2
#pragma config WDT = OFF
#pragma config WDTPS = 32768
#pragma config CCP2MX = ON
#pragma config STVREN = ON
#pragma config LVP = ON
#pragma config ICPRT = OFF
#pragma config DEBUG = OFF

#define _XTAL_FREQ 20000000UL

// ============ LCD I2C Configuration ============
#define LCD_ADDR 0x27
#define LCD_EN   0x04
#define LCD_RS   0x01
#define LCD_BL   0x08

// ============ LED Pins (Port D) ============
#define LED_LOW   LATDbits.LATD0
#define LED_MED   LATDbits.LATD1
#define LED_HIGH  LATDbits.LATD2

// ============ Temperature Thresholds ============
#define TEMP_LOW_THRESHOLD   20
#define TEMP_MED_THRESHOLD   30
#define TEMP_HIGH_THRESHOLD  40

// ============ Keypad Matrix ============
const char keypad[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

// ============ Global Variables ============
unsigned int adc_result = 0;
float temperature = 0.0;
char lcd_buffer[17] = {0};
unsigned int temp_setpoint = 25;
unsigned char system_mode = 0;

// ============ Function Prototypes ============
void init_system(void);
void init_adc(void);
void init_i2c(void);
void init_keypad(void);
void init_led(void);
void init_uart(void);

void i2c_start(void);
void i2c_stop(void);
void i2c_write_byte(unsigned char byte);
void i2c_wait(void);

void lcd_init(void);
void lcd_clear(void);
void lcd_command(unsigned char cmd);
void lcd_data(unsigned char data);
void lcd_set_cursor(unsigned char row, unsigned char col);
void lcd_write_string(const char *str);
void lcd_write_nibble(unsigned char nibble, unsigned char mode);
void lcd_pulse_enable(void);
void lcd_send_byte(unsigned char byte, unsigned char rs_bit);

float read_temperature(void);
void update_led(float temp);
char scan_keypad(void);
void process_key(char key);
void display_main(void);

void delay_ms(unsigned int ms);
void delay_us(unsigned int us);

// ============ Main Program ============
void main(void) {
    init_system();
    
    lcd_init();
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_write_string("Temp Monitor");
    lcd_set_cursor(1, 0);
    lcd_write_string("Ready!");
    delay_ms(2000);
    
    while(1) {
        temperature = read_temperature();
        update_led(temperature);
        display_main();
        
        char key = scan_keypad();
        if(key != '\0') {
            process_key(key);
        }
        
        delay_ms(200);
    }
}

// ============ System Initialization ============
void init_system(void) {
    // PORTA - ADC inputs
    TRISA = 0xFF;
    PORTA = 0x00;
    
    // PORTB - Keypad (RB0-RB3 output rows, RB4-RB7 input columns)
    TRISB = 0xF0;
    PORTB = 0x00;
    LATB = 0x0F;
    
    // PORTC - I2C (RC3=SDA, RC4=SCL) 
    TRISC = 0x18;
    PORTC = 0x00;
    
    // PORTD - LED outputs
    TRISD = 0x00;
    PORTD = 0x00;
    LATD = 0x00;
    
    // PORTE
    TRISE = 0x00;
    PORTE = 0x00;
    
    init_adc();
    init_i2c();
    init_led();
    init_keypad();
}

// ============ ADC Initialization ============
void init_adc(void) {
    // Configure analog input RA0/AN0
    ADCON1 = 0x0E;  // RA0 analog, rest digital
    ADCON0 = 0x00;  // ADC off initially
    ADCON2 = 0xBD;  // Right justified, Fosc/64, 12 TAD
    
    ADCON0bits.CHS = 0;    // Channel 0
    ADCON0bits.ADON = 1;   // Enable ADC
}

// ============ I2C Initialization ============
void init_i2c(void) {
    // Master mode, 100kHz for 20MHz: SSPADD = 49
    SSPADD = 49;
    SSPCON1 = 0x28;
    SSPCON2 = 0x00;
    SSPSTAT = 0x00;
}

// ============ LED Initialization ============
void init_led(void) {
    LED_LOW = 0;
    LED_MED = 0;
    LED_HIGH = 0;
}

// ============ Keypad Initialization ============
void init_keypad(void) {
    LATB = 0x0F;  // Set rows high
}

// ============ I2C Functions ============
void i2c_wait(void) {
    while((SSPCON2 & 0x1F) || (SSPSTATbits.RW));
}

void i2c_start(void) {
    SSPCON2bits.SEN = 1;
    i2c_wait();
}

void i2c_stop(void) {
    SSPCON2bits.PEN = 1;
    i2c_wait();
}

void i2c_write_byte(unsigned char byte) {
    SSPBUF = byte;
    i2c_wait();
}

// ============ LCD Functions ============
void lcd_init(void) {
    delay_ms(20);
    
    // 4-bit mode initialization sequence
    lcd_write_nibble(0x33, 0);
    delay_ms(5);
    lcd_write_nibble(0x32, 0);
    delay_ms(1);
    
    lcd_command(0x28);  // 4-bit, 2 lines, 5x8 font
    delay_ms(1);
    lcd_command(0x0C);  // Display ON, cursor OFF
    delay_ms(1);
    lcd_command(0x01);  // Clear display
    delay_ms(2);
    lcd_command(0x06);  // Entry mode
    delay_ms(1);
}

void lcd_send_byte(unsigned char byte, unsigned char rs_bit) {
    i2c_start();
    i2c_write_byte(LCD_ADDR << 1);
    
    unsigned char high_nibble = (byte & 0xF0);
    unsigned char low_nibble = ((byte << 4) & 0xF0);
    
    // Send high nibble
    unsigned char data = high_nibble | LCD_BL | (rs_bit ? LCD_RS : 0);
    i2c_write_byte(data);
    delay_us(2);
    
    i2c_write_byte(data | LCD_EN);
    delay_us(2);
    i2c_write_byte(data);
    delay_us(100);
    
    // Send low nibble
    data = low_nibble | LCD_BL | (rs_bit ? LCD_RS : 0);
    i2c_write_byte(data);
    delay_us(2);
    
    i2c_write_byte(data | LCD_EN);
    delay_us(2);
    i2c_write_byte(data);
    delay_us(100);
    
    i2c_stop();
}

void lcd_write_nibble(unsigned char nibble, unsigned char mode) {
    i2c_start();
    i2c_write_byte(LCD_ADDR << 1);
    
    unsigned char data = (nibble & 0xF0) | LCD_BL | (mode ? LCD_RS : 0);
    i2c_write_byte(data);
    delay_us(2);
    
    i2c_write_byte(data | LCD_EN);
    delay_us(2);
    i2c_write_byte(data);
    delay_us(100);
    
    i2c_stop();
}

void lcd_command(unsigned char cmd) {
    lcd_send_byte(cmd, 0);
    delay_ms(1);
}

void lcd_data(unsigned char data) {
    lcd_send_byte(data, 1);
    delay_us(100);
}

void lcd_clear(void) {
    lcd_command(0x01);
    delay_ms(2);
}

void lcd_set_cursor(unsigned char row, unsigned char col) {
    unsigned char address = (row == 0) ? col : (0x40 + col);
    lcd_command(0x80 | address);
}

void lcd_write_string(const char *str) {
    while(*str) {
        lcd_data(*str++);
    }
}

// ============ ADC and Temperature ============
float read_temperature(void) {
    // Start conversion
    ADCON0bits.GO = 1;
    
    // Wait for conversion
    while(ADCON0bits.GO);
    
    // Get result (10-bit right justified)
    adc_result = ((ADRESH << 8) | ADRESL);
    
    // LM35: 10mV per °C
    // Temperature = (ADC * 5V / 1024) / 0.01V = ADC * 0.4883
    temperature = (float)adc_result * 0.4883;
    
    return temperature;
}

void update_led(float temp) {
    LED_LOW = 0;
    LED_MED = 0;
    LED_HIGH = 0;
    
    if(temp < TEMP_LOW_THRESHOLD) {
        LED_LOW = 1;
    }
    else if(temp < TEMP_MED_THRESHOLD) {
        LED_MED = 1;
    }
    else {
        LED_HIGH = 1;
    }
}

// ============ Keypad Scanning ============
char scan_keypad(void) {
    unsigned char row, col;
    
    for(row = 0; row < 4; row++) {
        // Set current row to 0, others to 1
        LATB = (0x0F & ~(1 << row));
        delay_ms(2);
        
        // Read columns (RB4-RB7)
        unsigned char cols = (PORTB >> 4) & 0x0F;
        
        for(col = 0; col < 4; col++) {
            if(!(cols & (1 << col))) {  // Key pressed (column is LOW)
                delay_ms(20);  // Debounce
                
                // Verify key still pressed
                cols = (PORTB >> 4) & 0x0F;
                if(!(cols & (1 << col))) {
                    // Wait for key release
                    while(!((PORTB >> 4) & (1 << col))) {
                        delay_ms(5);
                    }
                    delay_ms(20);  // Debounce release
                    
                    LATB = 0x0F;  // Set all rows high
                    return keypad[row][col];
                }
            }
        }
    }
    
    LATB = 0x0F;  // Set all rows high
    return '\0';
}

void process_key(char key) {
    if(key >= '0' && key <= '9') {
        if(system_mode == 1) {
            if(temp_setpoint == 0) {
                temp_setpoint = key - '0';
            } else {
                temp_setpoint = (temp_setpoint * 10) + (key - '0');
            }
            
            if(temp_setpoint > 99) {
                temp_setpoint = key - '0';
            }
        }
    }
    else if(key == 'A') {
        system_mode = 1;
        temp_setpoint = 0;
        lcd_clear();
        lcd_set_cursor(0, 0);
        lcd_write_string("Enter Temp:");
    }
    else if(key == 'B') {
        system_mode = 0;
        lcd_clear();
        lcd_set_cursor(0, 0);
        lcd_write_string("Set to:");
        sprintf(lcd_buffer, "%d", temp_setpoint);
        lcd_set_cursor(1, 0);
        lcd_write_string(lcd_buffer);
        delay_ms(2000);
    }
    else if(key == 'C') {
        temp_setpoint = 0;
        system_mode = 0;
        lcd_clear();
        lcd_set_cursor(0, 0);
        lcd_write_string("Cleared");
        delay_ms(1000);
    }
    else if(key == 'D') {
        temp_setpoint = 25;
        system_mode = 0;
        lcd_clear();
        lcd_set_cursor(0, 0);
        lcd_write_string("Reset to 25C");
        delay_ms(1500);
    }
    else if(key == '*') {
        system_mode = 0;
    }
}

void display_main(void) {
    lcd_clear();
    
    lcd_set_cursor(0, 0);
    lcd_write_string("Temp:");
    sprintf(lcd_buffer, "%.1f", temperature);
    lcd_set_cursor(0, 6);
    lcd_write_string(lcd_buffer);
    lcd_data(0xDF);  // Degree symbol
    lcd_data('C');
    
    lcd_set_cursor(1, 0);
    lcd_write_string("Set:");
    sprintf(lcd_buffer, "%d", temp_setpoint);
    lcd_set_cursor(1, 5);
    lcd_write_string(lcd_buffer);
    lcd_data(0xDF);  // Degree symbol
    lcd_data('C');
}

// ============ Delay Functions ============
void delay_ms(unsigned int ms) {
    for(unsigned int i = 0; i < ms; i++) {
        for(unsigned int j = 0; j < 124; j++);
    }
}

void delay_us(unsigned int us) {
    for(unsigned int i = 0; i < us; i++) {
        asm("nop");
        asm("nop");
    }
}
