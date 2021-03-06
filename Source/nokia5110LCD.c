/*
* Nokia 5110 SPI LCD 84x48
*	Matt Casey Dec 2010
*	Based on the combined works of many others 
*	as well the handy datasheets.
*/

/****** I N C L U D E S **********************************************************/
#include <xc.h>
#include <plib.h>
#include "HardwareConfig.h"
#include "nokia5110lcd.h"
#include "SPI.h"

/****** V A R I A B L E S ********************************************************/

const char lcd_tinyBits[];
const char lcd_Font[];
const char lcd_Icons[];
volatile unsigned char g_lcdBuffer[LCD_ALLBYTES];

/*
const char lcd_BigNumbersCount;
const char lcd_BigNumbersChars[];
const char lcd_BigNumbersLen[];
const char lcd_BigNumbersData[];
*/

/*
*	Boot up code for the LCD
*/
void lcd_Init(void){
	#ifndef LCD_HARDWARERESET
	//GPIO driven reset : Why not save a pin and use a simple R/C circuit?
	LCD_RST = 0; 					// reset LCD
	delay_ms(100);      // Wait 100ms
	LCD_RST = 1;					// release from reset
	#endif
	
	lcd_writecommand(0x21);		// Activate Chip and H=1.
	lcd_writecommand(0b00010011);		// Adjust voltage bias 1:48.
	lcd_writecommand(0b11000010);		// Set LCD Voltage to about 7V.
	lcd_writecommand(0x20);		// Horizontal addressing and H=0.
	lcd_writecommand(0x09);		// Activate all segments.
	lcd_clearRam();			  		// Erase all pixel on the lcdram.
	lcd_writecommand(0x08);		// Blank the Display.
	lcd_writecommand(0x0C);		// Display Normal.
	lcd_cursorxy(0,0);		   	// Cursor Home.
}

/*
 * Renders complete lines from the global buffer.
 *
 */
void lcd_renderLines(unsigned char from, unsigned char to){
	unsigned short cursor;
	if( (from > 5) || (to > 5) || (from > to)){
		return;
	}
	//place LCD cursor at the first column of the first row we want to write
  lcd_setxy(0, from);
	LCD_DC = 1;
	//Write buffer
	LCD_CS_ENABLE
	for(cursor=(from * LCD_WIDTH); cursor<((to+1)*LCD_WIDTH); cursor++){
		SPI_8bitWrite(g_lcdBuffer[cursor]);	
	}
	LCD_CS_CLEAR
}

void lcd_write(unsigned char data){
	LCD_CS_ENABLE
	SPI_8bitWrite(data);
	LCD_CS_CLEAR
}

/*
 * Sends a graphic representation of a 16bit int
 * Uses a tiny font to cram all 16 bits in one row
 * Handy for displaying debug output
 * Can send MSBit first (dir=0) or LSBit first (dir=1)
 */
void lcd_sendBitGfx(unsigned short value, unsigned char dir){
	unsigned char idx, jdx, offset;
	unsigned short mask;
	if(dir == 0){
		//MSB first
		mask = 0b1000000000000000;
	}else{
		//LSB first
		mask = 0b0000000000000001;
	}
	LCD_DC = 1;
	LCD_CS_ENABLE
	for(idx=0; idx<16; idx++){
		if(value & mask){
			offset = 0;
		}else{
			offset = 4;
		}
		for(jdx=0; jdx<4; jdx++){
			SPI_8bitWrite(lcd_tinyBits[offset++]);
		}
		SPI_8bitWrite(0);
		if(dir == 0){
			value = value << 1;
		}else{
			value = value >> 1;
		}
	} 	
	LCD_CS_CLEAR
}

