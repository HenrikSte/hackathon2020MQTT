#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <GxEPD.h>


#if !defined ESP32DRIVERBOARD
  #if defined BWR_DISPLAY
    #include <GxGDEW042Z15/GxGDEW042Z15.h>    // 4.2" b/w/r
  #else
    #include <GxGDEW042T2/GxGDEW042T2.h>      // 4.2" b/w
  #endif
#else
  #if defined BWR_DISPLAY
    #define HAS_RED_COLOR
    #error BWR in 7.5inch not supported yet
  #else
    #include <GxGDEW075T7/GxGDEW075T7.h>      // 7.5" b/w 800x480
  #endif
#endif

// FreeFonts from Adafruit_GFX
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>


#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>

#if defined(ESP32)

#if !defined ESP32DRIVERBOARD
  #define PIN_BUSY  4   // Purple
  #define PIN_RST   16  // White
  #define PIN_DC    17  // Green
  #define PIN_CS    5   // Orange
  #define PIN_CLK   18  // Yellow
  #define PIN_DIN   23  // Blue
#else
  #define PIN_BUSY  25 
  #define PIN_RST   26
  #define PIN_DC    27
  #define PIN_CS    15 
  #define PIN_CLK   13
  #define PIN_DIN   12
  #define PIN_MISO  12
  #define PIN_MOSI  14
  #define PIN_SS    15 
#endif


#define font9  &FreeMonoBold9pt7b
#define font12 &FreeMonoBold12pt7b
#define font18 &FreeMonoBold18pt7b
#define font24 &FreeMonoBold24pt7b


//SPIClass displaySPI;  

class ePaper
{
  public:
    ePaper();

    GxIO_Class io;
    GxEPD_Class display;

    void showText( const GFXfont* f, const char* text);



    void showFont(const char name[], const GFXfont* f);
    //void showFontCallback();
    //void drawCornerTest();
    //void showBitmapExample();
};
#endif 
