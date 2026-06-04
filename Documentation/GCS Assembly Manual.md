# GCS Assembly Manual

This manual covers the physical construction of all three Golf Cart Computer System components: the GCM (Golf Cart Mesh radio), GCD (Golf Cart Display), and GCI (Golf Cart Internal computer). Complete this manual before starting the *GCS Software Installation Manual*.

> **Legal Notice:** The Golf Cart Computer System is an electrical kit intended for educational, demonstration, and prototyping purposes only. It is not a finished consumer product. Assembly requires technical knowledge and experience. By using this kit, the assembler assumes all risks and liability for damage, injury, or death resulting from improper installation or use. It is highly recommended that a licensed professional performs the cart installation. The creators of this kit and the Villages Hands-On Tech Club are not responsible for any modifications or incorrect usage.

All downloadable files — 3D print STLs, wiring diagrams, BOMs, schematics, and assembly instructions — are available from:

> `https://handsontechorg.weebly.com/small_computers.html`

Relevant presentation dates on that page:
- **10/28/2025** — System overview and combined BOM
- **11/25/2025** — GCM assembly and Meshtastic configuration
- **1/27/2026** — GCD assembly instructions
- **2/24/2026** — GCI assembly instructions, schematic, BOM, and 3D print files

**Assembly order:** Build GCM, GCD, and GCI in any order. All three must be complete before starting the *GCS Software Installation Manual*.

---

## 1. GCM Assembly

The GCM (Golf Cart Mesh) is a Heltec T114 Meshtastic LoRa radio with an integrated u-blox GPS. It is housed in a compact 3D-printed enclosure and connects to the GCD via a single RJ 6P6C cable carrying power, GPS signal, and serial communications.

**Download from `handsontech.org > Small Computers > 11/25/2025`:**
- GCM 3D print files (enclosure base, lid, and cart mount)
- GCM Wiring Diagram

### 1.1 Hardware Overview

<img src="Assembly Images/GCM T114 front.jpg" width="320" alt="Heltec T114 with u-blox GPS module, front view">

The Heltec Mesh Node T114 is a low-power development board based on the Nordic nRF52840 and Semtech SX1262. Key specifications:

- **Current draw:** ~49 mA (all peripherals on), ~19 mA (after GPS lock), ~11 µA (deep sleep)
- **Radio:** LoRa (SX1262) and Bluetooth 5.0 — no WiFi
- **Antenna:** U.FL/IPEX connector; internal antenna included, knock-outs support optional external SMA
- **Power:** USB-C, lithium battery, or solar input
- **Accessories:** 1.14" TFT display and u-blox GPS module included

Specifications: `https://heltec.org/project/mesh-node-t114/`

### 1.2 Required Components

- Heltec T114 LoRa development board with L76K GPS module
- LoRa antenna (included with T114 — **never power on the T114 without the antenna connected**)
- RJ 6P6C (6-wire) modular jack with PCB breakout adapter
- 3D-printed enclosure in PETG (base, lid, and optional cart mount) — **use PETG, not PLA; the enclosure is subject to heat**
- **26 AWG silicone-insulated hookup wire** — very strongly recommended; the flexibility is essential for the T114 solder pads
- Heat-shrink tubing

### 1.3 Clip the Reset Button Tab

Before wiring, clip the small protruding plastic tab on the T114's reset button using flush cutters. This allows use of the `GCM base w extended buttons.3mf` 3D printing file which results in a better button feel  — the tab will prevent proper seating if left in place.

<img src="Assembly Images/GCM button clip.jpg" width="320" alt="Clipping the T114 reset button tab with flush cutters">

### 1.4 GPS Signal Tap

The T114's integrated GPS module communicates internally via a small wire bundle. The GPS TX signal line (the **blue wire** in the GPS bundle) must be tapped to route the GPS signal through the RJ connector to the GCD.

<img src="Assembly Images/GCM GPS tap.jpg" width="320" alt="GPS signal tap point on T114 PCB">

**Procedure:** Cut the blue wire (GPS TX) and solder a new wire to join all three cut ends together. Insulate the splice with heat-shrink tubing. This creates a Y-connection: the GPS module's TX output continues to the T114's internal GPS RX pin, and the new wire carries the same signal out through the RJ connector to the GCD.

### 1.5 Soldering on the T114

> **Warning:** Soldering wires directly to the T114 solder pads is the most difficult soldering in the entire GCS project. **Do not attempt this as your first soldering project.** If you are new to soldering, watch `https://youtu.be/jz67KgHzXVw` (the critical technique starts at approximately 7:20).