/*
	Sends a char as two hex characters.
	eg. 27 (0x1B) gets sent as a 1 followed by a B
*/
void lcd_sendHex(unsigned char hex){
	unsigned char temp;
	temp = ((hex & 0xF0)>>4);
	if (temp <= 0x09)
		lcd_sendChar(0x30 + temp);
	else
		lcd_sendChar(0x30 + temp + 0x07);

	temp = hex & 0x0F;
	if (temp <= 0x09)
		lcd_sendChar(0x30 + temp);
	else
		lcd_sendChar(0x30 + temp + 0x07);
}	


/* 
	Puts cursor to Line and Character specified
	! Assumes that we are using a 6*8 px font !
	Input Arguments: row    -> Line number range from 0 to 5         	
	Input Arguments: Column -> character position range from 0 to 13
	You can have maximum of 6 lines of text on LCD and each line
	containing 14 characters. Address is auto increment
*/
void lcd_cursorxy(unsigned char col,unsigned char row){
	if( (row > 5) || (col > 13))
  return;
  lcd_setxy(((col)*6), (row));
}

/* 
	Puts ram pointer to X,Y column, row position            
	Input Arguments: x-> X cordinate range from 0 to 83 
	Input Arguments: y-> Y cordinate range from 0 to 5  
*/ 
void lcd_setxy (unsigned char x, unsigned char y){
	lcd_writecommand( 0x40 | (y & 0x07) );
	lcd_writecommand( 0x80 | (x & 0x7f) );
}

/* 
	Puts ram pointer to Y row position            
	Input Arguments: y-> Y cordinate range from 0 to 5  
*/ 
void lcd_sety (unsigned char y){
	lcd_writecommand( 0x40 | (y & 0x07) );
}

void lcd_clearRam(void){
	unsigned short lcdram;
	lcd_cursorxy(0,0);
	for ( lcdram = LCD_ALLBYTES; lcdram > 0; lcdram--){
		// write all 504 LCDRAM addresses.
		lcd_writedata(0);
	}
}

/*Writes a full screen of junk*/
void lcd_fillscreen(void){
	unsigned short lcdram;
	lcd_cursorxy(0,0);
	for ( lcdram = LCD_ALLBYTES; lcdram > 0; lcdram--){
		lcd_writedata(TMR1 & 0xff);
	}	
}

/*
*	Clears a line and leave the cursor at the beginning of the line.
*/
void lcd_clearLine(unsigned char line){
    unsigned char i;
    lcd_cursorxy(0, line);
    for (i = 0; i < LCD_WIDTH; i++) {
        lcd_writedata(0);
    }
    lcd_cursorxy(0, line);
}

void lcd_sendChar(unsigned char x){
	int a,b;
	unsigned char c;
	if( (x<0x20) || (x>0x81) )
		return;
	a = ((5*x)-160);
	for( b=5; b>0; b--){
		c = lcd_Font[a];
		lcd_writedata(c);
		a++;
	}
	lcd_writedata(0x00);
}

/*
 * Sends a custom icon.
 * expects to find them in a const called lcd_Icons
 * Icons contain 8 cols
 */
void lcd_sendIcon(unsigned char num){
	int idx = num * 8;
	unsigned char col;
	for(col=8; col>0; col--){
		lcd_writedata(lcd_Icons[idx++]);
	}
	lcd_writedata(0x00);
}

void lcd_sendStr(const char *s){
	while(*s) lcd_sendChar(*s++);
}

void lcd_writecommand(unsigned char command){
	LCD_DC = 0;	// byte is a command it is read with the eight SCLK pulse
	lcd_write(command);
}

void lcd_writedata(unsigned char data){
	LCD_DC = 1;
	lcd_write(data);
}

/*
 * Optional double size numbers
 */
