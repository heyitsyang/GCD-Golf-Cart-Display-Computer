# GCS User Manual for GCD Ver 1.0.2

The Golf Cart Computer System is a smart display and telemetry platform for golf carts. It provides real-time speed, heading, fuel level, temperature, and GPS-synchronized time, along with mesh radio messaging, weather, and entertainment data.

The system has three components:

- **GCD** (Golf Cart Display) — the touchscreen display computer mounted in the cart. This is the primary user interface and the subject of this manual.
- **GCI** (Golf Cart Internal) — an internal computer installed inside the cart. It reads sensors (temperature, battery voltage, fuel level) and controls the headlight relay, sending telemetry data to the GCD wirelessly.
- **GCM** (Golf Cart Mesh) — a Meshtastic LoRa radio module co-located with the GCD. It connects to the long-range mesh radio network, enabling messaging between carts, weather broadcasts, and entertainment schedule data.

All three units are wired together via RJ cables that carry power and signals. The GCM is always connected to the GCD; the GCI requires a one-time software pairing step described in [System Setup](#system-configuration-first-time). When GCI is installed, the cart's ignition switch controls GCD power-down (deep sleep). Without GCI, the GCD remains running whenever power is supplied and draws more battery between uses.

An optional fourth component, the **Mailbox Sensor**, mounts inside a postal mailbox and lights a mail glyph on the Home screen when mail is delivered. It communicates over the mesh radio and requires no wiring to the cart. See the *GCS Mailbox Sensor Manual* for the full procedure.

---

## System Connections

<img src="Assembly Images/System Connection Diagram.png" width="480" alt="System Connection Diagram — GCM, GCD, and GCI connected via RJ cables">

Connect the three units before first use:

1. Plug one RJ 6P6C cable from the GCD **To Mesh Radio** jack to the matching jack on the GCM enclosure. This cable carries +5V power, GPS signal, and mesh communications.
2. Plug a second RJ 6P6C cable from the GCD **To GCI** jack to the matching jack on the GCI enclosure. This cable carries +5V power and the ignition status signal.
3. GCI telemetry (fuel level, battery voltage, temperature, and headlight status) travels over **ESP-NOW** — a short-range WiFi protocol — with no additional cable. GCI must be paired with the GCD once before telemetry begins (see [System Setup](#system-configuration-first-time)).

> Both RJ cables are keyed and only fit one way. Do not force.

---

## Splash Screen

<img src="GCD%20Screens/Splash.jpg" width="213" alt="Splash Screen">

The splash screen appears on every boot and disappears automatically after a few seconds. The center displays the Hands-On Tech logo.

Four identifiers are shown at the bottom in two columns:

- **GCD** (bottom-left, e.g., `v1.0.1+1`) — GCD firmware version. Useful when reporting issues.
- **GCI** (bottom-left, e.g., `v1.0.1+10`) — GCI firmware version. Shows the last-known version reported by the paired GCI; blank if no GCI has connected yet.
- **GCD MAC** (bottom-right) — the display computer's WiFi hardware address, used internally for GCI pairing. No user action required.
- **GCM Node** (bottom-right, e.g., `!aabbccdd`) — this is the cart's mesh radio address. Share this with other cart owners so they can send you direct messages.

---

## Home Screen

<img src="GCD%20Screens/Home.jpg" width="213" alt="Home Screen">

The home screen is the main at-a-glance view. Swipe left or right anywhere on the screen (outside the bottom icon bar) to open the Menu.

### Corner Indicators

- **Top-left — HEADING**: Compass direction of travel (N, NE, E, SE, S, SW, W, NW). Derived from GPS track, so it reflects the direction you are moving, not which way the cart is pointed.
- **Top-right — MPH**: Current speed from GPS.

### Center Clock and Date

The large clock is GPS-synchronized and updates continuously. If GPS signal is lost or stale for more than 60 seconds, the clock area shows **NO GPS** as a warning. Once signal is restored, the clock resumes normal display.

### Fuel Bar

A horizontal bar below the date represents fuel level as a percentage (full bar = 100%). The bar is only shown when a fuel sensor is configured. When fuel drops below the low-fuel threshold (set on the Vehicle screen), a yellow fuel pump icon (or battery icon for electric sensor type) appears in the bottom bar.

### Bottom Icon Bar

Five icons run along the bottom of the screen:

| Icon | Meaning | Touch action |
|------|---------|------------|
| Message bubble with number | Unread direct messages count | Opens Messages screen |
| Yellow envelope | Mail is waiting in your mailbox (requires the optional Mailbox Sensor) | Long-press to snooze |
| Green headlight symbol | Auto-headlights are currently ON | None (display only) |
| Yellow golf cart | Service is due (Hrs Since Service exceeds interval) | Opens Vehicle screen |
| Yellow fuel pump or battery | Fuel is below the low-fuel threshold; battery icon shown when Fuel Sensor Type is ADC ELECTRIC | None (display only) |

The envelope, headlight icon, golf cart icon, and fuel/battery icon are only visible when their respective conditions are active. The message badge number counts only unread **direct messages** (not channel messages).

### Mail Glyph

The yellow envelope appears with a chime when the optional Mailbox Sensor reports that mail has been delivered, and disappears on its own when the mail is collected.

**Press and hold the envelope for about a second to snooze it.** A click confirms. The snooze is temporary — because the mail is still there, the envelope returns at the sensor's next hourly report, with a chime. A short tap does nothing; the hold is required so that a stray touch cannot dismiss the reminder.

The glyph is dark after every power-up until the sensor's next report, which may take up to an hour.

For pairing, sensor health, and the full behaviour, see the *GCS Mailbox Sensor Manual*.

---

## Menu

<img src="GCD%20Screens/Menu.jpg" width="213" alt="Menu Screen">

The Menu provides access to all secondary screens. Tap the back arrow (top-left) to return to the Home screen.

| Icon | Destination |
|------|-------------|
| Cloud / sun | [Weather](#weather) |
| Theater masks | [Now Playing / Entertainment](#now-playing--entertainment) |
| Meshtastic symbol | [Messages](#messages) |
| Golf cart | [Vehicle](#vehicle) |
| Gear | [Settings](#settings) |

---

## Settings

<img src="GCD%20Screens/Settings.jpg" width="213" alt="Settings Screen">

Settings contains the GPS status readout and the display and audio controls. Tap the right arrow (top-right) to continue to Settings 2.

### GPS Status Row (satellite icon) *(requires GPS fix)*

*Read-only.* This row is hidden until the GCD acquires a GPS position fix, and appears automatically once one is available.

- **Satellites / HDOP** (e.g., `5/7.5`) — number of satellites in use and horizontal dilution of precision. HDOP below 2.0 indicates good positional accuracy; above 5.0 is poor.
- **Latitude / Longitude** — current GPS coordinates for reference.

### Day Backlight (sun icon) — range 1–10
Display brightness during daytime hours. The system automatically switches between day and night backlight based on calculated sunrise and sunset times derived from your GPS position.

### Night Backlight (moon icon) — range 1–10
Display brightness after sunset. Setting this lower than Day Backlight reduces glare when driving at night. The transition is automatic.

### Backlight Timeout — range 0–30 minutes

*(Applies when GCI is not installed.)* The screen dims and turns off after this many minutes of no touch input. Set to **0** to keep the screen on indefinitely. The screen wakes immediately on touch. When GCI is installed, the cart's ignition-off signal manages screen power automatically and this setting is not used.

### Volume (speaker icon) — range 1–10
Controls the volume of alert tones and notification sounds from the built-in speaker.

---

## Settings 2

<img src="GCD%20Screens/Settings2.jpg" width="213" alt="Settings 2 Screen">

Settings 2 contains the two pairing controls, plus display orientation, sensor, and system management settings. Navigate here using the right arrow on the Settings screen.

The two pairing rows at the top share the same layout and the same gestures: a blue button on the left, and a colour-coded status value on the right.

| Gesture | PAIR GCI | PAIR MBX |
|---|---|---|
| **Short tap** | Start pairing with the GCI | Accept a waiting mailbox offer |
| **Long press** (about a second) | Unpair the GCI | Unpair the mailbox sensor |

### GCI MAC / PAIR GCI

The value on the right shows the hardware address of the paired Golf Cart Internal computer, colour-coded:

| Value | Colour | Meaning |
|---|---|---|
| `NONE` | white | No GCI paired. **Not a fault** — the GCI is optional |
| `AA:BB:CC:DD:EE:FF` | green | Paired and responding |
| `AA:BB:CC:DD:EE:FF` | red | Paired, but no longer answering — check GCI power |

- **Short tap PAIR GCI** to start the wireless pairing process. See [System Setup](#system-configuration-first-time) for step-by-step instructions.
- **Long press PAIR GCI** to unpair. The value returns to a white `NONE` and telemetry stops. Do this before pairing a replacement GCI.

### Mailbox ID / PAIR MBX

*(Requires the optional Mailbox Sensor.)* Shows which mailbox sensor this cart is watching and how healthy the link to it is.

| Value | Colour | Meaning |
|---|---|---|
| `NO MAILBOX` | white | No sensor paired |
| `a1b2c3d4 OFFERED` | white | A sensor is offering to pair — tap to claim it. The button also turns green |
| `a1b2c3d4` | white | Paired, nothing heard yet since power-up. Normal; may take up to an hour |
| `a1b2c3d4` | green | Paired and healthy |
| `a1b2c3d4 (3)` | yellow | Paired, but 3 reports went missing. A small, stable count is normal on mesh radio |
| `a1b2c3d4 SILENT` | red | Nothing heard for 24 hours of listening — likely a dead battery |

- **Short tap PAIR MBX** to accept a waiting offer. A chime confirms.
- **Long press PAIR MBX** to unpair. The value returns to `NO MAILBOX` and the mail glyph stops appearing.

To pair, open this screen **first**, then press the pair button on the sensor — the offer is only claimable for 45 seconds. The full procedure, the meaning of the yellow count, and troubleshooting are in the *GCS Mailbox Sensor Manual*.

> On carts fitted with a GCI, the red `SILENT` state may never appear, because the display sleeps with the ignition and the 24-hour listening timer restarts on each wake. This is intended behaviour.

### Temp Sensor Adj — range –10 to +10 °F

A calibration offset added to the temperature reading reported by the GCI. If your temperature sensor consistently reads 3°F too high, set this to –3. Useful for compensating sensor placement (e.g., in a hot enclosure).

### Home Location (house icon) *(requires GPS fix)*

The Home Location controls are hidden until the GCD acquires a GPS position fix. Once a fix is available, they appear automatically. The GPS status readout itself is on the [Settings](#settings) screen.

- **HOME OFF / HOME ON toggle** — tap to set your current GPS position as the "Home" location. Set this while parked at your garage or starting point.
- **Fence slider** (100–1000 m) — the radius around the home point that counts as "at home."
- When GCI is not installed, being at home slows the GCM GPS polling rate (from 8 seconds to 2 minutes) to conserve power.
- To clear a home location, toggle the switch back to OFF.

### Flip Screen

Rotates the entire display 180°. Use this if your GCD unit is physically mounted upside-down. Touch calibration adjusts automatically — no recalibration needed after flipping.

### Fuel Sensor Type

Tap the button to cycle through the available fuel sensor types. The button label shows the currently selected type. This setting is automatically transmitted to the GCI so it reads the correct hardware.

| Option | Use case |
|--------|----------|
| **NO FUEL SENSOR** | No sensor installed; fuel bar and low-fuel alerts are disabled |
| **ADC GAS** | Analog float sensor (e.g., Yamaha gas gauge) wired to GCI's ADC input |
| **GPIO EXPANDER** | Binary level switches (25/50/75%) via I²C expander on GCI |
| **ADC ELECTRIC** | LiFePO4 electric vehicle approximate battery state of charge using ADC input|

### GCM Serial

Enables or disables serial communication between the GCD and the GCM radio module. **Must be ON** for the Messages, Weather, and Now Playing screens to receive data.

Turn this **OFF** when you need to configure the GCM via Bluetooth (e.g., using the Meshtastic phone app). The GCM can only communicate on one channel at a time — serial or Bluetooth — so the GCD serial link must be disabled before the GCM will accept a Bluetooth connection. When finished, return to Settings 2 and toggle **GCM SERIAL** back ON; it does not re-enable automatically except on boot.

### Reset Preferences

**RESET PREFS** wipes all saved settings back to factory defaults and restarts the GCD. **This cannot be undone.** Everything below is erased:

- **Touchscreen calibration** — see the warning below
- **Paired devices** — both the GCI and the Mailbox Sensor
- **Home location** and fence radius
- **Odometer, trip, and service hour counters**
- All sliders and toggles: backlights, volume, timeout, temperature offset, fuel sensor type, low-fuel threshold, service interval, LUX thresholds, flip screen, message filter
- Cached weather and entertainment data, and saved unread direct messages

> ### ⚠️ RESET PREFS erases the touchscreen calibration
>
> Touch calibration is stored alongside the other preferences and is wiped with them. **Recovering it is not something you can do from the touchscreen** — it requires a computer, flashing a separate calibration program onto the GCD, running through the calibration targets, and then flashing the main firmware back. See the *GCS Software Installation Manual*, section 3.
>
> The display remains usable in the meantime, but touch accuracy falls back to a rough automatic estimate that improves only as you touch the far edges of the screen. Expect taps to land noticeably off-target until the unit is recalibrated.
>
> **Do not use RESET PREFS as a troubleshooting step.** To clear a single paired device, long-press **PAIR GCI** or **PAIR MBX** instead — those affect only that pairing.

---

## Vehicle

<img src="GCD%20Screens/Vehicle.jpg" width="213" alt="Vehicle Screen">

The Vehicle screen manages maintenance counters, service reminders, and sensor thresholds. Distance and hour counters accumulate automatically while the cart is in use.

### Odometer

Displays total accumulated distance in miles. GPS speed is used to calculate distance, so no separate sensor is needed.

- **EDIT** — opens a numeric entry screen to manually set the odometer to a specific value. Useful when commissioning a new GCD on an existing cart to carry over the prior odometer reading.
- **RESET** — zeros the odometer. This cannot be undone.

### Trip

Distance traveled since the last trip reset.

- **RESET** — zeros the trip counter. Reset this at the start of each outing to track the current ride.

### Hrs Since Service

Operating hours accumulated since the last service reset, tracked in 6-minute increments.

- **RESET** — zeros the service hour counter. Reset this after each maintenance service.

### Svc Interval Hrs — range 50–150 hours

When **Hrs Since Service** exceeds this value, a service reminder alert activates on the Home screen. Adjust this to match your preferred service interval.

### Low Fuel Warn % — range 0–50%

The fuel percentage at which the low-fuel alert triggers on the Home screen. Has no effect if the Fuel Sensor Type is set to NO FUEL SENSOR. Set higher (e.g., 25%) if you want earlier warnings; set lower (e.g., 10%) for later warnings.

### LUX Auto-Headlight Thresholds

Controls the automatic headlight system. Requires a BH1750 light sensor connected to the GCI.

- **LUX Now** — displays the current ambient light reading from the sensor in real time.
- **On** (left slider handle) — headlights turn ON when the light level drops *below* this value.
- **Off** (right slider handle) — headlights turn OFF when the light level rises *above* this value.

The gap between the On and Off values creates **hysteresis** — a deliberate dead zone that prevents the headlights from rapidly cycling on and off at the boundary (common at dusk or when driving in and out of shade). For example: On at 1000 lux, Off at 2000 lux means headlights engage when entering a shaded area but only disengage in bright direct sunlight.

---

## Messages

<img src="GCD%20Screens/Messages.jpg" width="213" alt="Messages Screen">

The Messages screen displays incoming and outgoing mesh radio messages. All communication goes through the GCM radio module; **GCM Serial must be ON** in Settings 2.

### Message List

Each row displays one message with two sections:

- **Left column**: channel number and time (12-hour format) on the top line; sender's node short ID (e.g., `!0e8c`) on the bottom line.
- **Right column**: the message text.

**Unread direct messages** are highlighted in a bright color to distinguish them from read messages and channel messages.

Tapping a row selects it. If the message text is longer than the display width, it will scroll as a marquee after selection.

### Row Touch Interactions

The left and right zones of each row respond differently:

- **Tap the left zone** (the column showing channel/time/sender) — toggles that sender as a **Favorite**. Yellow text indicates a favorited node. Tap again to un-favorite.
- **Long-press the right zone** (the message text area) — opens the Canned Messages screen in **reply mode**, pre-addressed to that sender.

### Filter Dropdown (bottom-left)

Narrows which messages are shown in the list. Tap the dropdown to cycle through:

| Filter | Shows |
|--------|-------|
| ALL | Every message except HOT packet data broadcasts |
| DIRECT MSGS | Only messages sent directly to you or by you to a specific person |
| CHANNEL 0 | Only messages on channel 0 (HOT packets excluded) |
| CHANNEL 1 | Only messages on channel 1 (HOT packets excluded) |
| CHANNEL 2 | Only messages on channel 2 (HOT packets excluded) |
| DEBUG | All traffic including raw HOT packet data broadcasts |

The selected filter is saved and persists across reboots.

**HOT packets** are automated data broadcasts sent over the mesh by the weather and entertainment servers, and by mailbox sensors. They carry the content shown on the Weather and Now Playing screens, and the mail glyph on the Home screen, and are not intended for reading as messages. ALL and the channel filters suppress them so they do not clutter the message list. Switch to DEBUG only when troubleshooting the data feed — mailbox rows appear there as `<MBX a1b2c3d4 P>`.

**How the filter works — inbound and display:**

The filter does two things simultaneously:

1. **Display** — only messages matching the active filter appear in the list. Messages already in the buffer that do not match are hidden, not deleted. If you switch back to a wider filter (e.g., ALL), those hidden messages reappear.

2. **Inbound storage** — incoming messages that do not match the active filter are discarded before they enter the 32-message buffer. They cannot be recovered by switching the filter later.

Two exceptions always bypass the inbound filter regardless of the active setting: **direct messages to you** are always stored, and **your own outgoing messages** are always stored.

**Practical implication:** if you leave the filter set to CHANNEL 0 overnight, any channel 1 or channel 2 messages that arrive while the cart is powered will be permanently lost. Set the filter to ALL if you want to receive everything, and narrow it only when actively reading a specific channel or direct message.

### NEW MSG Button

Opens the Canned Messages screen in compose mode, ready to send a new message without replying to anyone specific.

### Message Retention

The GCD holds a maximum of **32 messages** in memory. When the buffer is full, the oldest message is discarded to make room for new ones.

All messages are lost when the cart powers down, **with one exception**: up to **4 unread direct messages** are saved to flash storage and automatically restored when the cart powers back on. A single beep on the Home screen confirms that saved DMs have been restored.

Weather data and entertainment schedule broadcasts do not appear in the message list and are not subject to this 32-message limit.

---

## Canned Messages (Compose & Reply)

<img src="GCD%20Screens/Canned%20Messages.jpg" width="213" alt="Canned Messages Screen">

The Canned Messages screen is used both to compose new messages and to reply to received ones. Tapping any of the 8 message buttons immediately sends the selected message and returns to the Messages screen.

### Channel Selector (top-left)

Tap to cycle through the available channels (e.g., `Ch 0: GolfCart`). Channel names are configured on the GCM radio and displayed here automatically once the GCM is connected.

### Recipient Selector (top-right)

Selects who receives the message. Tap to cycle through:

- Your **Favorites** (up to 8, shown by node short name)
- A temporary non-favorite node, if you arrived here via a reply to someone not in your favorites list (shown at the top of the list)
- **BROADCAST** — sends to all nodes listening on the selected channel

### Context Strip (reply mode only)

When you arrive via a long-press reply from the Messages screen, a strip in the middle of the screen shows the message you are responding to. This confirms the sender and content you are replying to.

### Understanding Channels, Direct Messages, and Broadcast

These are three distinct ways to send a message on the mesh:

**Channel message** — sent to a named channel (e.g., Ch 0: GolfCart). Every node on the Meshtastic network that is subscribed to that channel will receive it. This is the default when BROADCAST is selected as the recipient.

**Direct Message (DM)** — sent to a specific node ID (selected from Favorites or the temporary recipient). Only that node receives it. DMs appear highlighted in the message list and their unread count shows as a badge on the Home screen.

**Broadcast** — selecting BROADCAST in the recipient dropdown is equivalent to a channel message: it goes to all listeners on the selected channel.

### 8 Canned Message Buttons

The default messages are: **OK**, **Yes**, **No**, **On my way**, **Running late**, **Arrived**, **Stand by**, **Phone me**.

Tap any button to send immediately.

---

## Favorites

Favorites are a shortlist of up to 8 frequently messaged nodes. They appear as recipient options in the Canned Messages screen, making it fast to send to regular contacts without scrolling through all known nodes.

**Adding a favorite**: In the Messages screen, tap the **left zone** of any received message row. The sender's node ID text turns yellow, confirming they have been added.

**Removing a favorite**: Tap the left zone of a row from a favorited sender (yellow text). The text returns to white and the node is removed.

Favorites are stored in flash memory and survive power cycles. Node short names (the 4-character display names like `!0e8c`) update automatically if a newer name broadcast is received from that node over the mesh.

---

## Weather

<img src="GCD%20Screens/Weather.jpg" width="213" alt="Weather Screen">

The Weather screen displays forecast data received over the Meshtastic mesh as a broadcast from a weather station or data server on the network. No configuration is required — data appears automatically when a compatible broadcast is received.

The screen shows:

- **Date and time** the data was received (top of screen)
- **Current temperature** (large, center) — shows the live reading from the DS18B20 sensor on the GCI if installed; otherwise shows the temperature from the last weather broadcast
- **Sunrise** (left) and **sunset** (right) times with icons
- **Hourly forecast** — four time slots, each showing the hour, a weather condition icon (sun, cloud, rain), temperature, and precipitation amount (in inches, if non-zero)

If no weather broadcast has been received since the last boot, the screen will show the last cached data (which persists across reboots) or be empty if no data has ever been received.

---

## Now Playing / Entertainment

<img src="GCD%20Screens/Now%20Playing.jpg" width="213" alt="Now Playing Screen">

The Now Playing screen displays a live entertainment schedule received over the mesh network. Like the Weather screen, it requires no configuration — data appears automatically when a compatible broadcast is received from a schedule server on the mesh.

The screen shows:

- **Date and time** the data was received (top)
- A table of **Venue | Performer** rows listing current entertainment at multiple venues

Data is cached in flash storage and survives reboots, so the last received schedule remains visible even after the cart is powered down and back up.

---

## System Configuration (First-Time)

These steps are performed once when the system is first installed, or any time a component is replaced. All steps are done through the GCD touchscreen; no computer or special tools are required.

> **Pre-installation:** Before flashing the GCD firmware, it is recommended to run the touchscreen calibration program first. This ensures accurate touch response before the main program is installed.  See the *GCS Software Installation Manual* for further details.

**Before starting**, ensure all RJ cable connections are made:
- GCD to GCM (mesh radio) — carries serial data, GPS signal, and power
- GCD to GCI (internal computer) — carries power and the ignition signal wire

### Step 1: Enable GCM Serial (connect GCM)

1. From the Home screen, swipe left or right to open Menu.
2. Tap the gear icon → Settings → right arrow → Settings 2.
3. Toggle **GCM SERIAL** to ON.
4. The GCD connects to the GCM radio within a few seconds.
5. Power-cycle the GCD. On the next Splash screen, the **GCM Node** ID will appear (e.g., `!aabbccdd`). Note this ID and mark it on the GCM enclosure — other users will need it to send you direct messages.

### Step 2: Pair GCI (connect telemetry)

The GCI and GCD must be paired once so they recognize each other and can exchange telemetry.

Before starting, check the GCI's small display. If it shows **WAITING FOR PAIRING...**, it is ready.

1. On the GCI enclosure, hold down the display button until the screen prompts for confirmation, then press the button again to confirm. The GCI is now in pairing mode.
2. On GCD: navigate to Settings 2 → tap **PAIR GCI** within 30 seconds.
3. On success:
   - The GCI's MAC address fills in on the Settings 2 screen, in green.
   - The GCI's small display shows the paired GCD's MAC address.
   - Temperature, battery, and fuel data begin appearing on the Home screen.

Pairing is permanent and survives reboots. Repeat this step only if the GCI unit is replaced — long-press **PAIR GCI** to unpair the old unit first.

### Step 3: Configure Fuel Sensor

1. In Settings 2, tap the **Fuel Sensor Type** button to cycle through options.
2. Select the option matching your GCI installation (NO FUEL SENSOR if none is installed).
3. The setting is transmitted to the GCI automatically. The fuel bar on the Home screen will appear or disappear depending on your selection.
4. On the Vehicle screen, set the low fuel/low batery warning to the desired warning percentage.

### Step 4: Set Home Location

1. Drive or park the cart at your home/garage position.
2. Open Settings and check the GPS status row. It appears automatically once the GCD has a position fix; if it is not visible yet, wait for the satellite count to appear (4 or more satellites is ideal).
3. Continue to Settings 2 and tap **HOME OFF** to toggle it to **HOME ON**.
4. Adjust the **Fence** slider to set the radius (in meters) that counts as "at home."

### Step 5: Set Auto-Headlight Thresholds (optional, requires BH1750 sensor on GCI)

1. Navigate to Menu → Vehicle.
2. Note the **LUX Now** reading under typical daytime and shaded conditions to understand your ambient light range.
3. Adjust the **LUX dual slider**:
   - Move the **left handle (On)** to the lux value where you want headlights to turn on (lower = darker conditions).
   - Move the **right handle (Off)** to the lux value where you want them to turn off (higher = brighter conditions).
4. Keep a gap of at least 200–500 lux between the On and Off values to prevent rapid cycling.

### Step 6: Pair the Mailbox Sensor (optional)

Only needed if you have the optional Mailbox Sensor. The sensor must already be programmed and configured on the `GolfCart` primary channel — see the *GCS Mailbox Sensor Manual* for those steps.

**Pair the sensor before you mount it in the mailbox.** The sensor detaches easily and the display does not, so bring the sensor to the cart rather than working a touchscreen and a button that are metres apart against a 45-second clock.

1. Sit in the cart with the sensor **in your hand** and the display powered up.
2. On the GCD, navigate to Settings → right arrow → Settings 2. Do this **before** the next step: the offer expires after 45 seconds.
3. Press the pair button on the sensor.
4. The **PAIR MBX** button turns green and the value beside it reads `a1b2c3d4 OFFERED`.
5. Tap **PAIR MBX** once. A chime confirms and the id is now paired.
6. Take the sensor out to the mailbox and mount it.

The value turns green at the sensor's first report from its mounted position, which may take up to an hour — open and close the mailbox door to force one sooner. That first green also confirms the mailbox is within radio range. The mail glyph then appears on the Home screen whenever mail is delivered.

---

*Golf Cart Computer System — Hands-On Tech*
