// display.h
// Owns the LCD object and all screen-drawing functions

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const uint8_t LCD_COLS = 16;
const unsigned long SCROLL_PAUSE_MS = 1200; // pause at the start of each loop

// Time between scroll steps. Not a constant - the potentiometer adjusts
// this at runtime (see potUpdate() in the .ino). Lower = faster.
unsigned long scrollStepMs = 300;
const unsigned long SCROLL_STEP_MIN_MS = 120; // knob fully clockwise
const unsigned long SCROLL_STEP_MAX_MS = 700; // knob fully anticlockwise

// Tracks the scrolling state of a single LCD row
struct ScrollLine {
  String text = "";
  int textLen = 0;
  int scrollPos = 0;
  unsigned long lastStepMs = 0;
  bool pausing = false;
  bool completedPass = false; // has the full text been shown at least once?
};

ScrollLine lcdLine1;
ScrollLine lcdLine2;

// Which screen is currently shown on the LCD
enum Screen { SCREEN_WEATHER, SCREEN_CALENDAR };
Screen currentScreen = SCREEN_WEATHER;

void displayInit() {
  lcd.init();
  lcd.backlight();
}

// Sets the text for one row (0 or 1) and (re)starts its scroll animation
// if the text is too long to fit on the screen.
void setLcdLine(uint8_t row, String text) {
  ScrollLine &line = (row == 0) ? lcdLine1 : lcdLine2;

  // If this row already shows exactly this text, leave its scroll
  // animation running instead of snapping back to the start. Without
  // this, any periodic redraw restarts long lines from the beginning
  // and they never scroll all the way through.
  if (line.text == text) return;

  line.text = text;
  line.textLen = text.length();
  line.scrollPos = 0;
  line.pausing = true;
  line.completedPass = false;
  line.lastStepMs = millis();

  lcd.setCursor(0, row);
  if (line.textLen <= LCD_COLS) {
    String padded = text;
    while (padded.length() < LCD_COLS) padded += ' ';
    lcd.print(padded);
  } else {
    lcd.print(text.substring(0, LCD_COLS));
  }
}

void displayMessage(String line1, String line2) {
  lcd.clear();
  setLcdLine(0, line1);
  setLcdLine(1, line2);
}

void displayWeather(float temp, String city, String description) {
  lcd.clear();
  setLcdLine(0, String(temp, 1) + "C " + city);
  setLcdLine(1, description);
}

// Advances one row's scroll animation if it's due for a step.
void stepScrollLine(uint8_t row, ScrollLine &line) {
  if (line.textLen <= LCD_COLS) return; // fits on screen, nothing to scroll

  unsigned long now = millis();
  unsigned long interval = line.pausing ? SCROLL_PAUSE_MS : scrollStepMs;
  if (now - line.lastStepMs < interval) return;

  line.lastStepMs = now;

  if (line.pausing) {
    line.pausing = false; // pause finished, resume scrolling next steps
  } else {
    line.scrollPos++;
    int maxScroll = line.textLen - LCD_COLS + 3; // +3 = gap before the text repeats
    if (line.scrollPos >= maxScroll) {
      line.scrollPos = 0;
      line.pausing = true;        // pause again at the start of the text
      line.completedPass = true;  // the whole line has now been shown once
    }
  }

  String wrapped = line.text + "   " + line.text; // gap + repeat so it loops smoothly
  lcd.setCursor(0, row);
  lcd.print(wrapped.substring(line.scrollPos, line.scrollPos + LCD_COLS));
}

// Call this frequently from loop() to animate any text too long to fit.
void displayScrollUpdate() {
  stepScrollLine(0, lcdLine1);
  stepScrollLine(1, lcdLine2);
}

// True once every row that needed scrolling has shown its full text at
// least once. Rows short enough to fit count as done immediately.
// Callers use this to avoid changing the screen mid-scroll.
bool displayFinishedScrolling() {
  bool row0Done = (lcdLine1.textLen <= LCD_COLS) || lcdLine1.completedPass;
  bool row1Done = (lcdLine2.textLen <= LCD_COLS) || lcdLine2.completedPass;
  return row0Done && row1Done;
}
