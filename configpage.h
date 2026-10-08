// configpage.h
// Runs a small web page for changing the city while connected

#include <WebServer.h>

WebServer server(80);

void handleRoot() {
  String html = "<html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background: #1e2a38; color: white; text-align: center; padding: 30px 20px; }";
  html += "h2 { color: #4fc3f7; }";
  html += ".card { background: #2c3e50; border-radius: 12px; padding: 20px; max-width: 350px; margin: 20px auto; }";
  html += "input[type=text] { width: 80%; padding: 12px; font-size: 16px; border-radius: 8px; border: none; margin-top: 10px; }";
  html += "input[type=submit] { width: 85%; padding: 14px; font-size: 16px; border-radius: 8px; border: none; background: #4fc3f7; color: #1e2a38; font-weight: bold; margin-top: 15px; }";
  html += "</style>";
  html += "</head><body>";

  html += "<h2>Heads Up Display</h2>";
  html += "<div class='card'>";
  html += "<p>Current city:<br><strong>" + String(currentCity) + "</strong></p>";
  html += "<form action='/save' method='GET'>";
  html += "<input type='text' name='city' placeholder='e.g. London,GB'>";
  html += "<br><input type='submit' value='Update City'>";
  html += "</form>";
  html += "</div>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleSave() {
  if (server.hasArg("city")) {
    String newCity = server.arg("city");
    newCity.toCharArray(currentCity, 40);
    Serial.print("City updated to: ");
    Serial.println(currentCity);
  }

  fetchWeather();

  server.sendHeader("Location", "/");
  server.send(303);
}

void webserverInit() {
  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.begin();
  Serial.println("Web server started.");
}

void webserverHandle() {
  server.handleClient();
}