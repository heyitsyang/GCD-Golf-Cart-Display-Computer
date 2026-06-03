# GCS Software Installation Manual

Before starting this manual, complete the *GCS Assembly Manual* to physically build and wire all three units (GCM, GCD, GCI).

**Components covered:**
- **GCD** (Golf Cart Display) — ESP32-based CYD touchscreen display
- **GCI** (Golf Cart Internal) — Internal ESP32 cart computer with small display on a custom PC board
- **GCM** (Golf Cart Mesh) — Heltec T114 Meshtastic LoRa radio

**What you will need:**
- USB-A to USB-C cable
- PC with Google Chrome or Microsoft Edge (for the ESPConnect web flasher — no software installation required)
- Smartphone with Bluetooth (for GCM configuration)

**Installation order:**

1. Configure GCM (standalone, via Bluetooth, before wiring to GCD)
2. Flash GCI firmware
3. Flash GCD factory firmware, then run touchscreen calibration
4. Flash GCD main firmware
5. Complete post-installation configuration on the GCD touchscreen

---

## 1. Configure the GCM (do this before wiring to GCD)

The GCM (Heltec T114 running Meshtastic) is configured as a standalone unit via Bluetooth. Power it from USB during this step. Do not connect it to the GCD yet.

> The Meshtastic app UI differs significantly between Android and iPhone. Follow the section for your device.

### 1a. Android

<img src="Installation Images/GCM Android Settings 1.jpg" width="213" alt="Android Settings — Radio and Device configuration">
<img src="Installation Images/GCM Android Settings 2.jpg" width="213" alt="Android Settings — Module configuration">

**Steps:**

1. Install the **Meshtastic** app from Google Play Store.
2. Power the GCM via USB. On your phone, enable Bluetooth and open the Meshtastic app.
3. Tap **+** to add a device and select the Heltec T114 from the list. Pair if prompted.
4. Navigate to **Settings** (gear icon at bottom-right) and apply the following settings:

**Radio Configuration → LoRa**

| Setting | Value |
|---|---|
| Region | United States |
| Use Preset | ON |
| Preset | Long Range — Fast (LongFast) |
| Number of Hops | 3 |
| Frequency Slot | 20 |
| RX Boosted Gain | ON |
| Frequency Override | 906.875 |
| Ok to MQTT | ON |
| Transmit Enabled | ON |

**Radio Configuration → Channels**

The easiest method is to scan the QR code shown in the Meshtastic app at a club meeting — it sets all three channels at once. If configuring manually:

| # | Name | PSK | Uplink |
|---|---|---|---|
| 0 | GolfCart | `handsontechclubtelemeg==` | YES |
| 1 | The Villages | `CQ==` | NO |
| 2 | LongFast | `AQ==` | NO |

Save each channel edit and tap **SEND** to write changes.

**Device Configuration → User**

| Setting | Value |
|---|---|
| Long Name | Your name or cart identifier |
| Short Name | Up to 4 characters (e.g., initials + number: `jd0`) |

**Device Configuration → Device**

| Setting | Value |
|---|---|
| Device Role | CLIENT_MUTE |
| Rebroadcast Mode | ALL |
| Node Info Broadcast Interval | 3 hrs |
| Time Zone | EST5EDT,M3.2.0,M11.1.0 (or enable "Use phone time zone") |

**Device Configuration → Bluetooth**

| Setting | Value |
|---|---|
| Bluetooth Enabled | ON |
| Pairing Mode | FIXED_PIN |
| Fixed PIN | 123456 |

> Use a fixed PIN because the radio may not be accessible to display a random pairing code.

**Module Configuration → Serial**

| Setting | Value |
|---|---|
| Serial Enabled | **ON** |
| Echo Enabled | OFF |
| RX | 9 |
| TX | 10 |
| Baud Rate | BAUD_9600 |
| Timeout | 0 |
| Serial Mode | **PROTO** |
| Override Console Serial Port | OFF |

> The Serial module settings are the critical link between the GCM and GCD. Incorrect values here will prevent the GCD from receiving mesh data.

5. After saving all settings, close the app. Label the GCM enclosure with the last four characters of its Node ID (visible in the app) for future reference.

---

### 1b. iOS (iPhone)

<img src="Installation Images/GCM iPhone Settings 1.png" width="213" alt="iPhone Settings — Radio and Device configuration">
<img src="Installation Images/GCM iPhone Settings 2.png" width="213" alt="iPhone Settings — Module configuration (scroll down to find Serial)">

**Steps:**

1. Install the **Meshtastic** app from the App Store.
2. Power the GCM via USB. Enable Bluetooth and open Meshtastic.
3. Tap **Connect** (link icon at bottom) → tap **+** → select the Heltec T114. Pair if prompted by iOS.
4. Tap **Settings** (gear icon at bottom-right) and apply the following settings:

**Radio Configuration → LoRa**