Requirements:
- Steady hands
- Good eyesight or a magnifier
- 26 AWG silicone-insulated wire (flexibility is critical — standard hookup wire will break the pads)

**T114 soldering tips:**
- Cut wires initially to 3.5 inches, then trim to final length after routing
- Twist the stripped end of each wire and tin it with solder before inserting
- Fill the solder holes on the T114 with solder first, then reheat and fully insert the tinned wire
- Alternatively: carefully remove the T114 display completely before soldering to gain better access to the pads (the display connector is delicate — only attempt if you are familiar with the micro-lever connector used to connect the display)

### 1.6 Wire the RJ Jack

Following the GCM Wiring Diagram below, wire the RJ 6P6C jack breakout to the T114:

- **GPIO9** — serial RX (data from GCD)
- **GPIO10** — serial TX (data to GCD)
- **GPS signal tap** — GPS TX (to GCD pin 3)
- **+5V** — power from GCD
- **GND** — common ground

<img src="Assembly Images/GCM Wiring Diagram.png" width="640" alt="GCM Wiring Diagram — T114 to RJ 6P6C connections">

> **Critical:** Verify all wire endpoints against the wiring diagram before applying power. **Do not power the T114 from two sources simultaneously** (e.g., USB and the RJ cable). The unit can be USB-powered for bench testing and Meshtastic configuration before the RJ cable is installed.

<img src="Assembly Images/GCM RJ jack wired.jpg" width="320" alt="RJ jack wired and ready for enclosure">

### 1.7 3D Printed Enclosure

Print the enclosure parts (base, lid, and optional cart mount) in **PETG** filament. PLA is not recommended due to heat tolerance requirements in a cart environment.

> **Note:** Tolerances are intentionally tight. The lid will require some force to snap together.

The enclosure supports two antenna configurations:
- **Internal antenna** — (recommended) the included flexible antenna lays loosely behind the PCB inside the enclosure
- **External antenna** — knock-outs are provided for an SMA connector through the enclosure wall

### 1.8 Final Assembly

Route all wires cleanly and install the T114 and RJ jack breakout into the enclosure base. When seating the T114, ensure the USB-C connector is properly aligned with the enclosure opening before pressing the T114 board fully home.


Attach the LoRa antenna. If using the internal antenna, lay it loosely in the space behind the PCB (or install the optional Molex dipole antenna). Close with the lid.

<img src="Assembly Images/GCM fully wired.jpg" width="320" alt="GCM fully wired, ready for lid">

<img src="Assembly Images/GCM assembled.jpg" width="320" alt="GCM in completed enclosure">

After completing software configuration in the *GCS Software Installation Manual*, label the enclosure with the last four characters of the T114's Node ID (visible in the Meshtastic app).

---

## 2. GCD Assembly