#ifdef LCD_USEBIGNUMBERS
	/*
		Sends double height characters.  Send top or bottom half.
		Limited to numerals and a couple of extra chars.
	*/
	void lcd_sendCharBig(unsigned char c, unsigned char offset){
		unsigned int idx = 0;
		unsigned char data, len = 0, ctr = 0;
		//Find the requested character, it's width and find the starting index for data.
		while(ctr < lcd_BigNumbersCount){
			len = lcd_BigNumbersLen[ctr];
			if(lcd_BigNumbersChars[ctr] == c) break;
			idx += (len*2);
			ctr++;
		}
		//Top/Bottom row data is interleaved.
		idx += offset;
		
		if(ctr == lcd_BigNumbersCount) return;
		while(len > 0){
			data = lcd_BigNumbersData[idx];
			lcd_writedata(data);
			idx += 2;
			len--;
		}
		lcd_writedata(0x00);
	}
	
	/*
		Send double height 8x16 characters
		Needs to know x,y position as it has to write 
		two rows of data.
	*/
	void lcd_sendStrBig(const char *s, unsigned char x, unsigned char y){
		unsigned int addr;
		addr = s;
		lcd_setxy(x,y);
		//send top half of characters
		while(*s) lcd_sendCharBig(*s++, 1);
		lcd_setxy(x,y+1);
		//Send bottom half
		s = addr;
		while(*s) lcd_sendCharBig(*s++, 0);
	}
	//Double height numbers (8x16)
	//Organised in columns of 16 pixels as high byte, low byte 
	const char lcd_BigNumbersCount = 12;
	const char lcd_BigNumbersChars[] = {0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x2e,0x2b};
	const char lcd_BigNumbersLen[] = {8,6,8,8,8,8,8,8,8,8,4,8};
	const char lcd_BigNumbersData[] = {
		0x1f,0xfc,0x3f,0xfe,0x70,0x07,0x60,0x03,0x60,0x03,0x70,0x07,0x3f,0xfe,0x1f,0xfc,	// 0
		0x60,0x06,0x60,0x07,0x7f,0xff,0x7f,0xff,0x60,0x00,0x60,0x00,						// 1
		0x7c,0x0c,0x7e,0x0e,0x67,0x07,0x63,0x83,0x61,0xc3,0x60,0xe7,0x60,0x7e,0x60,0x3c, 	// 2
		0x18,0x0c,0x38,0x0e,0x70,0x07,0x60,0x03,0x60,0xc3,0x71,0xe7,0x3f,0xfe,0x1f,0x7c, 	// 3
		0x07,0xc0,0x07,0xe0,0x06,0x70,0x06,0x38,0x06,0x1c,0x06,0x0e,0x7f,0x07,0x7f,0x03,	// 4
		0x18,0xff,0x38,0xff,0x70,0xc3,0x60,0xc3,0x60,0xc3,0x71,0xc3,0x3f,0x83,0x1f,0x03,	// 5
		0x1f,0xfc,0x3f,0xfe,0x71,0xc7,0x60,0xc3,0x60,0xc3,0x71,0xc7,0x3f,0x8e,0x1f,0x0c,	// 6
		0x00,0x03,0x00,0x03,0x00,0x03,0x00,0x03,0x00,0x63,0x00,0x63,0x7f,0xff,0x7f,0xff,	// 7
		0x1f,0x3c,0x3f,0xfe,0x71,0xe7,0x60,0xc3,0x60,0xc3,0x71,0xe7,0x3f,0xfe,0x1f,0x3c,	// 8
		0x18,0x3c,0x38,0x7e,0x70,0xe7,0x60,0xc3,0x60,0xc3,0x70,0xe7,0x3f,0xfe,0x1f,0xfc,	// 9
		0x30,0x00,0x78,0x00,0x78,0x00,0x30,0x00,											// .
		0x01,0x80,0x01,0x08,0x01,0x08,0x0f,0xf0,0x0f,0xf0,0x01,0x80,0x01,0x80,0x01,0x80		// +
	};
#endif

