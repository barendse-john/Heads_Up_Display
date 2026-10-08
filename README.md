# HeadsUpDisplay

A small desk display built on an **ESP32** and a **16×2 I²C LCD**. It switches
between the current weather and your next few Google Calendar events.

- **Weather screen**: temperature and conditions from the OpenWeatherMap API
- **Calendar screen**: your next 4 events in the coming 7 days, fetched through
  a small Google Apps Script proxy (`CalendarProxy.gs`) protected by a secret key
- **Auto-rotation** between the screens every 2 minutes, plus a **push button**
  to switch manually
- **Potentiometer** to set how fast long lines scroll, with an on-screen speed bar
- **WiFi setup portal** (WiFiManager), so there are no WiFi passwords in the code
- **Built-in web page** to change the weather city from your phone or laptop
- Weather and calendar data refresh in the background every 5 minutes

## How the pieces fit together

Deploying this means setting up three things:

```
 Google Calendar ──► Apps Script web app ──(HTTPS JSON)──┐
   (your account)    CalendarProxy.gs                     │
                     step 6                               ▼
                                                   ESP32 + LCD ◄── your WiFi
 OpenWeatherMap API ──────────────(HTTP JSON)─────────────▲   (set up on first boot,
   free API key, step 5                                        step 9)
```

1. **A Google Apps Script** in your own Google account that reads your calendar
   and serves your next events as JSON (step 6).
2. **A private `config.h`** on your computer holding your API key, the script
   URL and your city (step 7). It is compiled into the firmware.
3. **The ESP32 firmware**, built and flashed over USB (step 8), then connected
   to your WiFi through a setup portal (step 9).

Budget about an hour the first time. Most of it is the Google script.

### Deployment checklist

- [ ] Parts wired up (sections 1–2)
- [ ] Board support and libraries installed (section 3)
- [ ] Code downloaded (section 4)
- [ ] OpenWeatherMap API key created (section 5)
- [ ] `CalendarProxy.gs` deployed as a **Web app** with access **Anyone**, and
      tested in a browser (section 6)
- [ ] `config.h` created and filled in (section 7)
- [ ] Firmware built and uploaded (section 8)
- [ ] ESP32 joined to your WiFi through `ESP32-Setup` (section 9)
- [ ] Serial Monitor shows `HTTP code: 200` and `Calendar events loaded: N`
      (section 9)

---

## 1. Parts list

| Qty | Part | Notes |
|---|---|---|
| 1 | ESP32 dev board | Any classic ESP32 "DevKit" board (PlatformIO board `esp32dev`). Not the S2/S3/C3 variants unless you change the pins. |
| 1 | 16×2 character LCD with I²C backpack | The common blue/green HD44780 module with a PCF8574 backpack soldered on the back. Usual address is `0x27` (some are `0x3F`). |
| 1 | Momentary push button | Any normally-open tactile switch. |
| 1 | 10 kΩ potentiometer | Linear (B10K). Any value from 5 kΩ to 100 kΩ works. |
| — | Breadboard and jumper wires | |
| 1 | USB cable | Must be a **data** cable, not charge-only. |

## 2. Wiring

| From | To (ESP32) |
|---|---|
| LCD backpack **GND** | GND |
| LCD backpack **VCC** | VIN / 5V (the LCD is dim on 3.3 V) |
| LCD backpack **SDA** | GPIO21 |
| LCD backpack **SCL** | GPIO22 |
| Push button, one leg | GPIO32 |
| Push button, other leg | GND |
| Potentiometer, outer leg 1 | 3V3 |
| Potentiometer, **middle leg (wiper)** | GPIO33 |
| Potentiometer, outer leg 2 | GND |

```
            ESP32
         ┌─────────┐
  LCD VCC┤ VIN     │
  LCD GND┤ GND     │
  LCD SDA┤ GPIO21  │
  LCD SCL┤ GPIO22  │
         │         │
         │ GPIO32  ├──[ button ]── GND
         │         │
         │ 3V3     ├──┐
         │ GPIO33  ├──┼─ pot wiper
         │ GND     ├──┘  (outer legs to 3V3 and GND)
         └─────────┘
```

Things that will bite you if you change the wiring:

- **ESP32 pins are 3.3 V only.** Never connect 5 V to a GPIO. The pot's outer
  legs go to **3V3**, not 5V.
- **The pot must be on an ADC1 pin (GPIO32–39).** ADC2 pins stop giving
  readings as soon as WiFi is on, and nothing tells you why.
