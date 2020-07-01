#include "epaper.h"
#include "ArduinoJson.h"
#include <Regexp.h>

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

uint16_t helperExtractColor(const String& c) {
        uint16_t color;
	if (c.equals("red")) {color =GxEPD_RED;}
	else if (c.equals("white")) {color =GxEPD_WHITE;}
	else {color =GxEPD_BLACK;}
	return color;
}


const GFXfont* helperSizeToFont(uint8_t size) {
  const GFXfont* font;
  switch (size)
  {
    case 9:
      font = font9;
      break;
    case 12:
      font = font12;
      break;
    case 18:
      font = font18;
      break;
    case 24:
      font = font24;
      break;

    default:
      font = font9;
      break;
  };
  return font;
}


// uint16_t helperGetText(const String& data, const String& text) {
//         uint16_t color;
// 	if (Vc.equals("red")) {color =GxEPD_RED;}
// 	else if (c.equals("white")) {color =GxEPD_WHITE;}
// 	else {color =GxEPD_BLACK;}
// 	return color;
// }

void ePaper::renderLabel(const String& data, const String& layout) 
{
  display.fillScreen(GxEPD_WHITE);

	String dat = "{\"id\":\"myID\"}";
  DynamicJsonDocument dataDoc(512);
  deserializeJson(dataDoc,dat);
  JsonObject dataJson = dataDoc.as<JsonObject>();

	DynamicJsonDocument doc(512);
	//deserializeJson(doc, layout);
	deserializeJson(doc, "[{\"type\":\"centeredText\",\"y\":2,\"size\":18,\"color\":\"black\",\"text\":\"test$id$\"},{\"type\":\"leftText\"},{\"type\":\"hline\",\"y\":40,\"w\":4,\"color\":\"black\"}]");
	// extract the values
	JsonArray array = doc.as<JsonArray>();
	for(JsonVariant v : array) {
		JsonObject obj = v.as<JsonObject>();
		String type = obj["type"];
		if (type.equals("centeredText")) {

			uint16_t y = obj["y"];
			uint16_t color = helperExtractColor(obj["color"]);
      const GFXfont* font = helperSizeToFont(obj["size"]);

			String rawtext = obj["text"];
      String text;
      MatchState ms;
      char buf [100];
      strcpy (buf, rawtext.c_str());

      ms.Target(buf);

      char result = ms.Match ("(%a*)$(%a+)$(%a*)", 0);
      if (result == REGEXP_MATCHED)
      {
        // matching offsets in ms.capture
        String prefix = ms.GetCapture (buf, 0);
        String key = ms.GetCapture (buf, 1);
        String sufix = ms.GetCapture (buf, 2);
        String variableValue = dataDoc[key];
        text = prefix + variableValue + sufix;
      }
      else if (result == REGEXP_NOMATCH)
      {
        // no match - no variable to replace
        text = rawtext;
      }
      else
      {
        // some sort of error
        text = "error";
      }

			
			printCenteredText(y,font,color,text.c_str());

		} else if (type.equals("hline")) {
			uint16_t y = obj["y"];
			uint16_t w = obj["w"];

			uint16_t color;
			String c = obj["color"];
			if (c.equals("red")) {color =GxEPD_RED;}
			else if (c.equals("white")) {color =GxEPD_WHITE;}
			else {color =GxEPD_BLACK;}

			printHLine(y,w, color);

		} else if (type.equals("leftText")) {
		
		}

	}
  display.update();
}

void ePaper::renderLabelTest( const String & data,  const String & layout)
{
  Serial.println("***** test Rendering *****");
  unsigned long startTime = millis();
  showText(font9, String(data+layout).c_str());
  Serial.print("********** ");
  Serial.print(millis() - startTime);
  Serial.println("ms *****");

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
  #ifndef HAS_RED_COLOR
    if (color == GxEPD_RED) {color = GxEPD_BLACK;}
  #endif
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
