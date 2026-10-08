#include <WiFiManager.h>
#include "config.h"
#include "display.h"
#include "weather.h"
#include "calendar.h"
#include "configpage.h"

WiFiManager wm;

// --- Screen rotation ---
const unsigned long SCREEN_ROTATE_MS = 2UL * 60UL * 1000UL; // 2 minutes
unsigned long lastScreenSwitchMs = 0;

// --- Manual screen button ---
// Wire a push button between this pin and GND - no external resistor
// needed, the internal pull-up is enabled in setup(). Avoid GPIO21/22
// (used by the LCD's I2C bus) and GPIO34-39 (no internal pull-ups).
const int BUTTON_PIN = 32;
int lastRawButtonReading = HIGH;
int buttonState = HIGH; // debounced state, HIGH = not pressed (INPUT_PULLUP)
unsigned long lastButtonDebounceMs = 0;
const unsigned long BUTTON_DEBOUNCE_MS = 50;

// --- Button debugging ---
// Set to false once the button is working to quieten the serial output.
const bool BUTTON_DEBUG = true;
unsigned long lastButtonHeartbeatMs = 0;
const unsigned long BUTTON_HEARTBEAT_MS = 2000;

// --- Scroll speed potentiometer ---
// MUST be an ADC1 pin (GPIO32-39). ADC2 pins (0/2/4/12-15/25-27) stop
// working the moment WiFi is active, which would break this silently.
// Wiper (middle leg) here, outer legs to 3V3 and GND.
const int POT_PIN = 33;
const unsigned long POT_READ_MS = 50;      // how often to sample the knob
const int POT_DEADBAND = 80;               // ignore ADC jitter below this
const unsigned long POT_OVERLAY_MS = 1200; // overlay lingers this long after the last move

int lastPotRaw = -10000; // forces a read on the first pass
unsigned long lastPotReadMs = 0;
unsigned long lastPotMoveMs = 0;
bool potOverlayShowing = false;

// --- Background data refresh ---
const unsigned long WEATHER_REFRESH_MS = 5UL * 60UL * 1000UL;  // 5 minutes
const unsigned long CALENDAR_REFRESH_MS = 5UL * 60UL * 1000UL; // 5 minutes
unsigned long lastWeatherFetchMs = 0;
unsigned long lastCalendarFetchMs = 0;

// Repaints whatever screen is already active, without resetting the
// auto-rotation timer. Used to restore the display after the pot overlay.
void redrawCurrentScreen() {
  if (currentScreen == SCREEN_WEATHER) {
    displayWeather(lastTemp, cityDisplayName(), lastDescription);
  } else {
    displayCalendarScreen();
  }
}

void switchScreen(Screen screen) {
  currentScreen = screen;
  lastScreenSwitchMs = millis();
  redrawCurrentScreen();
}

void toggleScreen() {
  switchScreen(currentScreen == SCREEN_WEATHER ? SCREEN_CALENDAR : SCREEN_WEATHER);
}

// Averages several samples - a single analogRead() on the ESP32 is noisy
// enough to make the value jump around on its own.
int readPotSmoothed() {
  long total = 0;
  for (int i = 0; i < 8; i++) total += analogRead(POT_PIN);
  return (int)(total / 8);
}

void showScrollSpeedOverlay(int raw) {
  int filled = (raw * 10) / 4095;
  if (filled > 10) filled = 10;

  String bar = "";
  for (int i = 0; i < 10; i++) bar += (i < filled) ? '#' : '-';

  setLcdLine(0, "Scroll speed");
  setLcdLine(1, bar + " " + String(scrollStepMs) + "ms");
}

void potUpdate() {
  unsigned long now = millis();

  if (now - lastPotReadMs >= POT_READ_MS) {
    lastPotReadMs = now;
    int raw = readPotSmoothed();

    if (abs(raw - lastPotRaw) > POT_DEADBAND) {
      lastPotRaw = raw;
      lastPotMoveMs = now;

      // Clockwise (higher reading) = shorter delay = faster scrolling
      scrollStepMs = SCROLL_STEP_MAX_MS -
        ((unsigned long) raw * (SCROLL_STEP_MAX_MS - SCROLL_STEP_MIN_MS)) / 4095UL;

      potOverlayShowing = true;
      showScrollSpeedOverlay(raw);
    }
  }

  // Put the real screen back once the knob has been still for a moment
  if (potOverlayShowing && (now - lastPotMoveMs) >= POT_OVERLAY_MS) {
    potOverlayShowing = false;
    redrawCurrentScreen();
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  analogReadResolution(12); // 0-4095, matches the maths in potUpdate()

  displayInit();
  displayMessage("WiFi setup...", "");

  bool connected = wm.autoConnect("ESP32-Setup");

  if (connected) {
    Serial.println("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    displayMessage("WiFi connected!", "");
    delay(1500);

    webserverInit();
    fetchWeather();
    fetchCalendar();

    unsigned long now = millis();
    lastWeatherFetchMs = now;
    lastCalendarFetchMs = now;

    switchScreen(SCREEN_WEATHER);
  } else {
    Serial.println("WiFi FAILED to connect.");
    displayMessage("WiFi failed", "");
  }
}

void loop() {
  webserverHandle();
  displayScrollUpdate();
  potUpdate();

  unsigned long now = millis();

  // Auto-rotate between screens
  if (now - lastScreenSwitchMs >= SCREEN_ROTATE_MS) {
    toggleScreen();
  }

  // Manual override button (debounced, active LOW)
  int reading = digitalRead(BUTTON_PIN);

  if (BUTTON_DEBUG) {
    // Heartbeat: proves loop() is running and shows the resting pin level
    if (now - lastButtonHeartbeatMs >= BUTTON_HEARTBEAT_MS) {
      lastButtonHeartbeatMs = now;
      Serial.print("[btn] alive, pin reads: ");
      Serial.println(reading);
    }
    // Fires the instant the raw pin level changes, before debouncing
    if (reading != lastRawButtonReading) {
      Serial.print("[btn] RAW CHANGE -> ");
      Serial.println(reading);
    }
  }

  if (reading != lastRawButtonReading) {
    lastButtonDebounceMs = now;
  }
  if ((now - lastButtonDebounceMs) > BUTTON_DEBOUNCE_MS) {
    if (reading != buttonState) {
      buttonState = reading;
      if (BUTTON_DEBUG) {
        Serial.print("[btn] debounced state -> ");
        Serial.println(buttonState);
      }
      if (buttonState == LOW) {
        if (BUTTON_DEBUG) Serial.println("[btn] PRESS -> toggling screen");
        toggleScreen();
      }
    }
  }
  lastRawButtonReading = reading;

  // Keep cached data fresh in the background, regardless of which
  // screen is currently showing
  if (now - lastWeatherFetchMs >= WEATHER_REFRESH_MS) {
    lastWeatherFetchMs = now;
    fetchWeather();
  }
  if (now - lastCalendarFetchMs >= CALENDAR_REFRESH_MS) {
    lastCalendarFetchMs = now;
    fetchCalendar();
  }

  // Page through calendar events while that screen is active. Suppressed
  // while the pot overlay is up so paging can't overwrite it.
  if (currentScreen == SCREEN_CALENDAR && !potOverlayShowing) {
    calendarScreenUpdate();
  }
}
