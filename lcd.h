#ifndef LCD_H
#define	LCD_H
#include "Mainmio.h"
//#ifdef	__cplusplus
//extern "C" {
//#endif

#ifndef FCY
#define _XTAL_FREQ                7372800 // xtal = 7.3728 Mhz
#define PLL_MODE                      16  // PLL x16
#define FCY         29491200UL//(_XTAL_FREQ*PLL_MODE/4)  30 MIPS
#endif


#define  LCD_RS         _LATC3
#define  LCD_RW         _LATC1
#define  LCD_EN         _LATD13
#define  LCD_D4         _LATD0
#define  LCD_D5         _LATD1
#define  LCD_D6         _LATD2
#define  LCD_D7         _LATD3
/*
#define  LCD_RS         _RC3
#define  LCD_RW         _RC1
#define  LCD_EN         _RD13
#define  LCD_D4         _RD0
#define  LCD_D5         _RD1
#define  LCD_D6         _RD2
#define  LCD_D7         _RD3
*/

#define  LCD_RS_TRIS    _TRISC3
#define  LCD_RW_TRIS    _TRISC1
#define  LCD_EN_TRIS    _TRISD13
#define  LCD_D4_TRIS    _TRISD0
#define  LCD_D5_TRIS    _TRISD1
#define  LCD_D6_TRIS    _TRISD2
#define  LCD_D7_TRIS    _TRISD3

void DelayuS(unsigned long int us);
void DelaymS(unsigned long int ms);
void make_out_data(char buffer_data);
void lcd_send_nibble(char buffer_nibble);
void lcd_send_byte( char c, char n );
void lcd_init();
void lcd_gotoxy( char x, char y);
void lcd_putc( char c);
void lcd_command(int cmd);
void lcd_clear(void);
void lcd_off_sreen(void);
void lcd_on_sreen(void);
void lcd_shiftleft(void);
void lcd_shiftright(void);
void lcd_moveright(char p,int ms);
void lcd_moveleft(char p,int ms);
void lcd_putsf(const char *s);
void lcd_puts(char *s);


//#ifdef	__cplusplus
//}
//#endif

#endif	/* LCD_H */