| Setting | Value |
|---|---|
| Region | United States |
| Use Preset | ON |
| Preset | Long Range — Fast (LongFast) |
| Number of Hops | 3 |
| Frequency Slot | 20 |
| RX Boosted Gain | ON |
| Frequency Override | 906.875 |
| Ok to MQTT | ON |
| Transmit Enabled | ON |

**Radio Configuration → Channels**

The easiest method is to scan the QR code shown in the Meshtastic app at a club meeting — it sets all three channels at once. If configuring manually:

| # | Name | PSK | Uplink |
|---|---|---|---|
| 0 | GolfCart | `handsontechclubtelemeg==` | YES |
| 1 | The Villages | `CQ==` | NO |
| 2 | LongFast | `AQ==` | NO |

**Device Configuration → User**

| Setting | Value |
|---|---|
| Long Name | Your name or cart identifier |
| Short Name | Up to 4 characters (e.g., initials + number: `jd0`) |

**Device Configuration → Device**

| Setting | Value |
|---|---|
| Device Role | CLIENT_MUTE |
| Rebroadcast Mode | ALL |
| Node Info Broadcast Interval | 3 hrs |
| Time Zone | EST5EDT,M3.2.0,M11.1.0 (or enable "Use phone time zone") |

**Device Configuration → Bluetooth**

| Setting | Value |
|---|---|
| Bluetooth Enabled | ON |
| Pairing Mode | FIXED_PIN |
| Fixed PIN | 123456 |

**Module Configuration → Serial**
*(scroll down in Module Configuration to find Serial)*

| Setting | Value |
|---|---|
| Serial Enabled | **ON** |
| Echo Enabled | OFF |
| RX | 9 |
| TX | 10 |
| Baud Rate | BAUD_9600 |
| Timeout | 0 |
| Serial Mode | **PROTO** |
| Override Console Serial Port | OFF |

> The Serial module settings are the critical link between the GCM and GCD. Incorrect values here will prevent the GCD from receiving mesh data.

5. After saving all settings, close the app. Label the GCM enclosure with the last four characters of its Node ID for future reference.

---

## 2. Flash GCI Firmware

**Flashing tool:** Open **Google Chrome** or **Microsoft Edge** and go to:
> `https://thelastoutpostworkshop.github.io/microcontroller_devkit/espconnect/`

No software installation is required. This tool does not work in Firefox or Safari.

**Firmware:** All GCI files can be obtained via browser from the presentation links at `handsontech.org > Small Computers > 2/24/2026`. If installing on the GCI for the first time, download `GCI factory reset firmware`. If updating existing firmware, download `GCI firmware`.

**Steps:**

1. Connect the GCI ESP32 to your computer via USB.
2. In ESPConnect, select the correct COM port.
3. Set the flash address to **`0x00`**.
4. Browse to `gci_fw_combo.bin` and click **Flash**.
5. The combo binary formats memory completely and installs the bootloader and firmware together. This is correct for a first-time install.
6. After flashing, power-cycle the GCI. The small display will show one of the following states:

<img src="Installation Images/GCI waiting for pair.jpg" width="213" alt="GCI display: WAITING FOR PAIRING...">

**WAITING FOR PAIRING...** (yellow) — normal state after first flash. GCI is ready to be paired with a GCD.

<img src="Installation Images/GCI unpaired error.jpg" width="213" alt="GCI display: unpaired error state">

**PR address shown in orange** — GCI was previously paired with a different GCD and that GCD is not present. This is normal if the GCI was used before. It will be re-paired in Section 5.

<img src="Installation Images/GCI paired normal.jpg" width="213" alt="GCI display: paired normal state">

**Paired (normal operating)** — GCI is paired and communicating with its GCD. You will see this after pairing is completed in Section 5.

<img src="Installation Images/GCI reset pairing.jpg" width="213" alt="GCI display: reset pairing confirmation">

**Reset pairing** — shown briefly when the pairing reset button sequence is performed. Not expected during a normal first install.

---

## 3. Touchscreen Calibration

Calibration must be done **before** flashing the main GCD firmware. The calibration program stores touch coefficients in NVS (non-volatile storage). The main firmware reads the same storage, so calibration data survives the subsequent firmware flash.

**Firmware:** All GCD files can be obtained via browser from the presentation links at `handsontech.org > Small Computers > 1/27/2026`. If installing on the GCD for the first time, download both `GCD Touch Calibration firmware` and `GCD factory reset firmware`. If updating existing firmware, skip to `4. Flash GCD Main Firmware`.

### Step 1: Flash the GCD factory combo

1. Connect the GCD (CYD board) to your computer via USB.
2. In ESPConnect, set the flash address to **`0x00`**.
3. Select `gcd_fw_combo.bin` and click **Flash**.
4. This establishes the bootloader and partition table on the fresh device.

### Step 2: Flash the calibration program

1. In ESPConnect, set the flash address to **`0x10000`** (not `0x00`).
2. Select `touch_calibration.bin` and click **Flash**.
3. Power-cycle the GCD.

### Step 3: Run calibration

1. The calibration program boots automatically and displays a crosshair target on screen.
2. Tap the center of each crosshair target as precisely as possible. Six targets will appear across the screen.
3. When complete, the calibration coefficients are saved to NVS automatically.

