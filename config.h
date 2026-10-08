// config.example.h
// Copy this file to config.h and fill in your own values.
// config.h is git-ignored, so your keys never get committed.

// OpenWeatherMap API key (https://home.openweathermap.org/api_keys)
#define API_KEY "your_openweathermap_key_here"

// Deploy CalendarProxy.gs as a Google Apps Script Web App (see the
// instructions at the top of that file), then paste the URL it gives you
// here, with your secret key appended as a query parameter.
#define CALENDAR_URL "https://script.google.com/macros/s/YOUR_DEPLOYMENT_ID/exec?key=YOUR_SECRET_KEY"

// Default city for the weather screen, as "City,CC"
#define DEFAULT_CITY "Kampala, UG"
