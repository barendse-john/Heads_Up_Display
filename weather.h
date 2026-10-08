// weather.h
// Handles fetching and parsing weather data

#include <HTTPClient.h>
#include <ArduinoJson.h>

// Default city comes from config.h (DEFAULT_CITY) so your real location
// stays out of git. Falls back to London if config.h doesn't set it.
#ifndef DEFAULT_CITY
#define DEFAULT_CITY "London,GB"
#endif
char currentCity[40] = DEFAULT_CITY;

// Cached values so the weather screen can be redrawn (e.g. when the
// display switches back to it) without hitting the API again.
float lastTemp = 0;
String lastDescription = "";

// currentCity is stored as "City,CC" for the API query;
// this strips the country code for display purposes.
String cityDisplayName() {
  String c = String(currentCity);
  int commaIdx = c.indexOf(',');
  if (commaIdx > 0) return c.substring(0, commaIdx);
  return c;
}

void fetchWeather() {
  HTTPClient http;
  String url = "http://api.openweathermap.org/data/2.5/weather?q=" + String(currentCity) + "&appid=" + API_KEY + "&units=metric";

  http.begin(url);
  int httpCode = http.GET();
  Serial.print("HTTP code: ");
  Serial.println(httpCode);

  if (httpCode == 200) {
    String payload = http.getString();

    JsonDocument doc;
    deserializeJson(doc, payload);

    lastTemp = doc["main"]["temp"];
    lastDescription = String((const char*) doc["weather"][0]["description"]);

    Serial.print("Temperature: ");
    Serial.println(lastTemp);
    Serial.print("Description: ");
    Serial.println(lastDescription);

    // Only redraw the LCD if the weather screen is actually the one
    // showing right now; otherwise just keep the cached values updated.
    if (currentScreen == SCREEN_WEATHER) {
      displayWeather(lastTemp, cityDisplayName(), lastDescription);
    }

  } else {
    Serial.print("Weather fetch failed, code: ");
    Serial.println(httpCode);
    if (currentScreen == SCREEN_WEATHER) {
      displayMessage("Weather error", "Code: " + String(httpCode));
    }
  }

  http.end();
}