The GCD (Golf Cart Display) is built around the ESP32-based CYD 2432S028R touchscreen board. It fits into a 3D-printed enclosure in two variants: **Shelf Mount** (clips over a horizontal dash surface) or **Screw Mount** (attaches to existing dash screw holes via a ¼"-20 heat-set insert).

**Download from `handsontech.org > Small Computers > 1/27/2026`:**
- GCD Assembly Instructions (full illustrated guide)
- GCD 3D print files (both enclosure variants)
- GCD Wiring Diagram

### 2.1 Components

<img src="Assembly Images/GCD CYD labeled.jpg" width="320" alt="CYD 2432S028R with key connectors labeled">

**Required components:**
- CYD ESP32-2432S028R touchscreen display board
- 2× RJ 6P6C modular jacks with PCB breakout adapters
- 3D-printed enclosure (select screw-mount or shelf-mount variant)
- Small speaker (included or sourced separately)
- #4 × 3/8" flat-head metal screws × 2 (back cover)
- #4 × 1" pan-head metal screws × 2 (RJ jack cover — shelf mount)

**Screw-mount variant only:**
- ¼"-20 heat-set threaded insert × 1

### 2.2 Wiring

Wire both RJ jacks to the CYD following the GCD Wiring Diagram:

<img src="Assembly Images/GCD Wiring Diagram.jpg" width="480" alt="GCD Wiring Diagram — CYD to RJ 6P6C connections">

> **Critical — do NOT wire by color.** The wire colors on the wiring diagram may not match your wire set. **Verify every wire by its start and end points**, not by color. Wiring by color will result in incorrect connections.

Additional wiring notes:
- **Do not shorten any wires.** The full wire lengths are required to reach all connection points during assembly.
- **Wire the RJ jack PCB breakout on the correct side** — refer to the wiring diagram for board orientation before soldering.
- **One jack (labeled "To Mesh Radio")** carries: +5V, GND, GPS RX (CYD pin 3), serial RX/TX to/from GCM (CYD pins 16/17)
- **Second jack (labeled "To GCI")** carries: +5V, GND, ignition status signal

After wiring is complete: plug both RJ jack PCBs into the CYD board and unplug the speaker (it will be re-installed during case assembly).

### 2.3 Pre-Assembly Preparation

Before beginning case assembly:
1. Confirm all wires are soldered correctly into both RJ jack PCBs and plugged into the display PCB.
2. Unplug the speaker connector — it will be reconnected during case assembly.
3. Color-code the two RJ jack connectors with a marker or tape to distinguish "To Mesh Radio" from "To GCI" at a glance.
4. Print and confirm all 3D printed parts are ready: enclosure body, back cover, front speaker cover, RJ jack cover, and reset button peg.
5. **Optional:** Use the tip of a soldering iron to briefly heat the reset button peg to mushroom its top slightly — this keeps the peg from falling out.

### 2.4 Shelf Mount Assembly

The shelf-mount variant clips over the front lip of a horizontal golf cart tray.

<img src="Assembly Images/GCD shelf mount assembly.jpg" width="320" alt="GCD in shelf-mount enclosure">

**Step 1 — Insert speaker and route wire.**

Insert the speaker into the front cover. Orient the speaker so the wire exits at the bottom-right corner after insertion.

**Step 2 — Tilt in the display PCB.**

Tilt the CYD board in at the bottom edge first, then press the top edge inward. During insertion:
- Ensure the board is fully seated and tight at the bottom before pressing the top home.
- Do not press in the center of the PCB — apply pressure at the edges.
- Tolerances are intentionally tight; some force is normal.

**Step 3 — Route wires under the retainer.**

Route all wires under the wire retainer as shown in the assembly instructions. Twist the wire bundle to help it stay in place. Confirm the speaker wire exits at the lower-left corner, seated in the slight notch provided.

**Step 4 — Fit speaker into the front cover.**

Seat the speaker into its pocket in the front cover. Ensure the speaker wire exits exactly as shown — no wires should be pinched underneath the speaker.

**Step 5 — Install the back cover.**

Insert the back cover into its slots. Install the reset button (if separate). Secure with two **#4 × 3/8" flat-head metal screws**. Do not use longer screws.

**Step 6 — Insert RJ jack PCBs into their cover.**

Slide both RJ jack PCBs into the RJ jack cover and snap on the cover.

**Step 7 — Attach the RJ jack cover.**

Slide the RJ jack cover into the speaker cover. Tuck all wires into the RJ jack cover — twist the bundle to make this easier and confirm no wires are pinched. Secure with **#4 × 1" pan-head metal screws**. Do not use longer screws.

**Step 8 — Install on dash.**

The completed assembly clips over the lip of the golf cart shelf.  Be sure the clips are engaged when attaching.  The fit should be snug and not slide easily.

<img src="Assembly Images/GCD in enclosure.jpg" width="320" alt="GCD display board seated in enclosure">

### 2.5 Screw Mount Assembly

The screw-mount variant attaches to a dash surface via a ¼"-20 threaded insert.

<img src="Assembly Images/GCD screw mount assembly.jpg" width="320" alt="GCD in screw-mount enclosure">

**Step 1 — Install the heat-set insert.**

Press a ¼"-20 threaded heat-set insert into the designated boss on the enclosure back using a soldering iron set to approximately 200°C. See the installation technique video at `https://www.youtube.com/watch?v=KC1LLU54DKU`.

**Step 2 — Tilt in the display PCB.**

Same as Shelf Mount Step 2: tilt the board in bottom-first, press top home. Tolerances are tight — do not press the PCB center.

**Step 3 — Assemble the back cover.**

Route all wires through the back cover opening. Connect the speaker to the PCB. Snap on the RJ jack covers. Do not pinch any wires — some force may be required to seat the covers.

**Step 4 — Install the back cover.**

Insert the back cover into its slots. Install the reset button (if separate). Secure with two **#4 × 3/8" flat-head metal screws**. Do not use longer screws.

---

## 3. GCI Assembly

The GCI (Golf Cart Internal) is a custom PCB assembly housing an ESP32 1.14" LCD development board, with interfaces for temperature sensing, fuel level measurement, battery voltage monitoring, and headlight relay control.

**Download from `handsontech.org > Small Computers > 2/24/2026`:**
- GCI Assembly Instructions (step-by-step illustrated guide)
- Technical bundle — Bill of Materials, schematic (GCI v2.0), KiCad files for PCB ordering
- GCI 3D print files — enclosure bottom, lid, standoffs, and optional Yamaha mounting bracket

### 3.1 Components and PCB

The GCI PCB is a 2-layer board ordered from JLCPCB (or equivalent) using the KiCad files in the Technical bundle. Download and print the **Component List** from the Technical bundle — cross off each component as you install it.

<img src="Assembly Images/GCI all components.jpg" width="480" alt="All GCI components laid out">

**Required components:**
- ESP32 ideaSpark 1.14" LCD development board (U1)
- GCI custom PCB v2.0
- DC-DC step-down converter (adjustable; set to 5V before installation — see §3.6.1)
- 4N25 optocoupler (U3) — ignition and relay isolation
- Active-high relay module (4-relay board; only Relay 1 used)
- Schottky diodes D1, D2
- Electrolytic capacitor C2 (verify polarity)
- Capacitors C1, C3, C4, C5
- 3A fuse (F1)
- Resistors R1, R2, R3 (R_BAT), R4, R5, R6, R7 (see §3.2 for value selection)
- RJ 6P6C jack (J2)
- Phoenix screw terminal connectors (J3, J4, J10)
- 6-pin SIP header J1 (relay connection)
- Female DIP 02×12 socket (J8 — **mounts on reverse side of PCB**)
- Two 01×15 SIP female sockets (CPU socket rails)
- Right-angle tactile push button switch (SW1)

**Optional components** (install based on cart configuration):
- DS18B20 temperature probe and cable — interior temperature sensing
- BH1750 light sensor module — automatic headlight control; requires i2c bus extender U2 (PCA9515BDGKR) and resistors R9, R10, R11, R12
- Resistors R8, R9, R10, R11, R12 (optional features)
- DPDT slide switch S1 and resistors R6, R8 — ignition simulation for bench testing (requires cutting jumper JP2)

### 3.2 Pre-Assembly: Resistor Selection

Two resistor values must be determined before populating the PCB, based on your cart's battery voltage and fuel sender output.

**R3 (R_BAT) — Ignition voltage sensing:**

R3 is part of a resistor divider that scales the cart's battery voltage down to a level safe for the ESP32. Select based on cart battery voltage:

| Battery Type | Approx. Charging Voltage | R_BAT (R3) Maximum Value |
|---|---|---|
| +12V | +15V | 220 KΩ |
| +48V | +60V | 56 KΩ |
| +51.3V (lithium) | +60V | 56 KΩ |

**R4 & R5 — Fuel sender voltage divider:**

The ESP32 ADC input must not exceed 3.3V (2.5V is ideal for best linearity). R4 and R5 form a divider to scale down the fuel sender output voltage. Use 100 KΩ for R4 and solve for R5 based on your sender's full-tank output voltage.

> If your fuel sender output is less than 2.5V at full tank (measure at empty and full with a voltmeter), R4 and R5 are not needed — omit them or leave those positions unpopulated.

### 3.3 PCB Assembly Tips

- **Identify all parts with certainty** before beginning — separate and label them if necessary.
- **Install smallest/shortest components first** to maintain soldering access (resistors and small capacitors before connectors and sockets).
- **For long-lead components:** insert and bend the leads to hold the component in place while soldering.
- **Observe polarity** on all polarized parts: electrolytic capacitors (C2), Schottky diodes (D1, D2), and the 4N25 optocoupler (U3). Pin 1 is marked by the square pad on the PCB and by the dot on the IC.
- **If new to soldering:** watch `https://www.youtube.com/watch?v=jz67KgHzXVw` (starting at about 7:20 for the critical technique).
- **Allow inexpensive soldering irons** 5 minutes to fully heat before use.
- **Multi-pin components:** solder one pin, reheat and reposition if necessary before soldering remaining pins.
- **After soldering:** clean the PCB with isopropyl alcohol and a toothbrush, then inspect with a magnifier.

### 3.4 PCB Population

<img src="Assembly Images/GCI PCB blank.jpg" width="480" alt="GCI PCB — blank, front and back">

**Step 1 — Low-profile components first:**
- Optional U2 i2c bus extender (PCA9515BDGKR — surface-mount; install before U3 to allow soldering room)
- Capacitors C1, C3, C4, C5
- 3A fuse F1
- 4N25 optocoupler U3 — verify orientation (Pin 1 = square pad/dot mark)

**Step 2 — Discrete through-hole components:**
- Resistors R1, R2, R3 (R_BAT — use value from §3.2), R4, R5, R6, R7
- Optional resistors R8, R9, R10, R11, R12
- Schottky diodes D1, D2 — verify polarity (note the band marking on diode body)
- Electrolytic capacitor C2 — verify polarity (negative lead toward marked stripe)

**Step 3 — Connectors, sockets, and the CPU:**
- J1 — 6-pin SIP header (relay connection)
- J2 — Wurth RJ 6P6C jack
- J3 — Phoenix 01×09 socket
- J4 — Phoenix 01×02 socket
- J8 — Female DIP 02×12 socket — **mount on the REVERSE (bottom) side of the PCB**
- J10 — Phoenix 01×03 socket
- Optional S1 — right-angle DPDT slide switch (bench testing)
- SW1 — right-angle tactile push button switch
- Two 01×15 SIP female sockets (CPU socket rails)
- U1 — ideaSpark ESP32 with 1.14" display (installs into the SIP socket rails)

<img src="Assembly Images/GCI PCB populated.png" width="480" alt="GCI PCB fully populated with ESP32 installed">

### 3.5 Attach Standoffs

Attach standoffs to the PCB using M3 × 18 mm screws (or #4 × ¾" screws) with their respective nuts. The excess screw length protruding beyond the nut is essential — it seats inside the enclosure standoff holes to hold the PCB in place.

- **Use the longer standoffs** if not adding a daughterboard (most builds).
- **Use the shorter standoffs** only if adding a daughterboard on top.

### 3.6 Enclosure Assembly

#### 3.6.1 Set the DC-DC Converter to 5V — CRITICAL

> **This step must be completed before installing the DC-DC converter in the enclosure. Skipping it can permanently damage the GCI and ESP32.**

Using a digital multimeter, adjust the DC-DC converter's trimmer potentiometer until the output reads **+5.0 to +5.2V DC**. Do not exceed +5.2V. Even if your converter is labeled or described as fixed at 5V, **verify the output voltage before installation**.

#### 3.6.2 Mark the Relay Wiring Orientation

Using a red permanent marker, place a dot next to the **last pin** of the 6-pin relay module connector (J1). When attaching the relay wiring bundle, match the red wire to this dot before inserting. This prevents reversed relay wiring.

#### 3.6.3 Component Placement

<img src="Assembly Images/GCI enclosure assembled.jpg" width="480" alt="GCI boards installed in enclosure, lid removed">

1. **Install the grommet** — slit the grommet and press it into the cable entry in the enclosure wall.
2. **Wire the relay board** as shown in the assembly instructions:
   - Tin all relay wires with solder before inserting into the relay screw terminals.
   - Sleeve a portion of each wire where possible.
   - Wire color order: RED, BLU, GRN, WHT, YEL, BLK (when using the specified jumper).
   - Relay control wires must exit at the bottom of the relay board.
3. **Mount the DC-DC converter** using #4 × 3/8" sheet metal screws and washers. Note: DC-DC converter models vary in appearance.
4. **Arrange the relay board and DC-DC converter** per the assembly instructions before securing.

#### 3.6.4 PCB Installation

Seat the mainboard (with standoffs attached) so the standoff screws sit **on top of** the enclosure standoff posts, with the excess screw length going **inside** the hollow standoff holes.

> **Pushbutton warning:** The SW1 tactile push button protrudes through the side of the enclosure. Be careful when installing or removing the PCB. If the GCI display behaves erratically, the button is likely pressing against the case wall — center it before closing.

#### 3.6.5 External Sensor Connections

**DS18B20 Temperature Probe (J3):**

| Wire Color | Signal | J3 Position |
|---|---|---|
| Red | +5V | J3-1 |
| Yellow / White / Blue | Data (DQ) | J3-2 |
| Black | GND | J3-3 |

Best installed in the wheel well where air circulates. Wire can be extended as needed.

**PCB connection summary (J3 Phoenix connector):**
- J3-1: +5V (DS18B20 power)
- J3-2: 1-Wire data bus (DQ)
- J3-3: GND

**BH1750 Light Sensor** (optional, requires i2c extender U2): connects via the i2c bus Phoenix connector or the daughterboard header.

**Fuel sender** (optional): connects to the ADC fuel sender input. See §3.2 for voltage divider selection.

**Battery monitor / ignition sense**: connect to the designated Phoenix terminals per the GCI schematic.

### 3.7 Optional: Yamaha Golf Cart Mounting Bracket

For 2017 and similar Yamaha gas carts, the 3D print files bundle includes two bonus files:
- **Yamaha angled bracket** — allows mounting the GCI enclosure on the angled area behind the ignition key while keeping the enclosure level
- **Yamaha bracket drilling template**

Hardware required: eight M4 × 4 mm depth heat-set inserts, four M4 × 10 mm screws, and four M4 × 6 mm screws.

---

*Golf Cart Computer System — Hands-On Tech*
