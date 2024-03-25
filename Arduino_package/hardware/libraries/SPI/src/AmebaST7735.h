#ifndef _AMEBA_ST7735_H_
#define _AMEBA_ST7735_H_

#include "Print.h"
#include <cstdint>

#define ST7735_TFTWIDTH     128
#define ST7735_TFTHEIGHT    160

#define ST7735_NOP         0x00
#define ST7735_SWRESET     0x01
#define ST7735_RDDID       0x04
#define ST7735_RDDST       0x09

#define ST7735_SLPIN       0x10
#define ST7735_SLPOUT      0x11
#define ST7735_PTLON       0x12
#define ST7735_NORON       0x13

#define ST7735_RDMODE      0x0A
#define ST7735_RDMADCTL    0x0B
#define ST7735_RDPIXFMT    0x0C
#define ST7735_RDIMGFMT    0x0D
#define ST7735_RDSELFDIAG  0x0F

#define ST7735_INVOFF      0x20
#define ST7735_INVON       0x21
#define ST7735_GAMMASET    0x26
#define ST7735_DISPOFF     0x28
#define ST7735_DISPON      0x29

#define ST7735_CASET       0x2A
#define ST7735_PASET       0x2B
#define ST7735_RAMWR       0x2C
#define ST7735_RAMRD       0x2E

#define ST7735_PTLAR       0x30
#define ST7735_MADCTL      0x36
#define ST7735_PIXFMT      0x3A

#define ST7735_FRMCTR1     0xB1
#define ST7735_FRMCTR2     0xB2
#define ST7735_FRMCTR3     0xB3
#define ST7735_INVCTR      0xB4
#define ST7735_DFUNCTR     0xB6

#define ST7735_PWCTR1      0xC0
#define ST7735_PWCTR2      0xC1
#define ST7735_PWCTR3      0xC2
#define ST7735_PWCTR4      0xC3
#define ST7735_PWCTR5      0xC4
#define ST7735_VMCTR1      0xC5
#define ST7735_VMCTR2      0xC7

#define ST7735_RDID1       0xDA
#define ST7735_RDID2       0xDB
#define ST7735_RDID3       0xDC
#define ST7735_RDID4       0xDD

#define ST7735_GMCTRP1     0xE0
#define ST7735_GMCTRN1     0xE1

#define ST7735_MADCTL_MY   0x80
#define ST7735_MADCTL_MX   0x40
#define ST7735_MADCTL_MV   0x20
#define ST7735_MADCTL_ML   0x10
#define ST7735_MADCTL_RGB  0x00
#define ST7735_MADCTL_BGR  0x08
#define ST7735_MADCTL_MH   0x04

// Color definitions (RGB565)
#define ST7735_BLACK       0x0000      /*   0,   0,   0 */
#define ST7735_NAVY        0x000F      /*   0,   0, 128 */
#define ST7735_DARKGREEN   0x03E0      /*   0, 128,   0 */
#define ST7735_DARKCYAN    0x03EF      /*   0, 128, 128 */
#define ST7735_MAROON      0x7800      /* 128,   0,   0 */
#define ST7735_PURPLE      0x780F      /* 128,   0, 128 */
#define ST7735_OLIVE       0x7BE0      /* 128, 128,   0 */
#define ST7735_LIGHTGREY   0xC618      /* 192, 192, 192 */
#define ST7735_DARKGREY    0x7BEF      /* 128, 128, 128 */
#define ST7735_BLUE        0x001F      /*   0,   0, 255 */
#define ST7735_GREEN       0x07E0      /*   0, 255,   0 */
#define ST7735_CYAN        0x07FF      /*   0, 255, 255 */
#define ST7735_RED         0xF800      /* 255,   0,   0 */
#define ST7735_MAGENTA     0xF81F      /* 255,   0, 255 */
#define ST7735_YELLOW      0xFFE0      /* 255, 255,   0 */
#define ST7735_WHITE       0xFFFF      /* 255, 255, 255 */
#define ST7735_ORANGE      0xFD20      /* 255, 165,   0 */
#define ST7735_GREENYELLOW 0xAFE5      /* 173, 255,  47 */
#define ST7735_PINK        0xF81F

class AmebaST7735 : public Print {

public:
    AmebaST7735(int csPin, int dcPin, int resetPin);

    void begin(void);

    void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

    void writecommand(uint8_t command);
    void writedata(uint8_t data);
    void writedata(uint8_t *data, size_t datasize);

    void setRotation(uint8_t m);
    void setResolution(int16_t w, int16_t h);

    void fillScreen(uint16_t color);
    void clr();

    void fillRectangle(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

    void drawPixel(int16_t x, int16_t y, uint16_t color);

    void drawChar(int16_t x, int16_t y, unsigned char c, uint16_t fontcolor, uint16_t background, uint8_t fontsize);
    void drawChar(unsigned char c);

    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1);

    void drawRectangle(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void drawRectangle(int16_t x, int16_t y, int16_t w, int16_t h);

    void drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
    void drawCircle(int16_t x0, int16_t y0, int16_t r);

    virtual size_t write(uint8_t);

    int16_t getWidth();
    int16_t getHeight();

    void setCursor(int16_t x, int16_t y);
    void setForeground(uint16_t color);
    void setBackground(uint16_t color);
    void setFontSize(uint8_t size);

private:
    void reset(void);

    int _csPin;
    int _dcPin;
    int _resetPin;

    uint32_t _dcPort;
    uint32_t _dcMask;

    int16_t _width;
    int16_t _height;
    int16_t _newWidth;
    int16_t _newHeight;

    int16_t cursor_x;
    int16_t cursor_y;
    uint16_t foreground;
    uint16_t background;
    uint8_t fontsize;
    uint8_t rotation;
};

#endif
