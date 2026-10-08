// CalendarProxy.gs
// Deploy this as a Google Apps Script Web App to expose a small JSON
// feed of your upcoming Google Calendar events, for the ESP32 HUD to fetch.
//
// SETUP:
// 1. Go to https://script.google.com -> New project.
// 2. Delete the placeholder code and paste this whole file in.
// 3. Change SECRET_KEY below to your own random string (anything works,
//    just keep it private - think of it like a password).
// 4. Click "Deploy" -> "New deployment".
//      - Select type: Web app
//      - Execute as: Me
//      - Who has access: Anyone
//      - Click Deploy, then click "Authorize access" and approve the
//        permissions (this is you granting your own script access to
//        your own calendar).
// 5. Copy the Web app URL it gives you (ends in /exec).
// 6. In your ESP32 project's config.h, set:
//      #define CALENDAR_URL "<paste URL here>?key=<your SECRET_KEY>"
//
// NOTE ON PRIVACY: "Anyone" access means anyone who has the exact URL
// could call this script - that's why we check a shared secret key below
// and reject requests without it. Treat the deployment URL + key the same
// way you already treat API_KEY in config.h: keep it out of git, don't
// share it.
//
// If you ever want to revoke access, go to script.google.com -> this
// project -> Deploy -> Manage deployments -> Archive.

var SECRET_KEY = "change-this-to-your-own-secret";
var LOOKAHEAD_DAYS = 7; // how many days ahead to look for events
var MAX_EVENTS = 4;     // how many upcoming events to return
                        // (keep in sync with MAX_CALENDAR_EVENTS in calendar.h,
                        //  and redeploy a NEW VERSION after changing it)

function doGet(e) {
  if (!e.parameter.key || e.parameter.key !== SECRET_KEY) {
    return ContentService
      .createTextOutput(JSON.stringify({ error: "unauthorized" }))
      .setMimeType(ContentService.MimeType.JSON);
  }

  var calendar = CalendarApp.getDefaultCalendar();
  var now = new Date();
  var until = new Date(now.getTime() + LOOKAHEAD_DAYS * 24 * 60 * 60 * 1000);
  var events = calendar.getEvents(now, until);
  var timeZone = Session.getScriptTimeZone();
  var todayLabel = Utilities.formatDate(now, timeZone, "EEE");

  var results = [];
  for (var i = 0; i < events.length && results.length < MAX_EVENTS; i++) {
    var ev = events[i];
    var start = ev.getStartTime();
    var dayLabel = Utilities.formatDate(start, timeZone, "EEE");
    var timeLabel = ev.isAllDayEvent()
      ? "All day"
      : Utilities.formatDate(start, timeZone, "HH:mm");
    // Only prefix with the day name if the event isn't today
    var label = (dayLabel === todayLabel) ? timeLabel : (dayLabel + " " + timeLabel);

    results.push({ time: label, title: ev.getTitle() });
  }

  return ContentService
    .createTextOutput(JSON.stringify(results))
    .setMimeType(ContentService.MimeType.JSON);
}
