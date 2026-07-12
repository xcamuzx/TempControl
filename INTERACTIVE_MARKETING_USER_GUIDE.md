# TempControl Interactive Marketing User Guide

This guide is for operators running the Newy Ice Baths interactive activation
with the M5Stack Core2, SEN0385 temperature/humidity sensor, Duinotech NeoPixel
ring, SD-card login capture, and participant photo-proof page.

## Experience goal

Give each participant a short, guided cold-plunge moment that produces a
shareable proof image:

- Live bath temperature and humidity.
- A 3-minute breathing countdown with LED ring guidance.
- Participant name, week number, and badges.
- Date/time and optional GPS location.
- A downloaded PNG for the participant.
- A CSV login record saved on the Core2 SD card.

## What the participant sees

1. They connect to the Core2 Wi-Fi.
2. They open the interactive page.
3. They start the LED breathing timer.
4. They follow the ring:
   - light green: inhale for 4 seconds
   - amber: hold for 4 seconds
   - red: exhale for 4 seconds
5. They take or upload a photo.
6. They approve GPS if they want the location stamped on the image.
7. They save the image and receive a branded proof PNG.

## Equipment checklist

- M5Stack Core2 flashed with the TempControl firmware.
- DFRobot SEN0385 temperature/humidity sensor wired to Core2 Port A.
- Duinotech circular NeoPixel / WS2812 LED ring wired to GPIO27, 5V, and GND.
- FAT32 microSD card inserted in the Core2.
- Phone, tablet, or laptop for opening the Core2 web page.
- Optional: 5V external LED supply, 330-470 ohm data resistor, capacitor, and
  level shifter for the LED ring.

## Start-of-event setup

1. Power on the Core2.
2. Confirm the serial monitor or screen-side status shows:
   - Wi-Fi AP running
   - sensor detected
   - SD ready
3. On the participant phone/tablet/laptop, join:
   - Wi-Fi: `TempControl-Core2`
   - Password: `tempcontrol`
4. Open:

```text
http://192.168.4.1/
```

5. Confirm the page shows live temperature/humidity.
6. Tap `Start 3 Min` once to test the LED ring.
7. Confirm the ring performs:
   - 3 white start blinks
   - countdown ring progress
   - green/amber/red breathing phases
   - 3 red finish blinks

## Operator talk track

Use a short repeatable script:

> "You are about to do a 3-minute cold challenge. Follow the light ring:
> green means inhale, amber means hold, red means exhale. When the timer
> finishes, the ring will blink red three times. Afterward, we will create your
> proof image with your name, week, badges, temperature, time, and optional GPS."

After the challenge:

> "Enter your name, add your week number and badges, take a photo, approve GPS
> if you want the location printed, then tap Save Picture + Log. Your image will
> download to your device."

## Badge ideas

Badges are free text. Keep them short so they fit cleanly on the image.

Examples:

- First Plunge
- 3 Minute Club
- Breath Control
- Cold Warrior
- Week Streak
- Team Challenge
- Mindset Reset
- Newy Ice Baths

## Capturing the photo proof

1. Enter `Name`.
2. Enter `Week`, for example:
   - `001/999`
   - `012/999`
   - `104/999`
3. Enter one or more `Badges`.
4. Tap the picture input and take or choose a photo.
5. Tap `Request GPS`.
6. Approve the browser location prompt.
7. Tap `Save Picture + Log`.

The browser downloads a PNG with the metadata printed onto the image.

## What is saved to SD

Each successful `Save Picture + Log` appends one row to:

```text
/logins.csv
```

The CSV stores:

- Core2 uptime in milliseconds
- capture timestamp
- name
- week
- week total
- badges
- temperature
- humidity
- location text
- latitude
- longitude

The photo itself is downloaded by the browser to the participant device. The
Core2 stores the login metadata, not the image file.

## Retrieving event data

1. Power down the Core2.
2. Remove the microSD card.
3. Insert it into a computer.
4. Copy:

```text
logins.csv
```

5. Back up the file before the next event.

## Privacy and consent

Before capturing a proof image, tell participants:

- The image is saved to their device by their browser.
- GPS is optional.
- If GPS is approved, location text is printed on the image and saved in
  `logins.csv`.
- If GPS is denied or blocked by the browser, the image and CSV record location
  as unavailable.

## Troubleshooting

### The phone cannot open the page

- Confirm it is connected to `TempControl-Core2`.
- Open `http://192.168.4.1/` directly.
- Disable mobile data temporarily if the phone keeps routing away from the Core2
  local network.

### GPS does not appear

- Confirm the browser asked for location permission.
- Try another browser.
- Some browsers restrict GPS on local HTTP pages. The capture can still proceed;
  location will be recorded as unavailable.

### The LED ring does not light

- Confirm ring `DI` goes to Core2 `GPIO27`.
- Confirm ring `V+` has 5V and `GND` shares Core2 ground.
- Confirm the ring direction uses `DI`, not `DO`.
- If powered from 5V, use a proper 3.3V-to-5V data level shifter for best
  reliability.

### The SD log does not save

- Confirm a FAT32 microSD card is inserted before powering the Core2.
- Confirm the firmware reports SD ready.
- Try a different SD card if logging still fails.

## End-of-event reset

1. Save and back up `logins.csv`.
2. Clear or archive the SD card before the next event.
3. Inspect sensor and LED wiring.
4. Power-cycle the Core2 and run one test capture.
