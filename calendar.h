// calendar.h
// Fetches upcoming events from a small Google Apps Script JSON proxy
// (see CalendarProxy.gs for the script you deploy on script.google.com,
// and config.h for where CALENDAR_URL is set).
//
// The proxy returns a JSON array like:
//   [{"time":"14:00","title":"Dentist"}, {"time":"Fri 09:00","title":"Standup"}]

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// Set to false once the calendar is working to quieten the serial output.
const bool CALENDAR_DEBUG = true;

// NOTE: CalendarProxy.gs has its own MAX_EVENTS - raising this alone does
// nothing until that script is updated to match AND redeployed as a new
// version.
const int MAX_CALENDAR_EVENTS = 4;

// A page is held for at least CALENDAR_PAGE_MIN_MS, and then until any
// long titles on it have scrolled all the way through once - capped at
// CALENDAR_PAGE_MAX_MS so a very long title can't stall the rotation.
const unsigned long CALENDAR_PAGE_MIN_MS = 4000;
const unsigned long CALENDAR_PAGE_MAX_MS = 20000;

struct CalendarEvent {
  String time;
  String title;
};

CalendarEvent calendarEvents[MAX_CALENDAR_EVENTS];
int calendarEventCount = 0;

int calendarPage = 0;
unsigned long lastCalendarPageMs = 0;

// Draws the current page (up to 2 events, one per row) of the calendar screen
void renderCalendarPage() {
  if (calendarEventCount == 0) {
    setLcdLine(0, "No events");
    setLcdLine(1, "");
    return;
  }

  int idx0 = calendarPage * 2;
  int idx1 = idx0 + 1;

  setLcdLine(0, idx0 < calendarEventCount ? (calendarEvents[idx0].time + " " + calendarEvents[idx0].title) : "");
  setLcdLine(1, idx1 < calendarEventCount ? (calendarEvents[idx1].time + " " + calendarEvents[idx1].title) : "");
}

// Call this when switching to the calendar screen
void displayCalendarScreen() {
  lcd.clear();
  calendarPage = 0;
  lastCalendarPageMs = millis();
  renderCalendarPage();
}

// Call every loop() while the calendar screen is active. Pages through
// events two at a time when there are more than 2 to show, since the LCD
// only has 2 rows.
void calendarScreenUpdate() {
  int totalPages = (calendarEventCount + 1) / 2;
  if (totalPages <= 1) return;

  unsigned long now = millis();
  unsigned long shownFor = now - lastCalendarPageMs;

  // Always hold a page for a minimum time so it's readable
  if (shownFor < CALENDAR_PAGE_MIN_MS) return;

  // Then keep holding it until any long titles have scrolled through
  // once, so we never cut a title off half way. The max is a safety
  // valve for titles so long they'd otherwise hold the page forever.
  if (!displayFinishedScrolling() && shownFor < CALENDAR_PAGE_MAX_MS) return;

  lastCalendarPageMs = now;
  calendarPage = (calendarPage + 1) % totalPages;
  renderCalendarPage();
}

void fetchCalendar() {
  // Apps Script is HTTPS-only, so this needs a TLS-capable client -
  // plain http.begin(url) (which works for the OpenWeatherMap call)
  // silently fails here. setInsecure() skips certificate validation,
  // which is fine for reading your own calendar feed.
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  // Apps Script ALWAYS answers a /exec request with a 302 redirect over
  // to script.googleusercontent.com - without this the ESP32 stops at
  // the redirect and never sees the JSON.
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.setTimeout(15000); // Apps Script can be slow on a cold start

  http.begin(client, CALENDAR_URL);
  int httpCode = http.GET();
  Serial.print("Calendar HTTP code: ");
  Serial.println(httpCode);

  if (httpCode == 200) {
    String payload = http.getString();

    if (CALENDAR_DEBUG) {
      Serial.print("Calendar payload: ");
      Serial.println(payload.substring(0, 200));
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);

    if (err) {
      Serial.print("Calendar JSON parse failed: ");
      Serial.println(err.c_str());
    } else if (doc["error"].is<const char*>()) {
      // The proxy rejected us - almost always a key mismatch
      Serial.print("Calendar proxy returned an error: ");
      Serial.println((const char*) doc["error"]);
      Serial.println("-> check the ?key=... on CALENDAR_URL matches SECRET_KEY in CalendarProxy.gs");
    } else if (doc.is<JsonArray>()) {
      JsonArray arr = doc.as<JsonArray>();
      calendarEventCount = 0;
      for (JsonObject ev : arr) {
        if (calendarEventCount >= MAX_CALENDAR_EVENTS) break;
        calendarEvents[calendarEventCount].time = String((const char*) ev["time"]);
        calendarEvents[calendarEventCount].title = String((const char*) ev["title"]);
        calendarEventCount++;
      }
      Serial.print("Calendar events loaded: ");
      Serial.println(calendarEventCount);
    } else {
      Serial.println("Calendar response was not a JSON array");
    }
  } else {
    Serial.print("Calendar fetch failed, code: ");
    Serial.println(httpCode);
    if (httpCode < 0) {
      Serial.println("-> negative code = connection/TLS problem, not an HTTP error");
    }
  }

  http.end();

  // Only redraw the LCD if the calendar screen is actually showing right
  // now; otherwise just keep the cached events updated in the background.
  if (currentScreen == SCREEN_CALENDAR) {
    displayCalendarScreen();
  }
}
