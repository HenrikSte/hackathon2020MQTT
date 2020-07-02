#include "epaper.h"
#include "ArduinoJson.h"
#include <Regexp.h>
#include "bitmaps.h"
#include "HardwareSerial.h"

#define DEBUG_RENDER

//#include GxEPD_BitmapExamples

ePaper::ePaper() :
  io(SPI, /*CS=5*/ PIN_CS, /*DC=*/ PIN_DC, /*RST=*/ PIN_RST),
  display(io, /*RST=*/ PIN_RST, /*BUSY=*/ PIN_BUSY)
{
}


uint16_t helperExtractColor(const String& c) {
        uint16_t color;
	if (c.equals("red")) {color =GxEPD_RED;}
	else if (c.equals("white")) {color =GxEPD_WHITE;}
	else {color =GxEPD_BLACK;}
	return color;
}


const GFXfont* helperSizeToFont(uint8_t size, bool bold) {
  const GFXfont* font;
  if (bold)
  {
    switch (size)
    {
      case 9:
        font = font9b;
        break;
      case 12:
        font = font12b;
        break;
      case 18:
        font = font18b;
        break;
      case 24:
        font = font24b;
        break;

      default:
        font = font9b;
        break;
    };
  }
  else
  {
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
  }
  return font;
}


char buf [256];
char raw [256];

String helperGetText(DynamicJsonDocument &dataDoc, const String& rawtext) {
  String text = rawtext;
  MatchState ms;
  strcpy (raw, rawtext.c_str());

  ms.Target(raw);

  #ifdef DEBUG_RENDER
    Serial.print("dataDoc:");
    serializeJsonPretty(dataDoc,Serial);

    Serial.print("raw:");
    Serial.println(raw);
  #endif

  String exp = "[^$]*$([^$^%s]+)$[^$]*";
  // keep matching until we run out of matches
  for (unsigned int index = 0;
       char result = ms.Match (exp.c_str(), index) > 0 &&
       index < ms.src_len;                      // otherwise empty matches loop
    ){
    // increment index ready for next time, go forwards at least one byte
    index = ms.MatchStart + (ms.MatchLength == 0 ? 1 : ms.MatchLength);

    if (result == REGEXP_MATCHED)
    {
      // matching offsets in ms.capture
      String key = ms.GetCapture (buf, 0);
      String placeholder = "$" + key + "$";


      //TODO how to handle bool and numbers?
      String variableValue = (const String&)dataDoc[key];
      if (variableValue == "null") {
        variableValue = "";
      }
      else if (!variableValue) {
        variableValue = placeholder;
      }

      text.replace(placeholder,variableValue);

      #ifdef DEBUG_RENDER
        Serial.print("key:");
        Serial.println(key);
        Serial.print("placeholder:");
        Serial.println(placeholder);
        Serial.print("variableValue:");
        Serial.println(variableValue);
        Serial.print("variableValue.isEmpty:");
        Serial.println(variableValue.isEmpty());
        Serial.print("text:");
        Serial.println(text);
      #endif

    }  // end of match_callback 
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
  }

  return text;
}

bool helperIsVisible(DynamicJsonDocument &dataDoc, const String& rawVisibleFlag, const String& rawHiddenFlag)
{
  bool isVisible = true;
  #ifdef DEBUG_RENDER
    Serial.print("rawVisibleFlag:");
    Serial.println(rawVisibleFlag);
    Serial.print("rawHiddenFlag:");
    Serial.println(rawHiddenFlag);
  #endif
  if (!rawVisibleFlag.equals("null")) {
    String isVisibleText = helperGetText(dataDoc, rawVisibleFlag);
    #ifdef DEBUG_RENDER
      Serial.print("isVisibleText:");
      Serial.println(isVisibleText);
    #endif
    if (isVisibleText.equals("true")) {
      isVisible = true;
    }else{
      isVisible = false;
    }
  }else if (!rawHiddenFlag.equals("null")) {
    String isHiddenText = helperGetText(dataDoc, rawHiddenFlag);
    #ifdef DEBUG_RENDER
      Serial.print("isHiddenText:");
      Serial.println(isHiddenText);
    #endif
    if (isHiddenText.equals("true")) {
      isVisible = false;
    }else{
      isVisible = true;
    }
  }
  return isVisible;
}


