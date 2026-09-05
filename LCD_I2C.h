/**
 * LCD_I2C.h
 * I2C LCD Display Driver Header File
 * Compatible with PIC18F458 and similar microcontrollers
 * 
 * This header provides functions to control a 16x2 I2C LCD module
 * commonly used with PCF8574 I2C expander
 */

#ifndef LCD_I2C_H
#define LCD_I2C_H

#include <xc.h>

// ============ LCD Configuration ============
#define LCD_I2C_ADDRESS 0x27      // Default I2C address (can be 0x27 or 0x3F)
#define LCD_ADDR_WRITE  0x4E      // LCD write address (0x27 << 1)
#define LCD_ADDR_READ   0x4F      // LCD read address

#define LCD_WIDTH 16
#define LCD_HEIGHT 2

// ============ LCD Control Bits (for PCF8574) ============
#define LCD_RS    0x01            // Register Select
#define LCD_RW    0x02            // Read/Write
#define LCD_EN    0x04            // Enable
#define LCD_BL    0x08            // Backlight

// ============ LCD Commands ============
#define LCD_CMD_CLEAR             0x01
#define LCD_CMD_HOME              0x02
#define LCD_CMD_ENTRY_MODE        0x04
#define LCD_CMD_DISPLAY_CTRL      0x08
#define LCD_CMD_SHIFT             0x10
#define LCD_CMD_FUNCTION_SET      0x20
#define LCD_CMD_SET_CGRAM_ADDR    0x40
#define LCD_CMD_SET_DDRAM_ADDR    0x80

// ============ LCD Entry Mode Flags ============
#define LCD_ENTRY_SH              0x01
#define LCD_ENTRY_ID              0x02

// ============ LCD Display Control Flags ============
#define LCD_DISP_BLINK            0x01
#define LCD_DISP_CURSOR           0x02
#define LCD_DISP_ON               0x04

// ============ LCD Function Set Flags ============
#define LCD_FUNC_5x10DOTS         0x04
#define LCD_FUNC_2LINE            0x08
#define LCD_FUNC_8BIT             0x10

// ============ LCD Row Addresses ============
#define LCD_ROW0_ADDR             0x00
#define LCD_ROW1_ADDR             0x40

// ============ Function Declarations ============

/**
 * Initialize I2C LCD module
 * Configures LCD to 4-bit mode, 2 lines, 5x8 font
 */
void LCD_Init(void);

/**
 * Clear LCD display and reset cursor to home position
 */
void LCD_Clear(void);

/**
 * Return cursor to home position (0,0)
 */
void LCD_Home(void);

/**
 * Set cursor to specific position
 * @param row: Row number (0 or 1)
 * @param col: Column number (0-15)
 */
void LCD_SetCursor(unsigned char row, unsigned char col);

/**
 * Write a single character to LCD at current cursor position
 * @param data: Character to display
 */
void LCD_WriteChar(unsigned char data);

/**
 * Write a string to LCD starting at current cursor position
 * @param str: Pointer to null-terminated string
 */
void LCD_WriteString(const char *str);

/**
 * Send command to LCD
 * @param cmd: Command byte
 */
void LCD_SendCommand(unsigned char cmd);

/**
 * Display ON/OFF control
 * @param state: 1 to turn ON, 0 to turn OFF
 */
void LCD_DisplayOn(unsigned char state);

/**
 * Cursor visible ON/OFF
 * @param state: 1 to show cursor, 0 to hide cursor
 */
void LCD_CursorOn(unsigned char state);

/**
 * Cursor blink ON/OFF
 * @param state: 1 to enable blink, 0 to disable blink
 */
void LCD_BlinkOn(unsigned char state);

/**
 * Backlight control
 * @param state: 1 to turn ON, 0 to turn OFF
 */
void LCD_BacklightOn(unsigned char state);

/**
 * Shift display left
 */
void LCD_ShiftLeft(void);

/**
 * Shift display right
 */
void LCD_ShiftRight(void);

/**
 * Print formatted string (like printf)
 * @param fmt: Format string
 * @param ...: Variable arguments
 */
void LCD_Printf(const char *fmt, ...);

/**
 * Create custom character at position (0-7)
 * @param pos: Character position in CGRAM (0-7)
 * @param pattern: 8-byte array defining the character pattern
 */
void LCD_CreateChar(unsigned char pos, const unsigned char *pattern);

/**
 * Write custom character to LCD
 * @param pos: Character position (0-7)
 */
void LCD_WriteCustomChar(unsigned char pos);

// ============ Low-Level I2C Functions ============

/**
 * Low-level I2C write with pulse enable
 * @param data: Byte to write
 * @param mode: 0 for command, 1 for data
 */
void LCD_WriteByte(unsigned char data, unsigned char mode);

/**
 * Write nibble to LCD in 4-bit mode
 * @param nibble: 4-bit value
 * @param mode: 0 for command, 1 for data
 */
void LCD_WriteNibble(unsigned char nibble, unsigned char mode);

/**
 * Pulse enable signal to latch data
 */
void LCD_PulseEnable(void);

/**
 * Send data via I2C to LCD
 * @param data: Byte to send
 */
void LCD_I2C_Write(unsigned char data);

/**
 * Initialize I2C communication
 */
void LCD_I2C_Init(void);

#endif // LCD_I2C_H