const char lcd_Icons[] = {
	0x7F, 0x79, 0x79, 0x41, 0x41, 0x79, 0x79, 0x7F, // Track T icon
	0x7F, 0x41, 0x41, 0x55, 0x51, 0x43, 0x47, 0x7F, // BPM B icon
  0x7F, 0x7F, 0x49, 0x41, 0x41, 0x49, 0x7F, 0x7F, // I for Instrument or something
	0x7F, 0x7F, 0x4F, 0x4F, 0x41, 0x79, 0x73, 0x7F,  // Note icon for Step
  0x7F, 0x41, 0x41, 0x75, 0x75, 0x71, 0x7B, 0x7F   // P icon for Pattern
};

const char lcd_tinyBits[] = {
	0x7f,0x7f,0x7f,0x7f, 	// A little one : Solid block
	0x63,0x41,0x41,0x63  	// A little zero : Empty block
	};

const char lcd_Font[] = { 
0x00,0x00,0x00,0x00,0x00, // 20 space ASCII Font for NOKIA LCD: 96 rows * 5 bytes= 480 bytes
0x00,0x00,0x5f,0x00,0x00, // 21 ! Note that this is the same set of codes for character you
0x00,0x07,0x00,0x07,0x00, // 22 " would find on a HD44780 based character LCD. :)
0x14,0x7f,0x14,0x7f,0x14, // 23 # Also, given the size of the LCD (84 pixels by 48 pixels),
0x24,0x2a,0x7f,0x2a,0x12, // 24 $ the maximum number of characters per row is only 14. :)
0x23,0x13,0x08,0x64,0x62, // 25 %
0x36,0x49,0x55,0x22,0x50, // 26 &
0x00,0x05,0x03,0x00,0x00, // 27 '
0x00,0x1c,0x22,0x41,0x00, // 28 (
0x00,0x41,0x22,0x1c,0x00, // 29 )
0x14,0x08,0x3e,0x08,0x14, // 2a *
0x08,0x08,0x3e,0x08,0x08, // 2b +
0x00,0x50,0x30,0x00,0x00, // 2c ,
0x08,0x08,0x08,0x08,0x08, // 2d -
0x00,0x60,0x60,0x00,0x00, // 2e .
0x20,0x10,0x08,0x04,0x02, // 2f /
0x3e,0x51,0x49,0x45,0x3e, // 30 0
0x00,0x42,0x7f,0x40,0x00, // 31 1
0x42,0x61,0x51,0x49,0x46, // 32 2
0x21,0x41,0x45,0x4b,0x31, // 33 3
0x18,0x14,0x12,0x7f,0x10, // 34 4
0x27,0x45,0x45,0x45,0x39, // 35 5
0x3c,0x4a,0x49,0x49,0x30, // 36 6
0x01,0x71,0x09,0x05,0x03, // 37 7
0x36,0x49,0x49,0x49,0x36, // 38 8
0x06,0x49,0x49,0x29,0x1e, // 39 9
0x00,0x36,0x36,0x00,0x00, // 3a :
0x00,0x56,0x36,0x00,0x00, // 3b ;
0x08,0x14,0x22,0x41,0x00, // 3c <
0x14,0x14,0x14,0x14,0x14, // 3d =
0x00,0x41,0x22,0x14,0x08, // 3e >
0x02,0x01,0x51,0x09,0x06, // 3f ?
0x32,0x49,0x79,0x41,0x3e, // 40 @
0x7e,0x11,0x11,0x11,0x7e, // 41 A
0x7f,0x49,0x49,0x49,0x36, // 42 B
0x3e,0x41,0x41,0x41,0x22, // 43 C
0x7f,0x41,0x41,0x22,0x1c, // 44 D
0x7f,0x49,0x49,0x49,0x41, // 45 E
0x7f,0x09,0x09,0x09,0x01, // 46 F
0x3e,0x41,0x49,0x49,0x7a, // 47 G
0x7f,0x08,0x08,0x08,0x7f, // 48 H
0x00,0x41,0x7f,0x41,0x00, // 49 I
0x20,0x40,0x41,0x3f,0x01, // 4a J
0x7f,0x08,0x14,0x22,0x41, // 4b K
0x7f,0x40,0x40,0x40,0x40, // 4c L
0x7f,0x02,0x0c,0x02,0x7f, // 4d M
0x7f,0x04,0x08,0x10,0x7f, // 4e N
0x3e,0x41,0x41,0x41,0x3e, // 4f O
0x7f,0x09,0x09,0x09,0x06, // 50 P
0x3e,0x41,0x51,0x21,0x5e, // 51 Q
0x7f,0x09,0x19,0x29,0x46, // 52 R
0x46,0x49,0x49,0x49,0x31, // 53 S
0x01,0x01,0x7f,0x01,0x01, // 54 T
0x3f,0x40,0x40,0x40,0x3f, // 55 U
0x1f,0x20,0x40,0x20,0x1f, // 56 V
0x3f,0x40,0x38,0x40,0x3f, // 57 W
0x63,0x14,0x08,0x14,0x63, // 58 X
0x07,0x08,0x70,0x08,0x07, // 59 Y
0x61,0x51,0x49,0x45,0x43, // 5a Z
0x00,0x7f,0x41,0x41,0x00, // 5b [
0b00000110,0b00001001,0b00001001,0b00000110,0x00, // 5c Degree symbol
//0x02,0x04,0x08,0x10,0x20, // 5c Yen symbol
0x00,0x41,0x41,0x7f,0x00, // 5d ]
0x04,0x02,0x01,0x02,0x04, // 5e ^
0x40,0x40,0x40,0x40,0x40, // 5f _
0x00,0x01,0x02,0x04,0x00, // 60 `
0x20,0x54,0x54,0x54,0x78, // 61 a
0x7f,0x48,0x44,0x44,0x38, // 62 b
0x38,0x44,0x44,0x44,0x20, // 63 c
0x38,0x44,0x44,0x48,0x7f, // 64 d
0x38,0x54,0x54,0x54,0x18, // 65 e
0x08,0x7e,0x09,0x01,0x02, // 66 f
0x0c,0x52,0x52,0x52,0x3e, // 67 g
0x7f,0x08,0x04,0x04,0x78, // 68 h
0x00,0x44,0x7d,0x40,0x00, // 69 i
0x20,0x40,0x44,0x3d,0x00, // 6a j
0x7f,0x10,0x28,0x44,0x00, // 6b k
0x00,0x41,0x7f,0x40,0x00, // 6c l
0x7c,0x04,0x18,0x04,0x78, // 6d m
0x7c,0x08,0x04,0x04,0x78, // 6e n
0x38,0x44,0x44,0x44,0x38, // 6f o
0x7c,0x14,0x14,0x14,0x08, // 70 p
0x08,0x14,0x14,0x18,0x7c, // 71 q
0x7c,0x08,0x04,0x04,0x08, // 72 r
0x48,0x54,0x54,0x54,0x20, // 73 s
0x04,0x3f,0x44,0x40,0x20, // 74 t
0x3c,0x40,0x40,0x20,0x7c, // 75 u
0x1c,0x20,0x40,0x20,0x1c, // 76 v
0x3c,0x40,0x30,0x40,0x3c, // 77 w
0x44,0x28,0x10,0x28,0x44, // 78 x
0x0c,0x50,0x50,0x50,0x3c, // 79 y
0x44,0x64,0x54,0x4c,0x44, // 7a z
0x00,0x08,0x36,0x41,0x00, // 7b <
0x00,0x00,0x7f,0x00,0x00, // 7c |
0x00,0x41,0x36,0x08,0x00, // 7d >
0x10,0x08,0x08,0x10,0x08, // 7e ~
0x78,0x46,0x41,0x46,0x78, // 7f Big down arrow
0b00000100,0b00000010,0b00011111,0b00000010,0b00000100, // 80 Up arrow
0b00010000,0b00100000,0b01111100,0b00100000,0b00010000 // 81 Down arrow
};