void ePaper::renderLabel(const String& data, const String& layout) 
{
  Serial.println("***** renderLabel *****");
  unsigned long startTime = millis();
  
  display.fillScreen(GxEPD_WHITE);

	//String dat = "{\"id\":\"myID\",\"prod\":\"HBP36748\"}";
  DynamicJsonDocument dataDoc(4096);
  deserializeJson(dataDoc,data);


	DynamicJsonDocument layoutDoc(4096);
	deserializeJson(layoutDoc, layout);

	//deserializeJson(layoutDoc, "[{\"type\":\"rightText\",\"y\":2,\"size\":18,\"color\":\"black\",\"text\":\"test$id$?\"},{\"type\":\"leftText\",\"y\":48,\"size\":9,\"color\":\"black\",\"text\":\"Prod: $prod$\"},{\"type\":\"hline\",\"y\":40,\"w\":4,\"color\":\"black\"}]");
	// extract the values
	JsonArray array = layoutDoc.as<JsonArray>();
	for(JsonVariant v : array) {
		JsonObject obj = v.as<JsonObject>();
		String type = obj["type"];
    bool isVisible = helperIsVisible(dataDoc,obj["visible"],obj["hidden"]);

    //skip if hidden
    if (!isVisible) {
      Serial.println("element "+type+" should be hidden. Skipping");
      continue;
    }

		if (type.equals("centeredText")) {

			uint16_t y = obj["y"];
  		uint16_t color = helperExtractColor(obj["color"]);
      const GFXfont* font = helperSizeToFont(obj["size"], obj["bold"]);
			String text = helperGetText(dataDoc, obj["text"]);
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
			uint16_t y = obj["y"];
			uint16_t color = helperExtractColor(obj["color"]);
      const GFXfont* font = helperSizeToFont(obj["size"], obj["bold"]);
			String text = helperGetText(dataDoc, obj["text"]);

			printLeftAlignedText(y,font,color,text.c_str());

		} else if (type.equals("rightText")) {
			uint16_t y = obj["y"];
			uint16_t color = helperExtractColor(obj["color"]);
      const GFXfont* font = helperSizeToFont(obj["size"], obj["bold"]);
			String text = helperGetText(dataDoc, obj["text"]);

			printRightAlignedText(y,font,color,text.c_str());
		}

	}
  Serial.print("***** redering took ");
  Serial.print(millis() - startTime);
  Serial.println("ms *****");
  display.update();
  Serial.print("***** all together took ");
  Serial.print(millis() - startTime);
  Serial.println("ms *****");

}



void ePaper::printHLine(uint16_t y, uint16_t width, uint16_t color) {
  #ifndef HAS_RED_COLOR
    if (color == GxEPD_RED) {color = GxEPD_BLACK;}
  #endif
  display.fillRect(0,y,GxEPD_WIDTH,width,color);
}

uint16_t ePaper::printCenteredText(uint16_t y, const GFXfont* f, uint16_t color, const char* text) {
  
  uint16_t textWidth,textHeight;
  if (text && strlen(text))
  {
    display.setFont(f);
    int16_t x1,y1;
    uint16_t heightOfOneLine;
    uint16_t heightOfTwoLines;
    
    display.getTextBounds("QW@$_",0,y,&x1,&y1,&textWidth,&heightOfOneLine);
    display.getTextBounds("QW@$_\r\nQW@$_",0,y,&x1,&y1,&textWidth,&heightOfTwoLines);
    display.getTextBounds(text,0,y,&x1,&y1,&textWidth,&textHeight);
    
    uint16_t xOffset = (GxEPD_WIDTH-textWidth)/2;
    uint16_t yOffset = y                   // Position from layout
                      + (heightOfOneLine); // origin of text is bottom left, so we add on line height, so a 0,0 will draw at top left corner of display

    if (textHeight > heightOfOneLine) 
    {                     
      yOffset -= (heightOfTwoLines-heightOfOneLine)/2; // for multiple Lines we go up 1/2 the difference, kinda-vertical-center
    }

    display.setCursor(xOffset,yOffset);
    display.setTextColor(color);
    display.println(text);

  }    
  //return hight of line
  return textHeight;
}

