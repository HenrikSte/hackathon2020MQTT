#include "epaper.h"

#include GxEPD_BitmapExamples

ePaper::ePaper() :
  io(SPI, /*CS=5*/ PIN_CS, /*DC=*/ PIN_DC, /*RST=*/ PIN_RST),
  display(io, /*RST=*/ PIN_RST, /*BUSY=*/ PIN_BUSY)
{
  Serial.println(display.bm_default);
  Serial.println(io.name);
}


void ePaper::showFont(const char name[], const GFXfont* f)
{

  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display.setFont(f);
  display.setCursor(0, 0);
  display.println();
  display.println(name);
  display.println(" !\"#$%&'()*+,-./");
  display.println("0123456789:;<=>?");
  display.println("@ABCDEFGHIJKLMNO");
  display.println("PQRSTUVWXYZ[\\]^_");
#if defined(HAS_RED_COLOR)
  display.setTextColor(GxEPD_RED);
#endif
  display.println("`abcdefghijklmno");
  display.println("pqrstuvwxyz{|}~ ");
  display.update();
  delay(5000);
}

/*
void ePaper::showFontCallback()
{
  const char* name = "FreeMonoBold9pt7b";
  const GFXfont* f = &FreeMonoBold9pt7b;
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display.setFont(f);
  display.setCursor(0, 0);
  display.println();
  display.println(name);
  display.println(" !\"#$%&'()*+,-./");
  display.println("0123456789:;<=>?");
  display.println("@ABCDEFGHIJKLMNO");
  display.println("PQRSTUVWXYZ[\\]^_");
#if defined(HAS_RED_COLOR)
  display.setTextColor(GxEPD_RED);
#endif
  display.println("`abcdefghijklmno");
  display.println("pqrstuvwxyz{|}~ ");
}

*/

void ePaper::rederLabel( const String & data,  const String & layout)
{

}


void ePaper::showText( const GFXfont* f,
              const char* text)
{
  display.fillScreen(GxEPD_WHITE);
  #if defined(HAS_RED_COLOR)
    display.setTextColor(GxEPD_RED);
  #else
    display.setTextColor(GxEPD_BLACK);
  #endif
  display.setFont(f);
  display.setCursor(0, 0);
  display.println();
  display.println(text);
  display.update(); 
}

void ePaper::printHLine(uint16_t y, uint16_t width, uint16_t color) {
  display.fillRect(0,y,GxEPD_WIDTH,width,color);
}

uint16_t ePaper::printCenteredText(uint16_t y, const GFXfont* f, uint16_t color, const char* text) {
  display.setFont(f);
  int16_t x1,y1;
  uint16_t width,height;
  display.getTextBounds(text,0,y,&x1,&y1,&width,&height);
  //display.fillRect(x1,y1+height,width,height,GxEPD_BLACK);
  //color=GxEPD_WHITE;

  uint16_t offset = (GxEPD_WIDTH-width)/2;

  display.setCursor(offset,y+height);
  display.setTextColor(color);
  display.println(text);
  
  //return hight of line
  return height;
}

uint16_t ePaper::printLeftAlignedText(uint16_t y, const GFXfont* f, uint16_t color, const char* text) {
  display.setFont(f);
  int16_t x1,y1;
  uint16_t width,height;
  display.getTextBounds(text,0,y,&x1,&y1,&width,&height);
  //display.fillRect(x1,y1+height,width,height,GxEPD_BLACK);
  //color=GxEPD_WHITE;

  display.setCursor(0,y+height);
  display.setTextColor(color);
  display.println(text);

  //return hight of line
  return height;
}


void ePaper::printLabel()
{
  display.fillScreen(GxEPD_WHITE);
  // id
  printCenteredText(5,font18,GxEPD_BLACK,"BC200N01");

  //  top hline
  printHLine(40,4, GxEPD_BLACK);

  // details
  printLeftAlignedText(48,font9,GxEPD_BLACK,"Prod: HBP36748");
  printLeftAlignedText(66,font9,GxEPD_BLACK,"MO:   123467889999765");

  // statusbar
  printHLine(115,70, GxEPD_RED);
  printCenteredText(133,font24,GxEPD_WHITE,"NOT CLEAN");

  // bottom hline
  printHLine(GxEPD_HEIGHT-40,4, GxEPD_BLACK);

  // batch
  printCenteredText(GxEPD_HEIGHT-30,font18,GxEPD_BLACK,"BTPRD0001");

  //update
  display.update();
}
