#include "lcd.h"
#include <libpic30.h>
void make_out_data(char buffer_data)
{
    if(buffer_data&1) LCD_D4 = 1;
    else LCD_D4 = 0;

    if(buffer_data&2) LCD_D5 = 1;
    else LCD_D5 = 0;

    if(buffer_data&4) LCD_D6 = 1;
    else LCD_D6 = 0;

    if(buffer_data&8) LCD_D7 = 1;
    else LCD_D7 = 0;
}

void lcd_send_nibble(char buffer_nibble)
{
    make_out_data(buffer_nibble);
    __delay_us(2);   
    LCD_EN = 1;    
    __delay_us(50);
    LCD_EN = 0;
}

void lcd_send_byte( char c, char n )
{
    LCD_RS = 0;
    __delay_us(50);
    if(c) LCD_RS = 1;
    else LCD_RS = 0;
    __delay_us(2);
    LCD_EN = 0;
    lcd_send_nibble(n >> 4);
    lcd_send_nibble(n & 0xf);
}

void lcd_init()
{
    LCD_RS_TRIS = 0;    LCD_RS = 0;
    LCD_RW_TRIS = 0;    LCD_RW = 0;
    LCD_EN_TRIS = 0;    LCD_EN = 0;
    LCD_D4_TRIS = 0;    LCD_D4 = 0;
    LCD_D5_TRIS = 0;    LCD_D5 = 0;
    LCD_D6_TRIS = 0;    LCD_D6 = 0;
    LCD_D7_TRIS = 0;    LCD_D7 = 0;

    lcd_send_nibble(3);
    __delay_ms(5);
    lcd_send_nibble(3);
    __delay_us(120);
    lcd_send_nibble(3);
    __delay_us(50);
    lcd_send_nibble(2);
    lcd_send_byte(0,0x28);
    lcd_send_byte(0,0x0c);
    lcd_send_byte(0,1);
    lcd_send_byte(0,6);
}

void lcd_gotoxy( char x, char y)
{
    char address;
    switch(y)
    {
        case 0 : address=0x80;break;
        case 1 : address=0xc0;break;
        case 2 : address=0x94;break;
        case 3 : address=0xd4;break;
    }
    address=address+x;
    lcd_send_byte(0,address);
}

void lcd_putc( char c)
{
    lcd_send_byte(1,c);
}

void lcd_command(int cmd)
{
    lcd_send_byte(0,cmd);
}

void lcd_clear(void)
{
    lcd_send_byte(0,1);
    __delay_ms(2);
}

void lcd_off_sreen(void)
{
    lcd_send_byte(0,8);
}

void lcd_on_sreen(void)
{
    lcd_send_byte(0,0x0c);
}

void lcd_shiftleft(void)
{
    lcd_send_byte(0,0x18);
}

void lcd_shiftright(void)
{
    lcd_send_byte(0,0x1C);
}

void lcd_moveright(char p,int ms)
{
    char i;
    for(i=0;i<p;i++)
    {
        lcd_shiftright();
        __delay_ms(ms);
    }
}

void lcd_moveleft(char p,int ms)
{
    char i;
    for(i=0;i<p;i++)
    {
        lcd_shiftleft();
       __delay_ms(ms);
    }
}

void lcd_putsf(const char *s)
{
    while(*s) lcd_putc(*s++);
}

void lcd_puts(char *s)
{
    while(*s) lcd_putc(*s++);
}