- **Wire the button directly to GND.** No resistor is needed because the
  internal pull-up is enabled. Don't put an LED in series with it: the LED's
  voltage drop keeps the pin from ever reading LOW.
- Avoid GPIO34–39 for the button (they have no internal pull-up) and
  GPIO21/22 (used by the LCD).
- To make the pot work the other way round, swap its two outer legs.

## 3. Software you need

Pick **one** of these:

**Option A: PlatformIO (recommended).** Install [VS Code](https://code.visualstudio.com/)
and the PlatformIO extension. `platformio.ini` already lists the board and
libraries, so they are downloaded automatically on the first build.

**Option B: Arduino IDE 2.x.**
1. *File → Preferences → Additional boards manager URLs*, add
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
2. *Tools → Board → Boards Manager*, install **esp32 by Espressif Systems**.
3. *Tools → Manage Libraries*, install:
   - **ArduinoJson** by Benoit Blanchon (version 7.x)
   - **WiFiManager** by tzapu
   - **LiquidCrystal I2C** by Frank de Brabander
4. Select the board **ESP32 Dev Module**.

You may also need the USB-to-serial driver for your board's chip (CP210x or
CH340) if the board doesn't show up as a COM port.

## 4. Get the code

```bash
git clone https://github.com/barendse-john/HeadsUpDisplay.git
```

No git? Use **Code → Download ZIP** on the GitHub page and unzip it. The
folder will be called `HeadsUpDisplay-main`: **rename it to `HeadsUpDisplay`**.
The Arduino IDE only opens a sketch whose folder name matches the `.ino` file.

## 5. Get an OpenWeatherMap API key