uint16_t ePaper::printLeftAlignedText(uint16_t y, const GFXfont* f, uint16_t color, const char* text) {

  int16_t x1,y1;
  uint16_t textHeight;

  if (text && strlen(text))
  {
    display.setFont(f);
    uint16_t heightOfOneLine,heightOfTwoLines,textWidth;
    
    display.getTextBounds("QW@$_",0,y,&x1,&y1,&textWidth,&heightOfOneLine);
    display.getTextBounds("QW@$_\r\nQW@$_",0,y,&x1,&y1,&textWidth,&heightOfTwoLines);
    display.getTextBounds(text,0,y,&x1,&y1,&textWidth,&textHeight);
    
    uint16_t yOffset = y                   // Position from layout
                      + (heightOfOneLine); // origin of text is bottom left, so we add on line height, so a 0,0 will draw at top left corner of display

    if (textHeight - heightOfOneLine > (heightOfOneLine/2)) 
    {                     
      yOffset -= (heightOfTwoLines-heightOfOneLine)/2; // for multiple Lines we go up 1/2 the difference, kinda-vertical-center
    }
    
    display.setCursor(0,yOffset);
    display.setTextColor(color);
    display.println(text);
  }
  //return hight of line
  return textHeight;
}

uint16_t ePaper::printRightAlignedText(uint16_t y, const GFXfont* f, uint16_t color, const char* text) {

  int16_t x1,y1;
  uint16_t textHeight;

  if (text && strlen(text))
  {
    display.setFont(f);
    uint16_t heightOfOneLine,heightOfTwoLines,textWidth;
    
    display.getTextBounds("QW@$_",0,y,&x1,&y1,&textWidth,&heightOfOneLine);
    display.getTextBounds("QW@$_\r\nQW@$_",0,y,&x1,&y1,&textWidth,&heightOfTwoLines);
    display.getTextBounds(text,0,y,&x1,&y1,&textWidth,&textHeight);
    
    uint16_t yOffset = y                   // Position from layout
                      + (heightOfOneLine); // origin of text is bottom left, so we add on line height, so a 0,0 will draw at top left corner of display

    if (textHeight - heightOfOneLine > (heightOfOneLine/2)) 
    {                     
      yOffset -= (heightOfTwoLines-heightOfOneLine)/2; // for multiple Lines we go up 1/2 the difference, kinda-vertical-center
    }

    display.setCursor(GxEPD_WIDTH-textWidth,y+heightOfOneLine);
    display.setTextColor(color);
    display.println(text);
  }
  //return hight of line
  return textHeight;
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


void ePaper::renderLabelTest( const String & data,  const String & layout)
{
  Serial.println("***** test Rendering *****");
  unsigned long startTime = millis();
  showText(font9, String(data+layout).c_str());
  Serial.print("********** ");
  Serial.print(millis() - startTime);
  Serial.println("ms *****");




}

void ePaper::splashScreen()
{
  display.fillScreen(GxEPD_WHITE);

  if (GxEPD_WIDTH == 800)
  {
    display.drawBitmap(0,0,msiHackathon800x480,800,480, GxEPD_BLACK);
  }
  else if (GxEPD_WIDTH == 400)
  {
    display.drawBitmap(0,0,msiHackathon400x300,400,300, GxEPD_BLACK);
  }

  display.update(); 
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
