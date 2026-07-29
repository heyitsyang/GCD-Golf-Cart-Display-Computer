# GCS Mailbox Sensor Manual

The Mailbox Sensor is an optional accessory for the Golf Cart Computer System. It sits inside your postal mailbox, detects when mail has been delivered, and lights a yellow envelope on the Home screen of your cart's display.

This manual covers the whole life of the sensor: preparing it, programming it, installing it, pairing it to a cart, and reading its status. It is written for both the installer and the cart owner.

> **Prerequisite:** the cart must already have a working **GCM** mesh radio. If the Messages screen on your GCD never shows any traffic, fix that first — see the *GCS Software Installation Manual*.

---

## Contents

1. [Overview](#1-overview)
2. [Hardware](#2-hardware)
3. [How the Sensor Talks to the Cart](#3-how-the-sensor-talks-to-the-cart)
4. [Before You Install — The Primary Channel Rule](#4-before-you-install--the-primary-channel-rule)
5. [Programming and Commissioning](#5-programming-and-commissioning)
6. [Pairing to a Cart](#6-pairing-to-a-cart)
7. [Installing in the Mailbox](#7-installing-in-the-mailbox)
8. [Reading Sensor Status](#8-reading-sensor-status)
9. [The Mail Glyph on the Home Screen](#9-the-mail-glyph-on-the-home-screen)
10. [Un-pairing and Replacing a Sensor](#10-un-pairing-and-replacing-a-sensor)
11. [Sharing One Mailbox Between Two Carts](#11-sharing-one-mailbox-between-two-carts)
12. [Troubleshooting](#12-troubleshooting)
13. [Reference — Frame Format](#13-reference--frame-format)
14. [Limitations](#14-limitations)

---

## 1. Overview

The Mailbox Sensor is a small battery-powered unit that mounts inside a standard postal mailbox. A time-of-flight range sensor watches the inside of the box. When something is placed in it, the sensor reports **PRESENT** over the Meshtastic mesh; when the box is emptied, it reports **ABSENT**.

Your GCD listens for those reports and lights the envelope glyph on the Home screen. Drive home, glance at the display, and you know whether the walk to the mailbox is worth making.

Two characteristics make the sensor behave unlike anything else in the system, and nearly every rule in this manual follows from one of them:

**The sensor is transmit-only.** It never listens. There is no acknowledgement, no handshake, and no way for your cart to send it a command or a confirmation. This is a deliberate design choice — a receiver would dominate the power budget and cut battery life dramatically.

**The sensor broadcasts.** Its reports are not addressed to your cart. They go out to the whole mesh, and every cart in range hears every mailbox. This is why the sensor has an identity (`mbx_id`) and why pairing exists: your GCD must be told which one of the mailboxes it can hear is *yours*.

Together these give pairing an unusual shape. The sensor **offers** its identity when you press a button on it, and your GCD **accepts** that offer locally. The sensor is never told it was accepted and does not care. Consent flows one way only.

---

## 2. Hardware

> ⚠️ **This section is not yet complete.** The mailbox sensor hardware has not been built. Component list, battery type and expected life, enclosure, mounting hardware, and the exact location of the pair button will be documented once the design is finalised.
>
> Everything in the rest of this manual describes behaviour that is **built, verified on the display, and final** — the GCD half of the system is complete and tested.

**What is established:**

- An **ATtiny1614** microcontroller manages the range sensor, the power switching, and the night-transmit gating.
- A **Seeed XIAO** module running stock Meshtastic firmware provides the LoRa radio.
- The unit is **battery powered**, with the radio kept switched off except when a report is due.
- There is a **pair button**, pressed once during setup.
- There is **no display, no label, and no status indicator** on the sensor. This is why §5 matters.

*(Detailed design records: DDR01 — ToF Sensor, DDR03 — Load Switch & LDO, DDR04 — Night Transmit Gating, DDR05 — ATtiny1614 Programming.)*

---

## 3. How the Sensor Talks to the Cart

Understanding the reporting pattern explains most of what you will see on the display.

**Reports are sent on change, and repeated hourly.**
The sensor transmits whenever the mailbox goes from empty to occupied or occupied to empty. It also **re-sends its current state once an hour** regardless of whether anything changed.

**There is no retry.** A report is sent once, best-effort. If the mesh drops it, the sensor never knows and never resends it. The hourly repeat is what makes this acceptable: a lost report costs you at most one hour of delay, and the system corrects itself without anyone intervening. That self-healing property is relied on throughout the design.

**The sensor goes quiet overnight.** To save battery, transmissions are gated during night hours. A silence of eight to twelve hours is completely normal and is not a fault. This is also why the display's health indicator counts *missed reports* rather than *time since last heard* — a simple timer would raise an alarm every single morning.

**Practical consequences:**

| | |
|---|---|
| Mail delivered | Glyph lights within seconds, with a chime |
| Mail collected | Glyph clears within seconds |
| A report is lost | Corrects itself at the next hourly repeat |
| Overnight | No traffic at all — expected |
| Cart powered off | Nothing is missed; the sensor's reports simply go unheard, and the display re-syncs within an hour of powering up |

---

## 4. Before You Install — The Primary Channel Rule

**This is the single most important step in the whole installation, and getting it wrong fails silently.**

Meshtastic derives the radio's operating frequency slot from a hash of the **primary channel name** — channel 0. Two radios whose primary channels are named differently are physically transmitting on different frequencies. They will never hear each other, no matter how correct everything else is, and neither one reports an error.

**Therefore: the sensor's primary channel must be `GolfCart`** — the same primary channel the cart mesh uses.

It is not enough to add `GolfCart` as a secondary channel on the sensor, and it is not enough to add a `Mailbox` channel to the GCM. The name in **slot 0** is what selects the frequency.

Configure the sensor's Meshtastic radio to match the GCM exactly:

**LoRa**

| Setting | Value |
|---|---|
| Region | United States |
| Use Preset | ON |
| Preset | Long Range — Fast (LongFast) |
| Frequency Slot | 20 |
| Frequency Override | 906.875 |

**Channels**

| # | Name | PSK |
|---|---|---|
| **0 (primary)** | **`GolfCart`** | `handsontechclubtelemeg==` |

The sensor needs no other channels. See §1 of the *GCS Software Installation Manual* for the full GCM channel configuration these values are taken from.

### Verifying the radio link

Before installing the sensor in the mailbox, confirm the cart can actually hear it:

1. On the GCD, open **Menu → Messages**.
2. Set the filter dropdown at the bottom-left to **DEBUG**. This is the only filter that shows mailbox traffic.
3. Trigger the sensor, or press its pair button.
4. A row should appear within a few seconds — see the table below.
5. Set the filter back to **ALL** when you are finished.

| Row | Meaning |
|---|---|
| `<MBX_PAIR a1b2c3d4>` | A pair offer from mailbox `a1b2c3d4` |
| `<MBX a1b2c3d4 P>` | Mailbox `a1b2c3d4` reports mail **P**resent |
| `<MBX a1b2c3d4 A>` | Mailbox `a1b2c3d4` reports mail **A**bsent |

If nothing appears here, the radio configuration is wrong — almost always the primary channel name. Do not proceed to mounting until you see these rows.

> Mailbox traffic is hidden from every filter except DEBUG. It is data, not conversation, and it would otherwise flood the message list.

---

## 5. Programming and Commissioning

### The sensor's identity

Each sensor computes its own identity, `mbx_id`, from the serial number burned into its ATtiny chip at manufacture. It is an eight-character hexadecimal value such as `a1b2c3d4`. It is unique, it cannot be changed, and it is not related to the Meshtastic node number.

**The sensor has no way to tell you what its id is.** No display, no label, no blink code. On a street with several sensors, mailbox traffic is a stream of anonymous hex ids and there is nothing to distinguish yours.

The pairing procedure in §6 solves this for the owner — you press the button on your own sensor and the id that appears is necessarily yours. But you should still record the id at programming time, because it is the only way to identify a unit later without physically pressing its button.

### Commissioning steps

1. Flash the ATtiny1614 firmware over UPDI.
   > ⚠️ **TBD** — programmer, pinout, and firmware image pending final hardware (see §2). General flashing setup is covered in §2 of the *GCS Software Installation Manual*.
2. Configure the XIAO's Meshtastic radio per [§4](#4-before-you-install--the-primary-channel-rule).
3. **Read out the `mbx_id`** over the programming interface.
   > ⚠️ **TBD** — exact readout command pending final firmware.
   >
   > *Alternative that works today:* press the pair button with a GCD nearby and read the id from the DEBUG row (§4) or from the Settings 2 screen (§6).
4. **Write the id on a sticker and put it inside the mailbox lid.** Do this every time. It costs nothing now and is the only durable record; recovering it later means going back to the mailbox with a programmer or a cart.
5. Verify the radio link per §4 before closing the enclosure.

---

## 6. Pairing to a Cart

Pairing tells your GCD which mailbox is yours. It is done once and survives power cycles.

> ### Pair first, mount second
>
> **Do this with the sensor in your hand, sitting in the golf cart.** The sensor detaches easily and the display does not — it is bolted to the cart. So bring the sensor to the display, pair the two, and only then walk the sensor out to the mailbox and mount it ([§7](#7-installing-in-the-mailbox)).
>
> Pairing at the mailbox works too, but it means operating a touchscreen and a button that are metres apart, against a 45-second clock, and often at arm's length inside a metal box. There is no reason to make it difficult.

### Before you start

Sit in the cart with the sensor in hand and the display powered up. You will need to reach the pair button on the sensor.

### The procedure

**Open Settings 2 on the display first, then press the button on the sensor.** That order matters: the offer is only claimable for **45 seconds**, and you do not want to spend that budget navigating menus.

1. On the GCD, swipe on the Home screen to open the **Menu**.
2. Tap the **gear** icon to open **Settings**, then the **right arrow** to reach **Settings 2**.
3. The mailbox row is the second one, below `PAIR GCI`. It reads **`PAIR MBX`** with **`NO MAILBOX`** beside it in white.
4. **Press the pair button on the sensor**, which you are holding.
5. Within a few seconds the display responds:
   - The **`PAIR MBX` button turns green** — an offer is waiting and tapping it will do something.
   - The value beside it reads **`a1b2c3d4 OFFERED`** in white.
6. **Check the id against the sticker** you wrote in §5, then **tap `PAIR MBX` once**. A chime confirms.
7. The value settles to **`a1b2c3d4`** in white, and the button returns to blue. You are paired.
8. Now take the sensor out to the mailbox and mount it ([§7](#7-installing-in-the-mailbox)).
9. The value turns **green** the first time the sensor reports in from its mounted position — immediately if the mailbox state changes, otherwise within the hour. That first green is also your confirmation that the sensor is in range where it now lives.

### If it does not work

**Nothing appears.** The offer window is 45 seconds from the last frame heard. Press the button again — each press restarts the window. If repeated presses produce nothing at all with the sensor right beside the display, the radios are not talking; go back to §4.

**A different id appears.** You are seeing a neighbour's sensor offering at the same moment — a real possibility on a street where several are installed. Do not accept it. Press your own button again; the newer offer replaces the older one, and the display will show the new id. Pairing with the sensor in hand makes this easy to resolve, which is another reason to do it that way.

**You tapped too late.** An unaccepted offer simply expires. Nothing was claimed and nothing was harmed. Press the button and try again.

### Why there is no confirmation on the sensor

The sensor never receives anything, so it has no idea it was paired and nothing on it changes. The confirmation is on the display, and the guarantee that the id is yours comes from the fact that *you pressed the button*. That is what makes this safe without a handshake: you are not choosing from a list of ids you cannot verify, you are confirming one you caused.

---

## 7. Installing in the Mailbox

Mount the sensor **after** pairing it ([§6](#6-pairing-to-a-cart)).

> ⚠️ **This section is not yet complete.** Mounting position, aiming the range sensor at the interior of the box, weatherproofing, and battery access will be documented once the enclosure design is finalised.

**What is known now:** the range sensor must have a clear, unobstructed view of the space where mail lands, and the unit must be positioned so that the mailbox door can close normally.

**Confirm the range from the mounted position.** A metal mailbox attenuates the signal considerably, so a link that worked on the workbench is not proof. Once mounted, open and close the mailbox door to force a state change, then check that the value on Settings 2 turns **green** — or watch for an `<MBX …>` row under the DEBUG filter (§4). If nothing arrives, the sensor is out of range where it now sits.

If you ever remove the sensor — to change a battery, or to move it to a different mailbox — the pairing is unaffected. It lives in the display, not in the sensor.

---

## 8. Reading Sensor Status

The `PAIR MBX` row on Settings 2 doubles as a health display. The text tells you the id; the colour tells you how well the link is working.

| Display | Colour | Meaning |
|---|---|---|
| `NO MAILBOX` | white | No sensor paired |
| `a1b2c3d4 OFFERED` | white | A sensor is offering to pair — tap to claim it |
| `a1b2c3d4` | white | Paired, but nothing heard yet since the display powered up |
| `a1b2c3d4` | **green** | Paired and healthy — heard, with no reports missed |
| `a1b2c3d4 (3)` | **yellow** | Paired and heard, but 3 reports went missing |
| `a1b2c3d4 SILENT` | **red** | Paired, and nothing heard for 24 hours of listening |

White is neutral, never a fault. On power-up the display has simply not heard from the sensor yet, and it will not until the sensor's next hourly report. Waiting up to an hour for green is normal.

### What the yellow count means

The number in parentheses counts **reports that went missing** — gaps in the sequence numbering the sensor puts in every frame. It is a **link-quality hint, not an outage log**, and it should be read with three things in mind:

**It resets every time the display powers up.** This is deliberate. Reports sent while your cart was parked in the garage were never missed — nobody was listening. Counting them would turn every overnight into an alarm.

**It counts gaps, not time.** The overnight quiet period produces no gap, because a sensor that is not transmitting is not advancing its sequence number either. Night gating is invisible to this counter, which is exactly why it was designed this way.

**Very long outages can read as healthy.** The sequence number is a single byte and wraps at 256. An outage of about 10.7 days — or any multiple of it — lands back on the same number and looks like a perfect run. There is no way to recover that from the display side; the information is not in the message. In practice this does not matter, because an outage that long will have shown up as `SILENT` or as an obviously wrong mailbox state long before.

**A small and stable count is not a problem.** Mesh radio is best-effort and the occasional dropped frame is normal. A count that climbs steadily, on the other hand, suggests marginal range or a fading battery.

### About the red `SILENT` state

`SILENT` means the display has been listening continuously for 24 hours and heard nothing at all from the paired sensor. Since the longest legitimate quiet period is a single night, 24 hours of listening is roughly double that, and this reliably indicates a dead battery or a failed unit.

> ### ⚠️ If your cart has a GCI, `SILENT` may never appear
>
> The 24 hours is measured in **listening time**, not calendar time. On a cart fitted with a **GCI**, the display sleeps whenever the ignition is off, and the timer restarts on every wake-up. Unless you run the cart continuously for a full day, the threshold is never reached.
>
> **This is intended behaviour, not a fault.** A display that is asleep genuinely cannot tell a dead sensor from a parked cart. The alternative — measuring calendar time — would flag a healthy sensor as failed on any cart left standing for a day, which is worse.
>
> On a cart **without** a GCI, the display is powered continuously, and `SILENT` works exactly as described.
>
> Either way, a truly dead sensor announces itself the ordinary way: the glyph stops responding to your mail. If you suspect one, press its pair button — an offer appearing on Settings 2 proves the unit is alive.

---

## 9. The Mail Glyph on the Home Screen

<img src="GCD%20Screens/Home.jpg" width="213" alt="Home Screen showing the mail glyph in the bottom icon bar">

The yellow **envelope** in the bottom icon bar is the whole point of the sensor. It is the second icon, between the message-count badge and the headlight symbol, and it is only visible when there is mail waiting.

**Mail arrives** → the envelope appears with a chime.
**Mail is collected** → the envelope disappears on its own, within seconds.

### Snoozing the glyph

If you know about the mail and do not want the reminder, **press and hold the envelope for about a second**. A click confirms, and the envelope goes out.

**The snooze is temporary, and that is deliberate.** The mail has not gone anywhere, so the reminder comes back — at the sensor's next hourly report, **with a chime**. If you want it gone for good, collect the mail.

A short tap does nothing. The hold is required because the envelope sits in the middle of a screen you touch often, and dismissing it is the one action there that throws information away.

### Two behaviours worth knowing in advance

**A second delivery does not re-alert.** If a second item arrives before you have collected the first, the mailbox was already occupied, so the sensor has nothing new to report and the display has nothing new to show. There is no way around this — the information never reaches the cart.

**After a power cycle the glyph starts dark.** The display does not remember the mailbox state across power-ups, so it shows nothing until the sensor's next report — up to an hour. If you power up the cart and the envelope is missing, wait an hour before concluding anything is wrong.

### The chime rules, in full

| Event | Sound |
|---|---|
| Mail arrives, glyph lights | Alert chime |
| Hourly repeat while the glyph is already lit | *Silent* — nothing has changed |
| Hourly repeat after you snoozed, glyph returns | Alert chime |
| You snooze the glyph | Click |
| Mail collected, glyph clears | *Silent* |

The rule underneath all of this is simply: **the display chimes when the envelope lights up, and only then.** A snoozed reminder that came back without a sound would be an alarm you cannot hear, which would make snoozing useless.

---

## 10. Un-pairing and Replacing a Sensor

**To un-pair:** on Settings 2, **press and hold `PAIR MBX` for about a second**. A confirmation tone plays and the value returns to `NO MAILBOX`. The feature goes dormant; the envelope will not light again until something is paired.

A short tap will not do this — the hold protects against accidentally unpairing while reaching for something else on the screen.

**To replace a sensor:** you do not need to un-pair first. Bring the new sensor to the cart and pair it following [§6](#6-pairing-to-a-cart). Accepting a different id replaces the old pairing and clears its history — the miss count, the health colour, and the glyph all start fresh.

**To move a sensor to a different mailbox:** nothing to do. The pairing is stored in the display and follows the sensor, not the location.

Un-pairing also clears the history, so a unit that had accumulated a large yellow count starts clean if you pair it again.

---

## 11. Sharing One Mailbox Between Two Carts

Two carts in the same household can both watch the same mailbox. Pair each one separately, and do it before mounting: with the sensor in hand, sit in the first cart and pair it ([§6](#6-pairing-to-a-cart)), then carry the sensor to the second cart and repeat. Mount it in the mailbox once both are done.

If the sensor is already mounted, you can still pair a second cart from the mailbox — but taking it down for a minute is easier.

This works because the sensor never learns it was accepted, so there is nothing for a second cart to conflict with. Both displays will light, chime, and clear independently. Snoozing on one cart has no effect on the other.

---

## 12. Troubleshooting

| Symptom | Likely cause | What to do |
|---|---|---|
| No mailbox rows appear under the DEBUG filter, ever | The sensor's **primary** channel is not `GolfCart`, so the radios are on different frequencies | Reconfigure per [§4](#4-before-you-install--the-primary-channel-rule). This is by far the most common failure |
| Pressing the pair button produces nothing on screen | Sensor battery flat, radio misconfigured, or the 45-second window expired | Pair with the sensor in hand in the cart (§6) and press again. If several presses at arm's length do nothing, go to §4 |
| Paired fine, but the value never turns green once mounted | The mailbox is out of range, or the metal box is attenuating the signal too much | Open and close the mailbox door to force a report. If still nothing, reposition the sensor within the box (§7) |
| `OFFERED` shows an id that is not yours | A neighbour's sensor offered at the same moment | Do not accept it. Press your own button again — the newer offer replaces it |
| Value stays white and never turns green | No report heard yet this power-up | Normal. Wait up to an hour, or open the mailbox door to force a state change |
| Yellow count climbing steadily | Marginal range, or a fading sensor battery | Check range from the mounted position; plan to replace the battery |
| Red `SILENT` | Nothing heard in 24 hours of listening — battery or unit failure | Press the pair button. If an offer appears, the unit is alive and the problem is elsewhere. If not, replace the battery |
| `SILENT` never appears on a cart you know is not receiving | Cart has a GCI, so the display sleeps and the timer restarts | Expected — see the note in [§8](#about-the-red-silent-state) |
| Envelope is lit but the mailbox is empty | A snooze you were not expecting, or a missed ABSENT report | Long-press the envelope to snooze; it corrects itself at the next hourly report |
| Envelope missing right after powering up | State is not retained across power cycles | Expected. Wait up to an hour |
| Second delivery produced no chime | The box was already occupied, so the sensor had nothing new to report | Expected — see [§9](#two-behaviours-worth-knowing-in-advance) |
| Mailbox traffic clutters the message list | The filter is set to DEBUG | Set the filter back to **ALL** |

---

## 13. Reference — Frame Format

For anyone reading raw mesh traffic. The sensor sends a plain-text broadcast:

```
|#03#NORM#a1b2c3d4#5#87#142#PRESENT
 │   │    │        │ │  │   └── state       PRESENT or ABSENT
 │   │    │        │ │  └────── range in mm
 │   │    │        │ └───────── battery %
 │   │    │        └─────────── sequence number, 0-255, wrapping
 │   │    └──────────────────── mbx_id, 8 hex characters
 │   └───────────────────────── mode: NORM, DEV, or PAIR
 └───────────────────────────── packet type 03 = mailbox
```

In a **`PAIR`** frame — sent when the pair button is pressed — only the mode and the `mbx_id` carry meaning. Every other field may contain anything and is ignored by the display.

The **sequence number** is what the miss count in §8 is derived from. It advances by one for every transmission, so a jump of more than one means frames were lost in between.

The GCD abbreviates these in the DEBUG message list as `<MBX a1b2c3d4 P>` for state reports and `<MBX_PAIR a1b2c3d4>` for pair offers.

---

## 14. Limitations

Every item here is a deliberate design decision, explained in the section referenced. None is a defect.

| Limitation | Why | Section |
|---|---|---|
| A second delivery before the first is collected raises no alert | The mailbox was already occupied, so the sensor has nothing new to send | [§9](#two-behaviours-worth-knowing-in-advance) |
| The glyph is dark after power-up until the next report, up to an hour | Mailbox state is not stored across power cycles; the hourly repeat corrects it | [§9](#two-behaviours-worth-knowing-in-advance) |
| The miss count resets on every power-up | Reports sent while the cart was parked were never missed and must not be counted | [§8](#what-the-yellow-count-means) |
| An outage of about 10.7 days can read as healthy | The sequence number is a single byte and wraps at 256 | [§8](#what-the-yellow-count-means) |
| `SILENT` rarely appears on carts fitted with a GCI | The display sleeps with the ignition, so the 24-hour listening timer restarts | [§8](#about-the-red-silent-state) |
| A lost report costs up to an hour of delay | Transmissions are best-effort with no retry; the hourly repeat is the recovery | [§3](#3-how-the-sensor-talks-to-the-cart) |
| The sensor gives no confirmation that pairing succeeded | It is transmit-only; the confirmation is on the display | [§6](#why-there-is-no-confirmation-on-the-sensor) |
| The sensor cannot be un-paired from the sensor end | Same reason — un-pairing is done on the display | [§10](#10-un-pairing-and-replacing-a-sensor) |

---

*Golf Cart Computer System — Hands-On Tech*