1. Make a free account at [openweathermap.org](https://home.openweathermap.org/users/sign_up).
2. Copy your key from the [API keys page](https://home.openweathermap.org/api_keys).

A new key can take up to a couple of hours to start working. Until then the
LCD shows `Weather error / Code: 401`.

## 6. Set up the Google Calendar proxy

The ESP32 doesn't talk to Google Calendar directly (that would need OAuth on
the device). Instead, a tiny Google Apps Script runs in your Google account,
reads your calendar, and returns your next events as simple JSON:

```json
[{"time":"14:00","title":"Dentist"},{"time":"Fri 09:00","title":"Standup"}]
```

1. Sign in to the Google account whose calendar you want to show, go to
   [script.google.com](https://script.google.com) and click **New project**.
   Rename it (click "Untitled project" at the top), e.g. to `CalendarProxy`.
2. Delete the placeholder `function myFunction() {}` and paste in the whole of
   `CalendarProxy.gs`. Press **Ctrl+S** (Cmd+S on a Mac) to save.
3. Change `SECRET_KEY` to a long random string (20+ letters and numbers, no
   spaces, `&`, `?` or `#`, since it goes into a URL). Anyone who has your URL
   and this key can read your event titles, so treat it like a password.
   Save again.
4. *Project Settings (⚙ on the left) → Time zone*: set it to your own time
   zone, otherwise event times will be off.
5. Click the blue **Deploy** button (top right) → **New deployment**, then click
   the **⚙ gear next to "Select type"** and choose **Web app**. (If you pick
   *Library* instead, the URL only returns a "file does not exist" page.)
   - *Description*: anything, e.g. `v1`
   - *Execute as*: **Me**
   - *Who has access*: **Anyone** (not "Anyone with a Google account", or the
     ESP32 gets a 403)
6. Click **Deploy**. The first time, Google asks you to authorize the script:
   **Authorize access** → pick your account → on the "Google hasn't verified
   this app" screen click **Advanced → Go to CalendarProxy (unsafe)** →
   **Allow**. This is your own script asking to read your own calendar, so
   the warning is expected.
7. Copy the **Web app URL** (it starts with
   `https://script.google.com/macros/s/` and ends in `/exec`). If you lose it,
   it's under *Deploy → Manage deployments*.
8. **Test it before going further.** In your browser, open
   `<your URL>?key=<your SECRET_KEY>`. You should see a JSON list of your
   upcoming events (or `[]` if you have none in the next 7 days).
   `{"error":"unauthorized"}` means the key in the URL doesn't match
   `SECRET_KEY`. If this test doesn't work, the ESP32 won't either.

> **Editing the script later?** Changes do nothing until you redeploy:
> *Deploy → Manage deployments → ✏ edit → Version: **New version** → Deploy*.
> This keeps the same URL. Creating a brand-new deployment gives you a new URL
> that you'd have to put in `config.h`.

It only reads your **default** calendar. To use a different one, change
`CalendarApp.getDefaultCalendar()` to
`CalendarApp.getCalendarById("<calendar id>")` (the ID is in that calendar's
settings in Google Calendar).

## 7. Create `config.h`

Make a copy of `config.example.h` in the same folder and name it `config.h`:

```bash
# Windows (Command Prompt)
copy config.example.h config.h
# macOS / Linux
cp config.example.h config.h
```

(On Windows, turn on *View → File name extensions* in Explorer if you copy it
by hand, or you may end up with `config.h.txt`.)

Then open `config.h` and fill in your three values:

```cpp
#define API_KEY      "your_openweathermap_key"
#define CALENDAR_URL "https://script.google.com/macros/s/XXXXXXXX/exec?key=your_secret_key"
#define DEFAULT_CITY "Amsterdam,NL"   // "City,CountryCode"
```

- `API_KEY`: your key from step 5.
- `CALENDAR_URL`: the Web app URL from step 6, with `?key=` and your
  `SECRET_KEY` added to the end. It's exactly the URL you tested in your
  browser.
- `DEFAULT_CITY`: your city and two-letter country code, no space after the
  comma. Check the spelling on [openweathermap.org](https://openweathermap.org)
  if you're not sure the city is known.

`config.h` is in `.gitignore`, so your keys are never committed. Don't
remove it from there. The build fails with `config.h: No such file or
directory` if you skip this step.

## 8. Build and flash

Plug the ESP32 into your computer with the USB cable first.

**PlatformIO (VS Code):**
1. *File → Open Folder* and choose the `HeadsUpDisplay` folder (the one with
   `platformio.ini` in it).
2. Wait for PlatformIO to finish setting up. The first time, it downloads the
   ESP32 toolchain and libraries, which can take several minutes.
3. In the blue status bar at the bottom, click **✓ (Build)**. It should end
   with `SUCCESS`.
4. Click **→ (Upload)**. PlatformIO finds the board's port by itself.
5. Click the **🔌 (Serial Monitor)** icon to watch the board start up.

Or, from a PlatformIO terminal:

```bash
pio run                # build
pio run -t upload      # build and flash
pio device monitor     # serial output (115200 baud)
```

**Arduino IDE:**
1. *File → Open* → `HeadsUpDisplay.ino`. The other files open as tabs.
2. *Tools → Board* → **ESP32 Dev Module**, and *Tools → Port* → your board's
   COM port (the one that appears when you plug it in).
3. Click **✓ Verify**, then **→ Upload**.
4. *Tools → Serial Monitor*, set to **115200** baud.

A warning that LiquidCrystal I2C "may be incompatible with your current
board" is harmless; ignore it.

**If the upload fails:**
- "Failed to connect to ESP32" / stuck on `Connecting....`: hold the board's
  **BOOT** button until the upload starts.
- Fails halfway through: lower `upload_speed` in `platformio.ini` to `115200`
  (Arduino IDE: *Tools → Upload Speed*).
- "Sketch too big": Arduino IDE *Tools → Partition Scheme* → **Huge APP
  (3MB No OTA)**. In PlatformIO, add `board_build.partitions = huge_app.csv`
  to `platformio.ini`.

## 9. First boot

1. The LCD shows `WiFi setup...`.
2. On your phone, join the WiFi network **ESP32-Setup**. A setup page should
   open on its own (if not, browse to `192.168.4.1`).
3. Pick your home WiFi and enter its password. The ESP32 remembers it from
   then on.
4. The LCD shows `WiFi connected!`, then the weather screen.

The ESP32 only supports **2.4 GHz** WiFi.

**Check it worked.** In the Serial Monitor you should see something like:

```
WiFi connected!
IP address: 192.168.1.42
Web server started.
HTTP code: 200
Temperature: 14.30
Description: light rain
Calendar HTTP code: 200
Calendar payload: [{"time":"14:00","title":"Dentist"}, ...]
Calendar events loaded: 3
```

(mixed in with `*wm:` lines from WiFiManager and `[btn]` lines from the
button debugging, which you can switch off, see section 11).

If either HTTP code isn't `200`, see [Troubleshooting](#12-troubleshooting).
Note the IP address: you need it for the city web page.

**Moving to a different WiFi network:** if the saved network can't be found
at boot, the `ESP32-Setup` portal opens again automatically. To wipe the saved
WiFi on purpose, erase the flash (`pio run -t erase`, or in the Arduino IDE
enable *Tools → Erase All Flash Before Sketch Upload*) and upload again.

**Running it permanently:** after setup it no longer needs the computer. Power
it from any USB phone charger.

## 10. Using it

- **Button**: switch between the weather and calendar screens.
- **Potentiometer**: turn it to change the scroll speed of long lines. A
  speed bar appears for a moment while you turn it.
- **Calendar screen**: shows 2 events at a time and pages through all 4.
  Long titles scroll fully before the page changes.
- **Change the city**: the board's IP address is printed in the Serial
  Monitor at boot. Open `http://<that IP>` from any device on the same WiFi
  and enter a new city as `City,CC` (for example `Paris,FR`). This lasts
  until the next reboot; to change it permanently, edit `DEFAULT_CITY` in
  `config.h`.

## 11. Customising

| What | Where |
|---|---|
| Time between automatic screen switches (2 min) | `SCREEN_ROTATE_MS` in `HeadsUpDisplay.ino` |
| Weather / calendar refresh interval (5 min) | `WEATHER_REFRESH_MS`, `CALENDAR_REFRESH_MS` in `HeadsUpDisplay.ino` |
| Button and pot pins | `BUTTON_PIN`, `POT_PIN` in `HeadsUpDisplay.ino` |
| Scroll speed range | `SCROLL_STEP_MIN_MS`, `SCROLL_STEP_MAX_MS` in `display.h` |
| LCD I²C address and size | `LiquidCrystal_I2C lcd(0x27, 16, 2)` in `display.h` |
| How far ahead to look for events (7 days) | `LOOKAHEAD_DAYS` in `CalendarProxy.gs` (redeploy after) |
| Number of events | `MAX_EVENTS` in `CalendarProxy.gs` **and** `MAX_CALENDAR_EVENTS` in `calendar.h`. Keep them equal and redeploy the script. |
| Quieter serial output | set `BUTTON_DEBUG` and `CALENDAR_DEBUG` to `false` |

## 12. Troubleshooting

| Symptom | Likely cause / fix |
|---|---|
| Backlight on but no text, or a row of solid blocks | Turn the small blue contrast trimmer on the back of the LCD backpack. |
| Nothing on the LCD at all | Check SDA/SCL aren't swapped. Your LCD may be at `0x3F` instead of `0x27`: run an I²C scanner sketch and update `display.h`. |
| `Weather error / Code: 401` | API key wrong or not activated yet (new keys can take a couple of hours). |
| `Weather error / Code: 404` | City not found. Use the `City,CC` format, e.g. `London,GB`. |
| Calendar shows `No events` and Serial says `unauthorized` | The `?key=` in `CALENDAR_URL` doesn't match `SECRET_KEY` in the script. |
| Calendar HTTP code `403` | The deployment's access isn't set to **Anyone**. |
| Calendar URL shows a Google Drive "file does not exist" page | You deployed as a *Library*. Make a new deployment of type **Web app**. |
| Calendar HTTP code is a negative number | Connection/TLS problem, not an HTTP error. Check the URL starts with `https://` and the WiFi is up. |
| Script changes have no effect | You edited the code but didn't redeploy as a **New version**. |
| Event times are an hour or more off | Set the time zone in the Apps Script project settings. |
| Button does nothing | Watch the `[btn]` lines in the Serial Monitor. The pin should read `1` when released and `0` when pressed. |
| Pot does nothing or jumps around | Make sure the wiper (middle leg) is on GPIO33 and the outer legs are on 3V3 and GND. |
| Board never shows up as a COM port | Try another USB cable (many are charge-only) and install the CP210x / CH340 driver. |

## Security notes

- Keep `config.h` private. If your keys ever end up somewhere public, make a
  new OpenWeatherMap key and change `SECRET_KEY` in the script (then redeploy
  as a new version and update `config.h`).
- The calendar fetch uses `setInsecure()`, so it doesn't check Google's TLS
  certificate. That's fine for reading your own event titles on a home
  network, but don't reuse this pattern for anything sensitive.
- The city web page has no password. Anyone on your WiFi can change the city.

## Project layout

```
HeadsUpDisplay.ino   main loop: screen rotation, button, potentiometer, refresh timers
display.h            LCD setup, drawing helpers and line scrolling
weather.h            OpenWeatherMap fetch and parsing
calendar.h           fetch from the Apps Script proxy, calendar paging
configpage.h         small web server for changing the city
CalendarProxy.gs     Google Apps Script that serves upcoming events as JSON
config.example.h     template for your private config.h
platformio.ini       PlatformIO build config
```