Proceed immediately to Section 4 to load the main GCD firmware — calibration data is preserved.

> **If touch is still off after the main firmware is installed:** Re-flash `touch_calibration.bin` at `0x10000` and repeat Step 3 at any time. Calibration does not require re-flashing the combo binary.

---

## 4. Flash GCD Main Firmware

**Firmware:** Download `GCD firmware` from `handsontech.org > Small Computers > 1/27/2026`.

1. In ESPConnect, set the flash address to **`0x10000`** (not `0x00`).
2. Select `firmware.bin` and click **Flash**.
3. Power-cycle the GCD. The Splash screen will appear within a few seconds.

> It is normal at this stage for the GCM Node ID to be absent from the Splash screen — the GCM has not been connected yet. The Node ID will appear after the RJ cable is connected and Mesh Serial is enabled in Section 5.

---

## 5. Post-Installation Configuration

All remaining steps are performed using the GCD touchscreen. No computer is required. Refer to the *GCS User Manual* for detailed screen descriptions.

Before starting, ensure both RJ cables are connected:
- GCD ↔ GCM (mesh radio)
- GCD ↔ GCI (internal computer)

### Step 1: Enable Mesh Serial (connect GCM)

1. From the Home screen, swipe left or right to open the Menu.
2. Tap the gear icon → **Settings** → right arrow → **Settings 2**.
3. Toggle **MESH SERIAL** to ON.
4. Tap **REBOOT GCD** (or power-cycle). On the next Splash screen, the **GCM Node ID** will appear (e.g., `!aabbccdd`).
5. Note this ID and mark it on the GCM enclosure — other users will need it to send you direct messages.

### Step 2: Pair GCI

1. On the GCI enclosure, hold the display button until the screen prompts for confirmation, then press the button again to confirm. The GCI enters pairing mode.
2. On the GCD, navigate to **Settings 2** and tap **PAIR GCI** within 30 seconds.
3. On success, the GCI's MAC address fills in on Settings 2, and the GCI display shows the GCD's MAC. Temperature, battery, and fuel data begin appearing on the Home screen.

### Step 3: Configure Fuel Sensor

1. In Settings 2, tap the **Fuel Sensor** dropdown.
2. Select the option matching your installation:

   | Option | Description |
   |---|---|
   | `NO FUEL SENSOR` | No fuel sensor installed. Fuel display is disabled. |
   | `ADC GAS` | For Yamaha gas carts — works without modification. For non-Yamaha carts, the GCI ADC resistor divider network values must be adjusted to match the Yamaha cart's sensor output range. |
   | `GPIO EXPANDER` | For the three-sensor external tank kit available through The Villages Hands-On Tech club. Sensors mount outside the gas tank and report level in ¼-tank increments. |
   | `ADC ELECTRIC` | Calibrated for 48–51.3V LiFePO₄ battery packs. Requires appropriate resistor divider values at the GCI ADC input to use the full 3.3V measurement range. |

3. The setting is transmitted to the GCI automatically.

### Step 4: Set Home Location

1. Park the cart at your home/garage position.
2. Confirm GPS has a fix (check Settings 2 for satellite count ≥ 4).
3. Tap **HOME OFF** to toggle it to **HOME ON**.
4. Adjust the **Fence** slider to set the radius (in meters) that counts as "at home."

### Step 5: Set Auto-Headlight Thresholds (optional)

*Requires BH1750 light sensor installed on the GCI.*

1. Navigate to **Menu → Vehicle**.
2. Note the **LUX Now** reading in typical daytime and shaded conditions.
3. Adjust the dual slider: left handle (On) = lux level to turn headlights on; right handle (Off) = lux level to turn them off.
4. Keep a gap of at least 200–500 lux between the On and Off values to prevent rapid cycling at dusk.

---

## 6. Troubleshooting

| Symptom | Check |
|---|---|
| Splash screen shows no GCM Node ID (before Mesh Serial is enabled) | Normal — GCM Node ID only appears after Mesh Serial is enabled in Settings 2 and GCD is rebooted with GCM connected |
| Splash screen shows no GCM Node ID (after Mesh Serial enabled and rebooted) | Confirm RJ cable between GCD and GCM is seated; confirm GCM is powered; allow up to 30 s for GCM to boot and connect |
| GCM Bluetooth won't connect during configuration | Ensure the GCM is powered via USB and not yet wired to the GCD serial line; configure it standalone |
| GCI PAIR button times out or does nothing | GCI must be in pairing mode first — hold the GCI display button until prompted, then confirm |
| No temperature, battery, or fuel data on Home screen | GCI not yet paired; check the GCI display for its current state |
| Touch taps are off-target | Re-flash `touch_calibration.bin` at address `0x10000` and run calibration again (Section 3, Steps 2–3) |
| GCD shows blank screen after flashing | Verify flash address — main firmware must be at `0x10000`, not `0x00` |

---

*Golf Cart Computer System — Hands-On Tech*
