/*
 * This sketch demonstrates how to use TFT LCD with ILI9314 api
 * 
 * Pre-requirement:
 *     an ST7735 TFT LCD with SPI interface
 * 
 * An ST7735 TFT LCD with SPI interface can be used with spi to
 * send command and data. We can draw text, line, circle, and
 * other picture on it.
 * 
 **/

#include "SPI.h"
#include "AmebaST7735.h"


// For all supported boards (AMB21/AMB22, AMB23, BW16/BW16-TypeC, AW-CU488_ThingPlus), 
// Select 2 GPIO pins connect to TFT_RESET and TFT_DC. And default SPI_SS/SPI1_SS connect to TFT_CS.
#define TFT_RESET       PA25
#define TFT_DC          PA26
#define TFT_CS          PA15

AmebaST7735 tft = AmebaST7735(TFT_CS, TFT_DC, TFT_RESET);

#define ST7735_SPI_FREQUENCY 20000000

void setup() {
    Serial.begin(115200);
    Serial.println("ST7735 Test!"); 

    tft.setResolution(128, 160); // for 1.8"
    // tft.setResolution(128, 128); // for 1.44"
    // tft.setResolution(80, 160); // for 0.96"

    tft.begin();
    SPI.setDefaultFrequency(ST7735_SPI_FREQUENCY);

    Serial.println("test filling screen");
    testFillScreen();
    delay(500);

    Serial.println("test Lines");
    testLines(ST7735_CYAN);
    delay(500);

    Serial.println("test Circles");
    testCircles(5, ST7735_BLUE);
    delay(500);

    Serial.println("test Circles");
    testRectangle(ST7735_LIGHTGREY);
    delay(500);

    Serial.println("test Text");
    testText();
    delay(500);
}

void loop() {
    for (int i = 0; i < 4; i++) {
        tft.setRotation(i);
        testText();
        delay(500);
    }
}

unsigned long testFillScreen() {
    tft.fillScreen(ST7735_BLACK);
    tft.fillScreen(ST7735_RED);
    tft.fillScreen(ST7735_GREEN);
    tft.fillScreen(ST7735_BLUE);
    tft.fillScreen(ST7735_BLACK);

    return 0;
}

unsigned long testText() {
    tft.clr();

    tft.setCursor(0, 0);

    tft.setForeground(ST7735_GREEN);
    tft.setFontSize(5);
    tft.println(tft.getWidth());

    tft.setForeground(ST7735_WHITE);
    tft.setFontSize(3);
    tft.println("Hello World!");

    tft.setForeground(ST7735_YELLOW);
    tft.setFontSize(2);
    tft.println(1234.56);

    tft.setForeground(ST7735_RED);
    tft.setFontSize(1);
    tft.println(0xDEADBEEF, HEX);

    tft.println();

    tft.setForeground(ST7735_LIGHTGREY);
    tft.setFontSize(2);
    tft.println("Alice in Wonderland");

    tft.setFontSize(1);
    tft.println("\"Where should I go?\" -Alice. \"That depends on where you want to end up.\" - The Cheshire Cat.");

    return 0;
}

unsigned long testLines(uint16_t color) {
    int x0, y0, x1, y1;
    int w = tft.getWidth();
    int h = tft.getHeight();

    tft.clr();

    x0 = 0;
    x1 = w;
    for (y0 = y1 = 0; y0 <= h; y0 += 5, y1 += 5) {
        tft.drawLine(x0, y0, x1, y1, color);
    }
    y0 = 0;
    y1 = h;
    for (x0 = x1 = 0; x0 <= w; x0 += 5, x1 += 5) {
        tft.drawLine(x0, y0, x1, y1, color);
    }

    tft.fillScreen(ST7735_BLACK);

    x0 = w / 2;
    y0 = h / 2;
    x1 = 0;
    for (y1 = 0; y1 < h; y1 += 5) {
        tft.drawLine(x0, y0, x1, y1, color);
    }

    y1 = h;
    for (x1 = 0; x1 < w; x1 += 5) {
        tft.drawLine(x0, y0, x1, y1, color);
    }

    x1 = w;
    for (y1=h; y1>=0; y1-=5) {
        tft.drawLine(x0, y0, x1, y1, color);
    }

    y1 = 0;
    for (x1 = w; x1 >= 0; x1 -= 5) {
        tft.drawLine(x0, y0, x1, y1, color);
    }

    return 0;
}

void testRectangle(uint16_t color) {
    int rw, rh;
    int div = 60;
    int w = tft.getWidth();
    int h = tft.getHeight();

    tft.clr();

    for (rw = w / div, rh = h / div; rw < w; rw += w / div, rh += h / div) {
        tft.drawRectangle((w / 2 - rw), (h / 2 - rh), (rw * 2), (rh * 2), color);
    }
}

void testCircles(uint8_t radius, uint16_t color) {
    int x, y;
    int w = tft.getWidth();
    int h = tft.getHeight();

    tft.clr();

    for(x = radius; x < w; x += radius * 2) {
        for(y = radius; y < h; y += radius * 2) {
            tft.drawCircle(x, y, radius, color);
        }
    }
}
