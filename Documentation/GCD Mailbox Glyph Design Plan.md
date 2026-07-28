# GCD — Mailbox Available Glyph (`lbl_mail_available`)

**Design record — implemented**
Design date: 2026-07-26 (rev 3) · Built and verified: 2026-07-27 · Codebase: **GCD** · Related: **GCM** (channel config), **mailbox sensor** (firmware dependency)

> ## ✅ Status: BUILT AND VERIFIED ON HARDWARE — 2026-07-27
>
> All phases are implemented and confirmed working on the CYD with hand-sent frames from a
> Meshtastic phone client. Verified end to end: pair offer received → accepted on Settings 2 →
> persisted across a screen change → `NORM` frame lights the Home glyph with one chime → tap
> dismisses → re-assertion does not re-light → `ABSENT`/`PRESENT` cycles correctly.
>
> **Still open, both outside the GCD:**
> - **O-4** — mailbox sensor firmware: a pair button emitting one `mode=PAIR` frame per press.
> - **O-7** — GCS User Manual: screen captures and pairing instructions. Now unblocked.
>
> **Read §12 before changing any of this** — it records where the as-built differs from the plan
> below, which is otherwise left in its original forward-looking form.
>
> ### 🔨 Rev 4 is built, pending hardware verification — see §13
>
> A UI revision to make mailbox pairing consistent with the GCI pairing row above it: fixed
> `PAIR MBX` button label, plus a separate colour-coded `lbl_mailbox_id_str` value string carrying
> sensor health — **white** awaiting, **green** healthy, **yellow (N)** frames missed, **red**
> silent 24 h. Misses are counted from the frame's `seq` field rather than elapsed time, because the
> sensor gates transmissions overnight and any short time-based rule would alarm every morning.
>
> Rev 4 also **changes two rev-3 behaviours**, so §3.2 and §3.4 above have been corrected in place
> rather than left stale: the glyph is dismissed by **long press**, and a dismiss is now a **snooze**
> that lapses at the sensor's next PRESENT frame instead of persisting until a state edge.

> **Revision history**
> - **rev 1** — pair by tapping an `<MBX …>` row under a new MBX chat filter.
> - **rev 2** — pair by a GCD-side 60 s listen window (`mode=PAIR`). Removed the MBX filter.
> - **rev 3** — corrects rev 2's framing: the sensor is **transmit-only**, so there is no handshake
>   and no GCD-side window to coordinate. The **sensor owns the offer window**; the GCD simply holds
>   the most recent fresh offer and lets the user **accept** it. Simpler than rev 2 — no listen-window
>   state machine, no ambiguity heuristic, and the user *sees the id before claiming it*. See §3.3.

---

## 1. Context

A battery-powered mailbox sensor (ATtiny1614 + XIAO running stock Meshtastic) broadcasts the state
of a postal mailbox over the mesh. The GCD should light a glyph on the Home screen when mail is
waiting and extinguish it when the mail is collected.

Five constraints shape the design:

1. **Heap.** The GCD has a long history of LVGL out-of-memory crashes. The feature must add
   essentially zero heap and no lazily-created LVGL objects.
2. **Sensor selection.** The message is a *broadcast* — every mailbox on the shared channel is heard
   by every cart. The GCD must respond only to *one* mailbox. A hex keypad for manual entry is not
   affordable (that is why `hex_entry` was deleted from the project in the first place).
3. **Sensor identification.** `mbx_id` is a CRC-32 of the ATtiny serial. The sensor has **no display,
   no label, and no indicator**. The owner has no way to recognise their own id in a stream of
   neighbourhood mailbox traffic. *This is the problem rev 2/3 exist to solve.*
4. **The sensor is transmit-only.** It never receives. There is no ACK, no negotiation, and no way
   for the GCD to tell the sensor it has been claimed. Pairing is therefore **one-directional
   consent**: the sensor offers, the GCD accepts privately, and the sensor never knows.
5. **User override.** If the sensor misbehaves, the driver must be able to kill the glyph without a
   reboot.

**Intended outcome:** a lit glyph means *"there is mail in my mailbox"*, it costs ~50 bytes of RAM
and no ongoing heap, and the mailbox is claimed by pressing a button on the sensor and then
confirming the id that appears on the GCD.

### 1.1 Wire format (DDR02a §5, plus one addition)

The sensor sends a **HoT packet, type 03**, as a Meshtastic `TEXT_MESSAGE_APP` **broadcast**:

```
|#03#NORM#a1b2c3d4#5#87#142#PRESENT
 │   │    │        │ │  │   └── state       PRESENT | ABSENT      <-- used
 │   │    │        │ │  └────── tof_mm      (ignored)
 │   │    │        │ └───────── battery_pct (ignored)
 │   │    │        └─────────── seq         0-255 wrapping (ignored)
 │   │    └──────────────────── mbx_id      8 hex, CRC-32 of sensor serial  <-- used
 │   └───────────────────────── mode        NORM | DEV | PAIR      <-- used
 └───────────────────────────── hot_pkt_id  03 = mailbox
```

**`PAIR` is a third value of the existing `mode` field.** No new packet type, no new field, no change
to field order or count. In a `PAIR` frame **only `mode` and `mbx_id` are meaningful** — seq,
battery, tof and state may carry anything and the GCD ignores them. Any mode value the GCD does not
recognise is treated as `NORM`, so this is forward-compatible in both directions.

Behaviour: state frames are sent on every ABSENT↔PRESENT **edge** and **re-asserted hourly**.
Best-effort — no ACK, no retransmit. A lost edge self-heals at the next hourly re-assertion, **and
that property is what makes the glyph latch work** (see §3.2).

---

## 2. What already exists (nothing here needs building)

| Piece | Where | Status |
|---|---|---|
| HoT packet sniff + type dispatch | [hot_packet_parser.cpp:8-44](src/communication/hot_packet_parser.cpp#L8-L44) | Works; add `case 3` |
| HoT dispatch is **filter-independent** | [meshtastic_callback_task.cpp:53-55](src/tasks/meshtastic_callback_task.cpp#L53-L55) | `processHotPacket()` is called unconditionally, so the glyph and the pair offer update no matter what chat filter is selected |
| `HOT_PKT_HEADER_OFFSET 5` | [config.h:71](src/config.h#L71) | Body starts at the `mode` field — exactly right |
| Chat abbreviation (`[WX_BCAST]`, `<GC>`) | [chat_buffer.cpp:41-93](src/communication/chat_buffer.cpp#L41-L93) | Add an `03` case (diagnostics only) |
| HoT rows are **DEBUG-only** | [chat_buffer.cpp:29-38](src/communication/chat_buffer.cpp#L29-L38) — ALL/CH0-2 return `!isHot` | Confirms "DEBUG unchanged" needs **zero** filter work |
| Pair button + child status label | `objects.btn_send_mac_to_espnow` / `objects.lbl_pair_gci` on settings2 @ (231,45) | The widget pattern we clone (the ESP-NOW *protocol* is not a template — see §3.3) |
| Button click cb + child-label write from C | [settings2_screen.cpp:61-68](src/ui/settings2_screen.cpp#L61-L68) (`btn_fuel_sensor_type` / `lbl_fuel_sensor_type`) | No EEZ variable needed for the pair control |
| `settings2ScreenPump()` runs every GUI loop | [gui_task.cpp:162](src/tasks/gui_task.cpp#L162) — unconditional, not gated on active screen | Offer expiry is evaluated wherever we like |
| `settings2ScreenOnExit()` NVM flush | [gui_task.cpp:187-189](src/tasks/gui_task.cpp#L187-L189) | Reuse for the `mbx_id` write |
| Boolean EEZ var pattern (`headlights_on`) | [get_set_vars.cpp:744-750](src/get_set_vars.cpp#L744-L750) | Template for `mail_available` |
| NVS write path (≤15-char keys) | [preferences_manager.cpp:130-160](src/storage/preferences_manager.cpp#L130-L160) | Key `"mbx_id"` = 6 chars |
| `objects.lbl_mail_available` | `src/ui_eez/screens.c:786-796` @ (79,197), font `ui_font_cart_30`, `0xffffd500` | **Object exists**, but its EEZ Text expression currently evaluates to `""` — see Phase 0 |

**Budget check.** Flash is *not* the constraint people assume: `huge_app.csv` app0 = 3,145,728 B,
current `firmware.bin` = 2,021,584 B → **1.12 MB free (35.7%)**. This feature is ~1.2 KB.
RAM cost is ~50 bytes of `.bss`. **Heap: ~350 B once at boot, then nothing.**

---

## 3. Design

### 3.1 Data flow

```
mailbox sensor  --LoRa broadcast-->  GCM  --UART2-->  text_message_callback()
  (TX only; never hears us)                                 |
                                              meshtasticCallbackQueue (30 deep)
                                                            |
                                          meshtasticCallbackTask  [core 1]
                                             |                        |
                                   chatBufferAppend()          processHotPacket()
                                   (DEBUG filter only;          (ALWAYS runs)
                                    "<MBX_PAIR a1b2c3d4>"             |
                                     / "<MBX a1b2c3d4 P>")      case HOT_PACKET_MAILBOX
                                             |                        |
                                    diagnostics only    mailboxOnFrame(id, present, isPairOffer)
                                    (read-only)                       |
                                                        +-------------+-------------+
                                                        |                           |
                                          s_offerId / s_offerMs        s_mailPresent / s_stateEpoch
                                          (latest fresh offer)                      |
                                                        |                  get_var_mail_available()
                                    settings2: "PAIR MBX a1b2c3d4"                  |
                                             user taps = accept          tick_screen_home() [core 0]
                                                        |                           |
                                       s_pairedMbxId -> NVS "mbx_id"   lbl_mail_available "N" / ""
```

### 3.2 Glyph state machine — epoch latch (no mutex, no race)

The naive approach (`bool present` + `bool dismissed`) has a **lost-update race**: the parser task
clears `dismissed` while the GUI task sets it. Instead, use two counters with **exactly one writer
each** — lock-free and correct on the ESP32's two cores:

```c
static volatile bool     s_mailPresent;   // writer: parser task only
static volatile uint32_t s_stateEpoch;    // writer: parser task only — bumped ONLY on a state edge
static volatile uint32_t s_dismissEpoch;  // writer: GUI task only
```

> ⚠️ **Rev 4 changed the bump rule.** The original design bumped the epoch *only* on a state edge, so
> a dismiss survived the sensor's hourly re-assertion indefinitely. Rev 4 makes dismiss a **snooze**
> instead: the epoch bumps on **every PRESENT frame**, so the glyph returns the next time the sensor
> reports mail still waiting. The mail has not gone anywhere, so the reminder should not be
> permanently dismissible. The table below reflects rev 4.

```c
glyph_on   =  s_mailPresent && (s_dismissEpoch != s_stateEpoch)
dismiss()  =  s_dismissEpoch = s_stateEpoch          // GUI task
on_frame() =  if (present != s_mailPresent) { s_mailPresent = present; s_stateEpoch++;
                                              if (present) tone_alert(); }
              else if (present)             { s_stateEpoch++; }   // re-assertion, silent
```

Why this is the right shape:

| Scenario | Result |
|---|---|
| Mail arrives (ABSENT→PRESENT edge) | epoch bumps, `dismissEpoch` no longer matches → **glyph on** + chime ✅ |
| Driver long-presses to dismiss | `dismissEpoch = stateEpoch` → **glyph off** ✅ |
| Hourly PRESENT re-assertion after a dismiss | epoch bumps → **glyph returns, silently** ✅ — the snooze lapses because the mail is still there |
| Mail collected (PRESENT→ABSENT) | `s_mailPresent=false` → **glyph off**, latch reset for next time ✅ |
| Both the ABSENT and the next PRESENT edge frames are lost | next hourly re-assertion carries the *current* state → bump → **self-corrects** ✅ |

The chime deliberately stays on the rising edge only. Sounding it on every re-assertion would nag
hourly for mail the driver has already acknowledged, which is the opposite of what dismissing is for.

**The epoch pair is still required even though the rule is now simpler.** A plain `bool dismissed`
would have two writers — the parser clearing it and the GUI setting it — and that read-modify-write
race would occasionally swallow a dismiss.

Locking: none. ESP32 LX6 cores share DRAM with hardware coherency; naturally-aligned 32-bit and
single-byte loads/stores are atomic, so tearing is impossible. `volatile` prevents the GUI task's
poll from being hoisted. Precedent in this repo: `new_rx_data_flag` and `headlights_on` are already
plain cross-task flags; `hotPacketActiveBufferWx/Np` are already `volatile` publish words.

**Single-writer discipline extends to pairing.** The parser task writes only `s_offerId` /
`s_offerMs`; the GUI task is the only writer of `s_pairedMbxId`. Every shared word in this feature
has exactly one writer — no exceptions, no "benign" races to reason about.

> ⚠️ **`get_var_mail_available()` must not touch a mutex or a queue.** `ui_init()` at
> [main.cpp:175](src/main.cpp#L175) polls every getter, but the mutexes and queues are not created
> until [main.cpp:208-217](src/main.cpp#L208-L217). The design above satisfies this by construction.
> Likewise `mailboxLoad()` must read `prefs` directly and must **never** call
> `queuePreferenceWrite()` — it runs from `loadPreferences()` at [main.cpp:117](src/main.cpp#L117),
> long before `eepromWriteQueue` exists.

### 3.3 Pairing — offer / accept (rev 3)

**The problem.** Rev 1 asked the user to tap the row showing their `mbx_id` in a list of mailbox
traffic. That only works if the user knows their id — and nothing on the sensor tells them. On a
street with two sensors it is a coin flip, and a wrong pair fails *silently and slowly*: the glyph
reports someone else's mail for weeks.

**Why this is not ESP-NOW pairing.** The GCI handshake works because both ends transmit: the GCD
broadcasts a request and waits for an ACK. The mailbox sensor **never receives**, so there is nothing
to negotiate. What is left is one-directional: the sensor *offers* its identity, and the GCD *accepts*
it locally. The sensor is never told, and does not care — a second cart can accept the same offer,
which is a feature (two drivers, one mailbox), not a conflict.

**Who owns the window.** The sensor does. Rev 2 had the GCD open a 60 s listen window that the user
had to line up with the sensor's window — two timers to coordinate, for no benefit. In rev 3 there is
**only one window**, and it starts when the user presses the button on the sensor:

```
  user presses PAIR on the sensor
        │
        ├── sensor sends |#03#PAIR#a1b2c3d4#…   (one frame; press again to retry)
        │
        ▼
  GCD parser latches   s_offerId = a1b2c3d4 ,  s_offerMs = millis()
        │              (fresh for MBX_OFFER_TTL_MS = 45 s after the LAST frame heard)
        ▼
  Settings 2 button reads   PAIR MBX a1b2c3d4  ← stays live while the sensor keeps offering
        │
        ├── short click  → accept: s_pairedMbxId = a1b2c3d4 , write NVS, chime
        └── ignore       → offer goes stale and the button reverts. Nothing was claimed.
```

**The TTL runs from the last frame heard.** With one frame per button press (O-4), that means the
driver has 45 s from the press to see the offer and accept it — comfortable when Settings 2 is
already open, which is why that is the documented order. If the sensor is ever changed to send
several frames per press, each one simply re-latches the same id and extends the window; the GCD
needs no change for that.

**Why this shape is better than rev 2:**

- **No coordination.** Press the sensor button, then walk to the cart. There is no 60 s race.
- **Self-verifying.** The user *sees the id before claiming it*, and knows it is theirs because they
  just pressed the button on their own sensor. Rev 2 adopted blind, which is why it needed an
  `AMBIGUOUS` abort heuristic. Rev 3 does not need one: the accept is an explicit human confirmation,
  and if a neighbour's offer happens to overwrite the pending one, the button visibly shows a
  different id and the user simply re-presses their sensor.
- **No state machine.** There is no listen window to open, cancel, time out or restore. An offer is
  two words that go stale on their own.
- **Simpler sensor.** No mode to enter and leave, and no repeat window — a single frame per button
  press is all the firmware owes the GCD (O-4).

**Freshness comes from module state, not from the chat row.** Deliberately: `chatMessage_t.timestamp`
is `time(NULL)`, which is meaningless before GPS lock — and it fails in the *unsafe* direction, since
an unset clock makes every stored row look recent. Adding a `millis` field to `chatMessage_t` is also
not free: that struct is the one persisted by `saveDmsToNvs` / `loadDmsFromNvs`, so changing its
layout would invalidate or misparse stored DMs. Two module-level words in `mailbox.cpp` cost 8 bytes
and have neither problem.

**Button behaviour** (Settings 2, near the existing `PAIR GCI` button):

| Condition | Label | Short click |
|---|---|---|
| No fresh offer, unpaired | `NO MAILBOX` | — (no-op) |
| No fresh offer, paired | `MBX a1b2c3d4` | — (no-op) |
| Fresh offer, id ≠ paired | `PAIR MBX a1b2c3d4` | **accept** → chime, label becomes `MBX a1b2c3d4` |
| Fresh offer, id == paired | `MBX a1b2c3d4 RX` | — (already yours; confirms the sensor is being heard right now) |
| Any | — | **long press → forget** (`mbx_id = 0`, feature off) |

Longest string is `PAIR MBX a1b2c3d4` — **17 characters**, which sets the label width in Phase 0 and
the `char buf[20]` in Phase 4. All ASCII: no `LV_SYMBOL_*` glyph, so this works in
`lv_font_montserrat_14` with no font change.

> **Use `LV_EVENT_SHORT_CLICKED`, not `LV_EVENT_CLICKED`, for the short-click handler.** LVGL 9 fires
> `LV_EVENT_CLICKED` on release *regardless of press duration*, so a long press would fire both
> handlers and re-accept the pending offer immediately after forgetting it. `LV_EVENT_SHORT_CLICKED`
> is suppressed when the press exceeded the long-press threshold. (The chat-row pair at
> [chat_screen.cpp:371-372](src/ui/chat_screen.cpp#L371-L372) does not exercise this, because its
> long-press handler navigates away.)

**Rejected alternative — tap the `<MBX_PAIR …>` row under DEBUG (D-8).** Mechanically it would work
with the same two words of state, and the freshness gate would make stale rows inert while scrolling
exactly as intended. It was declined because it adds interaction code to `chat_screen.cpp` — this
project's worst crash history — collides with the existing left-tap-favourite / right-tap-marquee row
semantics, and buries the only pairing affordance in a diagnostics view. The `<MBX_PAIR …>` rows stay
under DEBUG as **read-only** confirmation that the offer was heard.

**What rev 2/3 delete from rev 1.** No `CHAT_FILTER_MBX`, no filter renumbering, no
`chatMessage_t::isMbx`, no `matchesFilter()` signature change, no `mesh_filter` NVS-meaning risk, no
filter-on-input caveat. `chat_screen.cpp`, `chat_buffer.h` and `chatMessage_t` are untouched.

**What is kept.** `chatAbbreviate()` renders type-03 as **`<MBX_PAIR a1b2c3d4>`** for pair offers and
**`<MBX a1b2c3d4 P>`** (`P`/`A` = PRESENT/ABSENT) for state frames. Diagnostics only — under DEBUG
this is how you confirm the GCM is hearing the sensor at all, and how you read the id back to check
it against the button.

> The id must be extracted with the `hotField()` helper (Phase 1), **not** a fixed offset — the `mode`
> field is 3-4 characters (`DEV` vs `NORM`/`PAIR`), so the id does not sit at a constant index.
> *(Rev 1 had `&src[5]` here, which pointed at the mode field. Fixed.)*

### 3.4 Long-press-to-dismiss on the glyph

Make the existing label clickable from C — **no new EEZ widget, no new LVGL object**:

```c
lv_obj_add_flag(objects.lbl_mail_available, LV_OBJ_FLAG_CLICKABLE);
lv_obj_set_ext_click_area(objects.lbl_mail_available, 8);   // ~40x46 px target
lv_obj_add_event_cb(objects.lbl_mail_available, mailGlyphLongPressedCb,
                    LV_EVENT_LONG_PRESSED, nullptr);
```

> ⚠️ **Rev 4: long press, not tap.** The glyph sits in the middle of the Home screen where stray taps
> are easy, and dismissing is the one action there that discards information. It uses the global
> `LONG_PRESS_TIME_MS` (§13.7), so it matches every other long press in the UI.

Verified safe:
- `create_screens()` runs once from `ui_init()`; objects are never destroyed, so a callback attached
  after `ui_init()` lives for the process lifetime. Same mechanism as
  [settings2_screen.cpp:63](src/ui/settings2_screen.cpp#L63).
- EEZ's `tick_screen_home()` only calls `lv_label_set_text()` on the object. Nothing in the EEZ
  output removes event callbacks, so re-export cannot break the handler.
- The Home screen's swipe gesture still works: `LV_OBJ_FLAG_GESTURE_BUBBLE` is set by default and
  must be **left alone**; a swipe starting on the label resolves up to `objects.home`.
- `lv_event_stop_bubbling()` is *not* needed — `LV_OBJ_FLAG_EVENT_BUBBLE` is off by default.
- No overlap with neighbours: label spans x 79-103; `btn_goto_messages` is x 35-65,
  `btn_goto_maintenance` x 223-253, `bar_fuel_level` y 177-185.
- The label is `LV_SIZE_CONTENT`, so when the glyph is dark the hit area is **zero** — you cannot
  dismiss a glyph that is already off. That is desirable, but guard the callback anyway
  (`if (!mailboxGlyphOn()) return;`).

### 3.5 Chimes

`tone_alert()` on the **ABSENT→PRESENT edge only** (i.e. inside the epoch-bump branch, when
`present == true`). Not on the hourly re-assertion, not on ABSENT. Calling a tone from
`meshtasticCallbackTask` is already established — see
[meshtastic_callback_task.cpp:44](src/tasks/meshtastic_callback_task.cpp#L44).

A second, distinct chime (`tone_message()`) fires from the GUI task when the user **accepts** an
offer, and `tone_confirm()` when a long press **forgets** one — an 800 ms hold is long enough that
without a cue you would not know when to let go.

No chime when a snoozed glyph returns at the next re-assertion (§3.2).

---

## 4. Cost

| | |
|---|---|
| **Heap** | ~**350 B**, allocated once in `create_screens()` at boot (pair button + child label). Nothing lazy, nothing per-message. Plus one `lv_event_dsc_t` slot (~12 B) for the glyph. |
| **DRAM `.bss`** | ~50 B (paired id, offer id, offer timestamp, present, 2 epochs, label cache + timer) |
| **`chatMessage_t`** | **unchanged** — no new field, so the `saveDmsToNvs` blob layout is untouched |
| **Flash** | ~1.2 KB (parser + helpers ~330 B, `mailbox.cpp` ~320 B, pair UI ~330 B, glyph wiring ~120 B, strings + EEZ assets ~150 B) |
| **NVS** | 1 key: `"mbx_id"` (6 chars, well inside the 15-char limit) |
| **New tasks / queues / mutexes** | none |
| **Files rev 3 does not touch** | `chat_screen.cpp`, `chat_buffer.h`, `types.h::chatMessage_t` |

**Measured after the build (2026-07-27):** RAM **23.4%** (76,780 of 327,680 B) · Flash **64.1%**
(2,017,537 of 3,145,728 B — **1.13 MB free**). RAM was unchanged to the byte by the last three
increments, and flash tracked the estimate. Note the totals include the Settings/Settings 2 screen
rework, so they are not a clean before/after for this feature alone.

---

## 5. Implementation

### Phase 0 — EEZ Studio (you, before any C work)

Project: `C:\Users\Yang\Documents\eez-projects\Golf-Cart-DIsplay-CYD-2432S028R-Flow\`

1. Add native variable **`mail_available`**, type `boolean`.
2. Set `lbl_mail_available` **Text** expression to `mail_available ? "N" : ""`.
   *(Today it evaluates to `""`, so the glyph is permanently dark.)*
   `'N'` = 78, inside `ui_font_cart_30`'s cmap range 64-91 — confirmed renderable.
3. On **settings2**, add a button **`btn_pair_mailbox`** with a child label **`lbl_pair_mbx`**,
   cloned from `btn_send_mac_to_espnow` / `lbl_pair_gci`.
   **No flow action and no variable binding** — C owns both the click and the label text, exactly as
   it does for `btn_fuel_sensor_type` / `lbl_fuel_sensor_type`.
   **Placement is yours to settle in the editor** (D-6) — only the object *names* bind to the C code,
   so moving the row, or moving it to another screen, costs nothing downstream. Starting suggestion:
   settings2 at **y ≈ 162**, the one clear band (occupied rows are y 7, 45-60, 67-85, 90-105,
   108-130, 134-160, 184-200, 210-228). Size the label for **17 characters** — `PAIR MBX a1b2c3d4`
   is the longest state.
   > If you end up moving it to a screen *other than* settings2, the Phase 4 code moves with it:
   > a new `src/ui/<screen>_screen.cpp` with init/pump/exit, plus registrations in
   > [gui_task.cpp:160-163](src/tasks/gui_task.cpp#L160-L163) and the exit-hook block at
   > [:187](src/tasks/gui_task.cpp#L187). Staying on settings2 reuses all three for free.
4. Export/sync → regenerates `src/ui_eez/{screens.c,screens.h,vars.h,ui.c}`.

Nothing below is useful until this is done. Per CLAUDE.md §2, `src/ui_eez/` is **never** hand-edited.

### Phase 1 — Receive + parse (buildable and serial-testable with no UI)

**`src/types.h`** — add `HOT_PACKET_MAILBOX = 3` to `enum HotPacketType`
([types.h:64-66](src/types.h#L64-L66); nothing else uses 3). **No `chatMessage_t` change.**

**`src/communication/mailbox.h` / `mailbox.cpp`** *(new)*

```c
void     mailboxLoad(void);            // from loadPreferences(); prefs.getInt only, no queue
void     mailboxOnFrame(uint32_t mbxId, bool present, bool isPairOffer);  // parser task
bool     mailboxGlyphOn(void);         // GUI task / EEZ getter — no locks
void     mailboxDismiss(void);         // GUI task
uint32_t mailboxGetPairedId(void);
uint32_t mailboxFreshOfferId(void);    // GUI task — 0 if none, or the offer has gone stale
bool     mailboxAcceptOffer(void);     // GUI task — claim the fresh offer; true if it changed
void     mailboxForget(void);          // GUI task — id = 0, clears glyph state
void     mailboxStatusText(char *dst, size_t n);  // GUI task — button label text
```

State (all single-writer, per §3.2):

```c
#define MBX_OFFER_TTL_MS  45000UL   // claimable for 45 s after the LAST offer frame heard
                                    // one frame per sensor button press — this is the whole budget

static uint32_t s_pairedMbxId;               // NVS "mbx_id"; 0 = off     writer: GUI
static volatile uint32_t s_offerId;          // latest pair offer          writer: parser
static volatile uint32_t s_offerMs;          // millis() of that offer     writer: parser
```

`mailboxOnFrame()`:
1. If `isPairOffer` — `s_offerId = mbxId; s_offerMs = millis();` (latest offer always wins) and
   return. A `PAIR` frame carries no trustworthy state fields, so it never touches the glyph.
2. If `mbxId != s_pairedMbxId || s_pairedMbxId == 0`, return.
3. Apply the epoch rule from §3.2; fire `tone_alert()` on a rising edge.

`mailboxFreshOfferId()` returns `s_offerId` only while `millis() - s_offerMs < MBX_OFFER_TTL_MS`
(and `s_offerId != 0`). `millis()` wraps at ~49 days; unsigned subtraction handles that correctly.

`mailboxForget()` and `mailboxAcceptOffer()` must both clear stale glyph state
(`s_mailPresent = false; s_dismissEpoch = s_stateEpoch;`) — otherwise re-pairing while the *old*
mailbox was PRESENT leaves the glyph lit for up to an hour.

**`src/communication/hot_packet_parser.cpp`** — two file-static helpers plus one switch case. The
parser is `const char*` throughout: **it must not mutate the buffer** (the weather branch does, via
`parseWeatherData((char*)text,…)` at [:51](src/communication/hot_packet_parser.cpp#L51)).

```c
// Pointer to '#'-delimited field `idx` (0-based) of s; length into *outLen.
// NULL if absent/empty/absurd. Tolerates a missing trailing '#'. Never modifies input.
static const char *hotField(const char *s, uint8_t idx, uint8_t *outLen);

// Exactly 8 hex digits -> uint32. Case-insensitive. false otherwise.
static bool parseHex8(const char *p, uint8_t len, uint32_t *out);
```

```c
case HOT_PACKET_MAILBOX: {
    // |#03#NORM#a1b2c3d4#5#87#142#PRESENT      (mode also: DEV | PAIR)
    const char *body = &text[HOT_PKT_HEADER_OFFSET];   // "NORM#a1b2c3d4#..."
    uint8_t mdLen = 0, idLen = 0, stLen = 0;
    const char *mdp = hotField(body, 0, &mdLen);       // mode
    const char *idp = hotField(body, 1, &idLen);       // mbx_id
    uint32_t mbxId = 0;
    if (!idp || !parseHex8(idp, idLen, &mbxId)) { Serial.println("MBX malformed"); break; }

    // A PAIR frame carries only mode + mbx_id; every other field is ignored by contract.
    if (mdp && mdLen == 4 && memcmp(mdp, "PAIR", 4) == 0) {
        mailboxOnFrame(mbxId, false, true);
        break;
    }

    const char *stp = hotField(body, 5, &stLen);       // state
    bool present;
    if      (stp && stLen == 7 && memcmp(stp, "PRESENT", 7) == 0) present = true;
    else if (stp && stLen == 6 && memcmp(stp, "ABSENT",  6) == 0) present = false;
    else { Serial.println("MBX bad state"); break; }
    // Unknown modes (e.g. DEV) fall through as NORM — forward-compatible in both directions.
    mailboxOnFrame(mbxId, present, false);
    break;
}
```

Short/truncated input is rejected cleanly — `hotField()` returns NULL at the first missing `#`.
`text` is always NUL-terminated by
[meshtastic_callback_task.cpp:80-84](src/tasks/meshtastic_callback_task.cpp#L80-L84), so the
`strchr`/`strlen` scans cannot run away.

**`src/communication/chat_buffer.cpp`** — in `chatAbbreviate()`
([:48-57](src/communication/chat_buffer.cpp#L48-L57)), add the `03` case *before* the
`[HoT_PACKET]` fallback: `<MBX_PAIR a1b2c3d4>` when mode is `PAIR`, else `<MBX a1b2c3d4 P>`. Use
`hotField()` (export it from the parser header, or duplicate it as a 12-line static). Fall back to
`[HoT_PACKET]` if the frame does not parse. **No other change to this file, and none at all to
`chat_buffer.h`.**

**`src/storage/preferences_manager.cpp`** — call `mailboxLoad()` from `loadPreferences()` (near the
`mesh_filter` line, ~:97).

*Checkpoint: `pio run` compiles; with `DEBUG_GCM_MESSAGES 1` you can watch frames arrive over serial
before any UI exists.*

### Phase 2 — Glyph live

**`src/get_set_vars.h` / `.cpp`**

```c
bool get_var_mail_available() { return mailboxGlyphOn(); }
void set_var_mail_available(bool) { }   // no-op — mirror set_var_num_unread_direct_msgs()
```

No new `extern` global; the state lives in `mailbox.cpp`. The setter must stay a no-op so a stray
EEZ flow assignment can never fight the parser.

### Phase 3 — Dismiss tap

**`src/ui/home_screen.h` / `home_screen.cpp`** *(new, ~25 lines)* — `homeScreenInit()` per §3.4,
null-guarding `objects.lbl_mail_available` the way
[settings2_screen.cpp:61](src/ui/settings2_screen.cpp#L61) does.
Call it from `main.cpp` after `settings2ScreenInit()` ([main.cpp:187](src/main.cpp#L187)), followed
by a `HEAP_LOG(...)` to match house style.

### Phase 4 — Accept UI (all inside `src/ui/settings2_screen.cpp`)

No new UI module, no new pump registration, no new exit hook — the mailbox button lives on a screen
that already has all three.

**`settings2ScreenInit()`** — alongside the existing `btn_fuel_sensor_type` wiring at
[:61-68](src/ui/settings2_screen.cpp#L61-L68):

```c
if (objects.btn_pair_mailbox) {
    lv_obj_add_event_cb(objects.btn_pair_mailbox, mbxAcceptCb, LV_EVENT_SHORT_CLICKED, nullptr);
    lv_obj_add_event_cb(objects.btn_pair_mailbox, mbxForgetCb, LV_EVENT_LONG_PRESSED,  nullptr);
}
```

`mbxAcceptCb` → `if (mailboxAcceptOffer()) { tone_message(); s_mbxWritePendingMs = millis(); }`
`mbxForgetCb` → `mailboxForget(); s_mbxWritePendingMs = millis();`

**`settings2ScreenPump()`** — two additions:

1. Commit `queuePreferenceWrite("mbx_id", (int)mailboxGetPairedId())` after `CYCLE_WRITE_DEBOUNCE_MS`
   (3 s) — mirroring `s_fuelWritePendingMs` exactly.
2. **Gated label refresh**, at most 1 Hz *and* only while settings2 is the active screen:

```c
if (lv_scr_act() == objects.settings2 && (now - s_mbxLabelMs) >= 1000) {
    s_mbxLabelMs = now;
    char buf[20];
    mailboxStatusText(buf, sizeof buf);
    if (strcmp(buf, s_mbxLabelCache) != 0) {          // only write on real change
        strcpy(s_mbxLabelCache, buf);
        lv_label_set_text(objects.lbl_pair_mbx, buf);
    }
}
```

> Both gates matter. `lv_label_set_text()` invalidates the object, and invalidating an object on an
> inactive screen queues display-dirty work that shows up as sluggish screen transitions — the same
> trap documented for `lv_obj_add_flag(HIDDEN)` in the exit-hook rules. The change-detect gate means
> the label is written only when the offer actually appears, is accepted, or goes stale.

**`settings2ScreenOnExit()`** — flush the `mbx_id` write unconditionally, next to the existing fuel
flush. **Zero LVGL calls in the exit hook**, per house rule.

*Offer expiry needs nothing to run: `mailboxFreshOfferId()` compares against `millis()` on read, so
an unclaimed offer lapses whether or not the GCD is awake, on that screen, or looking.*

---

## 6. Decisions

D-1 … D-5 were confirmed on 2026-07-26; D-6 … D-8 are open.

| # | Decision | Status |
|---|---|---|
| **D-1** | Dismiss hit target: clickable label from C, not an overlay button. | ✅ **Confirmed.** Zero new objects; hit area correctly vanishes when the glyph is dark. |
| **D-2** | *(rev 1: which row zone pairs the mailbox)* | ⛔ **Superseded** by D-8. |
| **D-3** | *(rev 1: MBX filter index 6 vs inserted)* | ⛔ **Moot** — no MBX filter. `CHAT_FILTER_DEBUG` keeps its stored value; no NVS-meaning risk. |
| **D-4** | Glyph character `"N"` in `ui_font_cart_30` (cmap 48-62, 64-91, 93-94, 97-113). | ✅ **Confirmed** — `'N'` = 78, in range. |
| **D-5** | `<MBX …>` rows stay visible under DEBUG. | ✅ **Confirmed** — and free: HoT rows are already DEBUG-only, so this needs no filter code. |
| **D-6** | Pair button placement. | ✅ **Yours to settle in EEZ Studio.** Plan is written against **settings2 @ y≈162**, which reuses the existing pump / exit hook / NVM debounce for free. Only the object names bind to C; if it lands on another screen, Phase 4 moves into a new UI module with its own gui_task registrations (see Phase 0 note). |
| **D-7** | Offer TTL — how long a `PAIR` frame stays claimable after it is heard. | ✅ **Under a minute — use 45 s.** With one frame per button press (O-4), this is the driver's entire budget to see the offer and accept it. Comfortable provided Settings 2 is opened *before* pressing the sensor button — document that order (O-7). One `#define` to change if bench testing says otherwise. |
| **D-8** | Accept surface: Settings 2 button, or tap the fresh `<MBX_PAIR …>` row under DEBUG? | ✅ **Settings 2 button.** `chat_screen.cpp` stays untouched, and the `<MBX_PAIR …>` rows under DEBUG remain read-only diagnostics. |
| **D-9** | Button label wording. | ✅ **`PAIR MBX a1b2c3d4`** for a claimable offer — an id alone was too nebulous. Full state set in §3.3; 17 chars max, ASCII only. |

---

## 7. Open items

- **O-1 — GCM / sensor RF alignment.** ✅ **Closed.** The sensor's **primary** channel will be set to
  `GolfCart`, matching the cart mesh, so the DDR02a §4.2 primary-name-hash slot derivation puts both
  radios on the same frequency.
- **O-2 — filter-on-input softening.** ⛔ **Dissolved** with the MBX filter. *(Your question — how
  does someone reset `mbx_id` for a replacement sensor? — is answered directly by the UI:
  **long-press the button to forget**, or just press the new sensor's pair button and accept the
  offer, which overwrites. Neither requires knowing either id.)*
- **O-4 — sensor firmware dependency (the only thing gating rev 3).** Pair mode requires a **button
  on the mailbox sensor** that emits `mode=PAIR` frames. Only `mode` and `mbx_id` need be valid in
  that frame.

  **One frame per button press is sufficient — no repeat window is required.** Pairing is done with
  the cart and the mailbox in close proximity, so the link is short and effectively lossless, and the
  driver is looking at the screen within seconds of pressing. The GCD holds the offer for 45 s from
  the single frame it heard, which is ample.

  If the frame is lost, nothing appears on the button and **the recovery is to press the sensor
  button again** — obvious, immediate, and requires no firmware state. That is a better failure mode
  than a repeat window, which spends sensor battery on every pairing to insure against a case the user
  can resolve with one more press.

  *(Optional hedge, decide after bench testing: 2-3 frames a few seconds apart per press. Cheap — a
  ~40-byte text packet is ~250 ms of airtime — and invisible to the GCD, which simply re-latches the
  same id. Only worth adding if drops turn out to be common at pairing range. It is **not** a
  prerequisite, and no GCD change is needed either way.)*

  > **Procedure consequence.** Because 45 s now runs from a single press, it has to cover *getting to
  > the screen*. Document the order as **open Settings 2 first, then press the sensor button** — if
  > the display is asleep, waking it and navigating Menu → Settings 2 can consume most of the window.
  > This is an O-7 documentation point, not a code constraint.

  This is a change to the sensor project (DDR02a §5), not to the GCD. **The GCD side is fully
  testable without it** — send a `PAIR` frame by hand from a phone (verification step 3).
- **O-5 — label the id on the sensor (recommended either way; zero firmware cost).** DDR05 computes
  the CRC-32 on-chip during programming; print it over UPDI/serial and put a sticker inside the
  mailbox lid. This does not replace pairing — it makes the accepted id *checkable*, and it is the
  only recovery path if the sensor's pair button ever fails.
- **O-6 — a second cart can accept the same offer.** Not a defect: two drivers sharing one mailbox is
  a legitimate case, and the sensor cannot tell anyone apart anyway. Worth stating so it does not get
  "fixed" later.
- **O-7 — GCS User Manual update (OPEN; now unblocked).** The firmware runs, so the screen captures
  this needs can be taken. This feature adds the first thing on the
  GCD that the driver must *pair* rather than just read, so it needs operator-level documentation in
  `Documentation\GCS User Manual.md`:
  - **New screen captures** — the Home screen with the glyph lit, and Settings 2 showing the pair
    button in its `NO MAILBOX` / `PAIR MBX a1b2c3d4` / `MBX a1b2c3d4` states. Follow the existing
    convention: 640×480 JPGs in `GCD Screens\GCD Screens 640x480\`, referenced from `Documentation\`
    as `../GCD Screens/...`. Captures cannot be taken until the firmware runs, so this trails
    Phase 4.
  - **Pairing procedure** — press the button on the mailbox sensor, walk to the cart, open Settings 2,
    confirm the id shown, tap to accept. Include the time limit and what to do if the offer lapses
    (press the sensor button again).
  - **What the glyph means and how to dismiss it** — tap to extinguish until the next state change,
    and why you might want to (a stuck or dead sensor).
  - **Un-pairing / replacing a sensor** — long-press the button to forget; or pair a new sensor,
    which overwrites.
  - Worth cross-checking whether the **Assembly** and **Software Installation** manuals also need the
    sensor's channel requirement (primary = `GolfCart`, O-1) — that is an installer step, not an
    operator one, so it likely belongs there rather than in the User Manual.
- **O-3 — declined, consequences noted.**
  - *No deep-sleep persistence.* After an ignition cycle the glyph starts dark and self-corrects at
    the next frame (≤ 1 h). Adding `RTC_DATA_ATTR` to the three state words would fix this for 12 B
    of RTC memory and 0 B of DRAM if you change your mind.
  - *No stale-sensor watchdog.* A dead sensor battery leaves the glyph lit indefinitely — but that is
    precisely the case the manual dismiss tap was requested for, so this is coherent.
  - *No RSSI proximity gate on accepting an offer.* A curbside mailbox can be 100 m away; an RSSI
    threshold would reject legitimate offers more often than it would block a neighbour's.
  - ~~*No `DEBUG_MAILBOX` serial flag.* `DEBUG_GCM_MESSAGES` plus the `<MBX …>` rows under DEBUG
    should give enough visibility.~~ **Reversed during bring-up** — `DEBUG_GCM_MESSAGES` is `0` by
    default, so the first test produced no serial output at all and looked like a total failure when
    the parser was in fact working. Pair offers, pair/unpair and glyph state edges now log
    **unconditionally**; they are rare and user-initiated, matching the ESP-NOW
    "always show pairing success" precedent. No new flag was added.

---

## 8. Verification

**Build**

```
pio run                     # compiles; check the RAM/Flash summary against the §4 estimate
```

*(I do not run `--target upload` — flashing the CYD needs wires moved.)*

**Bench, no sensor required.** Send frames as ordinary text messages from any Meshtastic client
(phone app or CLI) on a channel the GCM monitors.

| # | Step | Expected |
|---|---|---|
| 1 | Settings 2 shows the new button, nothing paired | `NO MAILBOX` |
| 2 | Send `\|#03#NORM#a1b2c3d4#5#87#142#PRESENT` (a state frame, not an offer) | Ignored — glyph dark, button still `NO MAILBOX`. Under DEBUG a row `<MBX a1b2c3d4 P>` appears |
| 3 | Send `\|#03#PAIR#a1b2c3d4#0#0#0#ABSENT` | Button becomes `PAIR MBX a1b2c3d4`; DEBUG row reads `<MBX_PAIR a1b2c3d4>` |
| 4 | Short-click the button | Chime; label becomes `MBX a1b2c3d4` |
| 5 | Leave Settings 2, return | Still `MBX a1b2c3d4` (NVS committed after 3 s / on exit) |
| 6 | Send `…#NORM#a1b2c3d4#…#PRESENT` | Home glyph **lights** ("N", yellow, left of the fuel glyph); one alert chime |
| 7 | Send it again (simulates the hourly re-assertion) | Glyph stays on, **no second chime** |
| 8 | Tap the glyph | Glyph **off** |
| 9 | Send `…#PRESENT` again | Glyph **stays off** — the dismiss latch holds through re-assertion |
| 10 | Send `…#ABSENT`, then `…#PRESENT` | Off, then **on** again with a chime |
| 11 | Send `…#NORM#deadbeef#…#PRESENT` | Glyph unchanged; `<MBX deadbeef P>` visible under DEBUG |
| 12 | Send `\|#03#PAIR#deadbeef#…`, then **wait 45 s** without clicking | Button shows `PAIR MBX deadbeef`, then reverts to `MBX a1b2c3d4`. Nothing was claimed; the old pairing is intact (**D-7**) |
| 12a | Send two `PAIR` frames for the same id ~10 s apart | Button stays `PAIR MBX deadbeef`; the second frame simply re-latches and extends the window — no flicker, no double-prompt |
| 13 | Send `\|#03#PAIR#deadbeef#…` and click within the TTL | Now paired to `deadbeef`; frames from `a1b2c3d4` are ignored; glyph state cleared, not stuck lit |
| 14 | Send a `PAIR` for the id already paired | `MBX a1b2c3d4 RX` — confirms reception, click is a no-op |
| 15 | Long-press the button | `NO MAILBOX`; **no** immediate re-accept of a pending offer (this is the `SHORT_CLICKED` check) |
| 16 | Malformed frames: `\|#03#`, `\|#03#PAIR#xyz#…`, `…#142#MAYBE`, `\|#03#DEV#a1b2c3d4#…#PRESENT` | `MBX malformed` / `MBX bad state` on serial; **no crash**; the `DEV` frame is handled as `NORM` |
| 17 | Reboot | Pairing survives; any pending offer is gone; glyph dark until the next frame (expected — see O-3) |
| 18 | Cycle the Messages filter through all 6 positions | Unchanged from today: `DM → ALL → CH 0 → CH 1 → CH 2 → DEBUG`; `<MBX …>` rows appear under DEBUG only |

**Heap regression.** With `DEBUG_HEAP 1`, compare the `[HOME] enter: free=…`, `[MESSAGES] enter: …`
and `[SETUP2] enter: …` markers against a pre-change build. Expect a one-time drop of ~350 B for the
two new EEZ objects and **no ongoing drift** — nothing in this feature allocates after boot.

---

## 9. Files touched

**GCD** (`C:\Users\Yang\Documents\PlatformIO\Projects\GCD-Golf-Cart_Display-Computer`)

| File | Change |
|---|---|
| *(EEZ project, external)* | native var `mail_available`; `lbl_mail_available` Text expression; new `btn_pair_mailbox` + `lbl_pair_mbx` on settings2; re-export |
| [src/types.h](src/types.h) | `HOT_PACKET_MAILBOX = 3` |
| **src/communication/mailbox.h / .cpp** | *new* — epoch latch, offer/accept, forget, status text |
| [src/communication/hot_packet_parser.cpp](src/communication/hot_packet_parser.cpp) | `hotField()`, `parseHex8()`, `case HOT_PACKET_MAILBOX` |
| [src/communication/chat_buffer.cpp](src/communication/chat_buffer.cpp) | `<MBX_PAIR …>` / `<MBX … P>` abbreviation (diagnostics only) |
| [src/get_set_vars.h](src/get_set_vars.h) / [.cpp](src/get_set_vars.cpp) | `get_var_mail_available()` / no-op setter |
| [src/storage/preferences_manager.cpp](src/storage/preferences_manager.cpp) | `mailboxLoad()` in `loadPreferences()` |
| **src/ui/home_screen.h / .cpp** | *new* — glyph click handler |
| [src/ui/settings2_screen.cpp](src/ui/settings2_screen.cpp) | accept / forget callbacks; label refresh + `mbx_id` debounce; exit flush |
| [src/main.cpp](src/main.cpp) | `homeScreenInit()` + `settingsScreenInit()` + `HEAP_LOG` |
| **src/ui/settings_screen.h / .cpp** | *new, not in the plan* — owns the GPS labels that moved to the Settings screen (§12) |
| [src/tasks/gui_task.cpp](src/tasks/gui_task.cpp) | *not in the plan* — `settingsScreenPump()` registration |
| [src/config.h](src/config.h) | *rev 4* — `LONG_PRESS_TIME_MS 800` (§13.7) |
| [src/hardware/display.cpp](src/hardware/display.cpp) | *rev 4* — `lv_indev_set_long_press_time()` in `initTouchscreen()` |

**Not touched (rev 1 would have changed all of these):** `chat_screen.cpp`, `chat_buffer.h`,
`chatMessage_t`, and `get_set_vars.h`'s `mesh_filter` comment. This held — the rev 3 design kept the
riskiest files out of the change entirely.

**GCM** — configuration only: sensor primary channel = `GolfCart` (O-1, settled). No firmware change.

**Mailbox sensor** — firmware: a **pair button** that emits one `mode=PAIR` frame per press
(**O-4**). Programming process: record and label the `mbx_id` (**O-5**).

**Documentation** — `Documentation\GCS User Manual.md` plus new 640×480 captures in
`GCD Screens\GCD Screens 640x480\` (**O-7**). Trails Phase 4, since the captures need running
firmware. Check whether the sensor channel requirement belongs in the Assembly / Software
Installation manuals as an installer step.

**GCI** — not involved.

---

## 10. Revision comparison

| | Rev 1 (row-tap) | Rev 2 (GCD listen window) | **Rev 3 (offer / accept)** |
|---|---|---|---|
| How the user finds their mailbox | Must already know their id — **unsolved** | Blind adopt inside a 60 s window | Presses the sensor button; **sees the id and confirms it** |
| Timing burden | None | Two windows must overlap | **One window, owned by the sensor** |
| Wrong-mailbox failure | Silent, persistent | Needs an `AMBIGUOUS` abort heuristic | Impossible to do silently — accept is an explicit tap on a shown id |
| GCD state machine | — | begin / cancel / timeout / restore | **Two words that go stale on their own** |
| Chat filter work | New filter, 3 bounds edits, `isMbx`, `matchesFilter()` signature, `rowLeftClickedCb()` branch, NVS renumber risk | None | **None** |
| Sensor firmware | None | 5-min mode with enter/leave | **A button that transmits one frame** |
| Files touched | 11 | 9 | **9** |

---

## 11. Housekeeping

The three copies are kept in sync on every revision:
- `C:\Users\Yang\.claude\plans\i-have-created-a-bubbly-reddy.md`
- `Documentation\GCD Mailbox Glyph Design Plan.md` (git-tracked, source of record)
- https://claude.ai/code/artifact/bffbab82-9e52-410f-8c4f-12d6dd7a0691 (printable)

---

## 12. As built — where reality differed from the plan

Everything in §1-§10 stands unless listed here. Read this section first when modifying the feature.

### 12.1 An extra module was required: `settings_screen.cpp`

Not in the plan, and not caused by it. During Phase 0 the Settings 2 screen was rearranged to make
room for the pair button, which moved four GPS objects — `sats_hdop_1`, `lbl_sats_hdop_value`,
`lbl_cur_lat_value`, `lbl_cur_long_value` — onto the **Settings** screen. `settings2_screen.cpp` was
still hiding all of them in `doHideGpsWidgets()` while its reveal sat behind
`if (lv_scr_act() != objects.settings2) return;`.

Two consequences, neither of which produced a compile error:

1. Those labels were hidden at boot and **could never be revealed while Settings was showing**. They
   would appear only after a Settings 2 visit with a GPS fix, then be re-hidden on the next Settings 2
   entry — visibility driven by unrelated navigation.
2. Toggling `HIDDEN` on them from Settings 2's `SCREEN_LOAD_START` invalidated objects on an
   *inactive* screen — the sluggish-transition trap that `settings2ScreenOnExit()` already warns about.

`src/ui/settings_screen.{h,cpp}` now owns those four with the same hide-at-`SCREEN_LOAD_START` /
reveal-on-`fix.valid.location` pattern, registered via `settingsScreenInit()` in `main.cpp` and
`settingsScreenPump()` in `gui_task.cpp`. `settings2_screen.cpp` was trimmed to its own six objects,
so **no code toggles visibility across screens any more**. Verified: the Settings GPS row appears on
fix, and Settings 2 shows no leftover artifacts from the objects that left it.

### 12.2 Pair button geometry, as placed

D-6 left placement to EEZ Studio. As first built: Settings 2 at (167, 75), 140 × 15, label in
`lv_font_montserrat_10` — not the y≈162 / montserrat_14 the plan suggested. *(Superseded by rev 4,
§13: the row was later reshaped to mirror the GCI row — button (80,71) 69×15 montserrat_12 with a
separate value string at (163,70).)*

### 12.3 Added: green button state

Not in the plan. Both PAIR buttons carried a `#2d9628` CHECKED background that nothing ever set.
`btn_pair_mailbox` now takes `LV_STATE_CHECKED` **exactly while an unclaimed offer is pending**, so
the button is green precisely when tapping it does something, and clears itself when the 45 s TTL
lapses. This is driven from C independently of the label text, which also makes it a usable fallback
if the label is ever contested again (§12.4).

### 12.4 Two EEZ bring-up traps — the whole cost of this build

Neither is a code defect; both cost real time and will recur.

**A label cannot be owned by both EEZ and C.** `lbl_pair_mbx` was created with its **Text** property
as an *expression* (evaluating to `""`), which makes EEZ emit a tick block in
`tick_screen_settings2()` that overwrites C's `lv_label_set_text` every tick. Combined with C's
change-detect cache the symptom is distinctive and misleading: **the text appears once, flickers, and
is then permanently blank, and never flickers again.** C writes and caches; EEZ blanks it; C compares
against its cache, sees no change, and never writes again.

> Diagnose with `rg -uu "lv_label_get_text\(objects.<label>\)" src/ui_eez/screens.c` — `src/ui_eez/`
> is gitignored, so plain grep silently returns nothing. A hit means EEZ owns the label. A label whose
> Text is a **literal** gets no tick entry; that is the C-ownable configuration. `lbl_fuel_sensor_type`
> is the known-good C-owned example on this screen; `lbl_gci_mac_addr_value` is the EEZ-owned one.

**A button with no DEFAULT background is invisible.** `btn_pair_mailbox` was briefly exported with
only a CHECKED background, so in its normal state it drew nothing. With the label also blank, the
control vanished entirely — two independent causes stacking, each masking the other. Both PAIR
buttons use `#086ab7` for DEFAULT; match that. When a control does not render at all, check the label
text **and** the default-state background before suspecting the C code.

### 12.5 Verification actually performed

§8 steps 1-10 and 14-17 were exercised with hand-sent frames and passed, including the dismiss latch
holding through a re-assertion and the glyph clearing on `ABSENT`. The glyph lighting also confirmed
the one thing that could not be checked statically: that `lbl_mail_available`'s Text expression is
genuinely bound to `mail_available`. That binding lives in the compressed `assets[]` blob in `ui.c`,
and the generated tick block looks identical whether the expression is bound or a literal `""`.

Not yet exercised: step 12/12a (TTL lapse and re-latch) and the two-mailbox cases, which need either
a second sensor id or patience.

---

## 13. Rev 4 — GCI-consistent pairing UI + sensor health (BUILT)

> **Status: built 2026-07-28, pending hardware verification.** Compiles clean at RAM 23.4% /
> Flash 64.2%. §13.7 records where the as-built differs from the design in §13.1-§13.6.

**Context.** The rev 3 UI packed everything into one button label (`PAIR MBX a1b2c3d4`), which does
not match how GCI pairing presents itself one row above it on the same screen. Rev 4 splits it into
the GCI shape — a fixed-label button plus a separate colour-coded value string — and uses the colour
to carry sensor health, which rev 3 had no way to show at all.

### 13.1 The GCI model, precisely

`lbl_gci_mac_addr_value` is **EEZ-owned for text, C-owned for colour**: it has a tick block, *and*
`updateEspnowGciMacColor()` ([gui_task.cpp:25-56](src/tasks/gui_task.cpp#L25-L56)) sets its colour
from C (green `#00ff2d` connected, red `#ff0000` disconnected). Text and colour are separate
properties, so there is no conflict.

Rev 4 mirrors that exactly, which also **permanently removes the label-ownership hazard of §12.4**:
C never writes this label's text, so no future re-export can reintroduce the fight.

Layout is already in place and mirrors the GCI row:

| | GCI row (y 45) | Mailbox row (y ~70) |
|---|---|---|
| Button | `btn_send_mac_to_espnow` (79,45) 68×15, montserrat_12 | `btn_pair_mailbox` (80,71) 69×15, montserrat_12 |
| Value | `lbl_gci_mac_addr_value` (163,45) montserrat_14 | `lbl_mailbox_id_str` (163,70) montserrat_14 |

From x=163 there is ~157 px, about 18 characters at montserrat_14 — `a1b2c3d4 OFFERED` (16) fits.

### 13.2 Why health is measured by `seq`, not elapsed time

The sensor **gates transmissions overnight** (DDR04). A legitimate silence is therefore 8-12 hours,
so any "not heard in N minutes → yellow" rule alarms every morning — every day, in a NO_GCI install
where the GCD is always on. Elapsed time is unusable as a health signal without encoding the sensor's
transmit window into the GCD, which would couple the two firmwares.

`seq` (field 2, 0-255 wrapping) is already in every frame and currently ignored. It is the only
**direct** measure of missed traffic:

```c
missed += (seq - lastSeq - 1) & 0xFF;
```

This is immune to night gating — a gated sensor sends nothing, so `seq` does not advance, and the
first frame after the gate is `lastSeq + 1` for a gap of zero. Re-baselining on the first frame of
each wake makes it immune to cart-off time too, satisfying *"this time gap should not be counted
toward missed messages"* with **no persistence, no RTC memory, and no dependence on whether the
install is always-on or deep-sleeping.**

**`seq` cannot detect a *dead* sensor**, because silence produces no gap — so silence is handled
separately by the 24-hour awake-time rule in §13.3.1. The two are complementary: `seq` counts what
was missed while the sensor was talking; the timer catches it going quiet altogether.

### 13.3 Display states

`lbl_pair_mailbox` label becomes the fixed literal **`PAIR MBX`**, set in EEZ; C stops writing it.

| Condition | `lbl_mailbox_id_str` | Colour |
|---|---|---|
| Not paired, no live offer | `NO MAILBOX` | white |
| Live offer, id ≠ paired | `deadbeef OFFERED` | white |
| Paired, silent ≥ 24 h awake (§13.3.1) | `a1b2c3d4 SILENT` | **red** `#ff0000` |
| Paired, no contact yet this wake | `a1b2c3d4` | white |
| Paired, contact, nothing missed | `a1b2c3d4` | **green** `#00ff2d` |
| Paired, contact, missed N ≥ 1 | `a1b2c3d4 (N)` | **yellow** `#ffff00` |

The miss count is parenthesised so it reads as an annotation rather than as part of the hex id,
which is otherwise easy to misread at a glance.

Precedence, highest first: **OFFERED → SILENT → yellow(N) → green → white.** Yellow is suppressed at
zero per your note, and because green/yellow are separated by the miss count rather than a timer, the
colour and the number can never disagree. Red reuses the GCI disconnected colour.

#### 13.3.1 Silent-sensor detection — 24 h of *awake* time

`seq` cannot see silence, so a dead sensor would otherwise sit at white forever. A 24-hour threshold
solves this and is immune to the night gate, being roughly double the longest legitimate quiet
period.

The measurement must be **awake time, not wall-clock**. Wall-clock cannot distinguish "the sensor is
silent" from "we were not listening": a cart parked Monday to Friday has a four-day-old last contact
even if the sensor transmitted hourly all week — a false alarm on a healthy sensor, and precisely the
cart-off case that must not count.

```c
#define MBX_SILENT_MS (24UL * 60UL * 60UL * 1000UL)

// Silent when nothing has been heard for 24 h of continuous listening. Falls back
// to time-since-boot when the paired mailbox has never been heard this wake, which
// is the common already-dead-at-power-on case.
uint32_t since = millis() - (s_heardThisWake ? s_lastHeardMs : s_bootMs);
bool silent = (s_pairedMbxId != 0) && (since >= MBX_SILENT_MS);
```

No persistence, no RTC memory, no dependence on GPS time — `millis()` within the current wake session
is sufficient, which also sidesteps the install-dependent power question.

**Behaviour by installation, stated so it is not later mistaken for a bug:**
- **NO_GCI** (GCD powered continuously, only the display sleeps): `millis()` runs unbroken, so this
  works as intended — red appears 24 h after genuine silence.
- **GCI** (deep sleep when the cart is off): the timer restarts each wake, so the warning appears only
  if the cart runs 24 h continuously — in practice, rarely. This is graceful degradation, not a
  defect: a GCD that is not listening cannot legitimately distinguish a dead sensor from a parked
  cart, and the alternative (wall-clock) trades this silence for false alarms.

*If dead-sensor detection is later wanted in deep-sleep installs, the addition is a persisted
last-heard epoch (NVS key `mbx_heard`, ≤15 chars) plus a rule that requires ~1 h of listening in the
new wake before trusting a wall-clock gap. That needs a valid GPS clock, which is unreliable before
lock — see the `time(NULL)` hazard in §3.3 — so it is deliberately not in scope here.*

Two rules that fall out of this and are worth stating:

- **A PAIR offer from the already-paired mailbox is treated as contact, not an offer.** It updates
  `seq`/health and shows green, rather than `OFFERED`. Accepting it is a no-op, so advertising it as
  actionable would be misleading.
- **An offer from a different id takes precedence over the paired display** while live (45 s), then
  reverts. It is transient and actionable; the paired id is static status.

The button keeps its rev 3 behaviour: short click accepts, long press forgets, and `LV_STATE_CHECKED`
green exactly while a claimable offer is pending.

### 13.4 Implementation

**EEZ Studio (first).**
1. New **string** variable `mailbox_id_str`; bind `lbl_mailbox_id_str`'s **Text** to it.
2. Set `lbl_pair_mbx`'s Text to the literal **`PAIR MBX`** (it currently has no tick block — keep it
   that way).
3. Export.

**`hot_packet_parser.cpp`** — read field 2 with the existing `hotField()`, parse to `uint8_t`, and
pass it through: `mailboxOnFrame(mbxId, present, isPairOffer, seq, seqValid)`.

**`mailbox.h` / `.cpp`** — replace `mailboxStatusText()` with:

```c
const char* mailboxIdText(void);   // for get_var_mailbox_id_str(); static buffer
uint8_t     mailboxHealth(void);   // 0 = white, 1 = green, 2 = yellow, 3 = red/silent
```

New state, all single-writer by the §3.2 discipline (parser writes, GUI reads):

```c
static volatile uint8_t  s_lastSeq;        // last seq from the paired mailbox
static volatile bool     s_seqValid;       // false until first contact this wake
static volatile uint16_t s_missedCount;    // saturating; reset on pair/forget
static volatile uint32_t s_lastHeardMs;    // millis() of last matched frame
static volatile bool     s_heardThisWake;  // false until first contact this wake
static           uint32_t s_bootMs;        // set once in mailboxLoad()
```

Contact handling, applied to **any** matched frame (NORM, DEV, PAIR) from the paired id, before the
existing PRESENT/ABSENT edge logic:

```c
if (!s_seqValid) { s_seqValid = true; }                       // baseline, no count
else             { s_missedCount += (uint8_t)(seq - s_lastSeq - 1); }
s_lastSeq       = seq;
s_lastHeardMs   = millis();      // also feeds the 24 h silence timer (§13.3.1)
s_heardThisWake = true;
```

`mailboxAcceptOffer()` and `mailboxForget()` clear `s_seqValid` and `s_missedCount` alongside the
existing glyph-state reset — a new sensor starts with a clean history.

**`get_set_vars.{h,cpp}`** — `get_var_mailbox_id_str()` returning `mailboxIdText()`, plus a no-op
`set_var_mailbox_id_str()`, mirroring the existing string-variable pattern (`get_var_sats_hdop()`).

**`settings2_screen.cpp`** — delete the `lbl_pair_mbx` writes, `s_mbxLabelCache` and `s_mbxLabelMs`.
Add a colour update in the existing 1 Hz pump block, modelled on `updateEspnowGciMacColor()` but
reusing the pump's existing `lv_scr_act() == objects.settings2` guard rather than duplicating it:

```c
uint8_t h = mailboxHealth();
if (h != s_lastHealth) {
    s_lastHealth = h;
    lv_color_t c = (h == 1) ? lv_color_hex(0xff00ff2d)   // green  — healthy
                 : (h == 2) ? lv_color_hex(0xffffff00)   // yellow — frames missed
                 : (h == 3) ? lv_color_hex(0xffff0000)   // red    — silent 24 h
                            : lv_color_white();          // white  — awaiting / unpaired
    lv_obj_set_style_text_color(objects.lbl_mailbox_id_str, c, LV_PART_MAIN | LV_STATE_DEFAULT);
}
```

### 13.5 Verification

| # | Step | Expected |
|---|---|---|
| 1 | Boot unpaired | `NO MAILBOX`, white; button reads `PAIR MBX` |
| 2 | Send `\|#03#PAIR#a1b2c3d4#5#…` | `a1b2c3d4 OFFERED` white; button green |
| 3 | Short click | `a1b2c3d4` white (paired, no contact yet); button back to blue |
| 4 | Send `\|#03#NORM#a1b2c3d4#6#…#PRESENT` | `a1b2c3d4` **green**; glyph lights |
| 5 | Send seq **7** | still green, no count — consecutive |
| 6 | Send seq **10** (skipping 8, 9) | `a1b2c3d4 2` **yellow** |
| 7 | Send seq 11 | stays `a1b2c3d4 2` yellow — count is cumulative, not reset by good frames |
| 8 | Send `\|#03#PAIR#a1b2c3d4#12#…` (own sensor) | treated as contact — no `OFFERED`, count unchanged |
| 9 | Send `\|#03#PAIR#deadbeef#…` | `deadbeef OFFERED` white, button green; after 45 s reverts to `a1b2c3d4 2` yellow |
| 10 | Accept `deadbeef` | `deadbeef` white — history cleared for the new sensor |
| 11 | Long press | `NO MAILBOX` white |
| 12 | Reboot while paired | `a1b2c3d4` white until the first frame — off-time did not count as missed |
| 13 | Simulate the night gate: pause 10 min, resume at the next consecutive seq | no miss counted — the gap is time, not `seq` |
| 14 | Temporarily set `MBX_SILENT_MS` to ~2 min, pair, then send nothing | `a1b2c3d4 SILENT` red after the timeout; restore the real value afterwards |
| 15 | With the short timeout, send a frame before it elapses | timer resets — stays green/yellow, never reaches red |

### 13.6 Open items

- **O-8 — dead-sensor detection in deep-sleep installs.** Handled for NO_GCI installs by §13.3.1; in
  GCI installs the 24 h awake timer rarely elapses. Closing that gap needs a persisted last-heard
  epoch plus a listening-time qualifier, and a trustworthy GPS clock. Deferred deliberately.
- **Confirm `MBX_SILENT_MS = 24 h` clears DDR04's gate.** The threshold assumes the longest
  legitimate quiet period is a single night. If the sensor can also gate on low battery or for
  multi-day stretches, raise it accordingly — it is one `#define`.
- **Sensor-side assumptions to confirm:** `seq` increments once per transmission, shares one counter
  across NORM/DEV/PAIR modes, and is not reset by the pair button. If PAIR frames use a separate
  counter, exclude them from the gap maths.

### 13.7 As built — rev 4

**Long press is now global, and doubled.** `LONG_PRESS_TIME_MS 800` in `config.h`, applied once via
`lv_indev_set_long_press_time()` in `initTouchscreen()`. LVGL 9 exposes this only at runtime per
input device — it is **not** an `lv_conf.h` setting, so it cannot be lost when the build script
re-copies the config template.

This replaced a first attempt that scoped the longer hold to the pair button alone
(`PRESSED` + `LONG_PRESSED_REPEAT` + a fired flag). That machinery existed only to avoid a dead zone
between LVGL's 400 ms `SHORT_CLICKED` cutoff and the 800 ms hold; once the global threshold moved to
800 ms, `SHORT_CLICKED` covers everything below it and the workaround was deleted. **It also affects
chat-row reply**, which is the intended consistency but the one other place it will be felt.

**A stray EEZ variable breaks the link, not just the display.** During bring-up the project briefly
declared two string variables for one label (`lbl_mbx_id_value` and `mailbox_id_str`). EEZ puts
*every* declared native variable into `ui.c`'s `native_vars` table, so **both** needed C definitions
or the firmware would not link — regardless of which one the label was bound to. Deleting the unused
variable in EEZ Studio and re-exporting is the fix; defining both is a valid stopgap since they can
return the same text. Generalises the §12.4 lesson: an orphaned EEZ variable is a build break, while
an orphaned EEZ *expression* is a silent display break.

**`lbl_remote_mac_addr` was deleted** from the EEZ project during the layout rework — the "REMOTE MAC"
caption on the GCI row. No C code referenced it, so there was no build impact, and it leaves both
pairing rows symmetric as `[button][value]`.

**Chime map, final:** `tone_alert()` on the PRESENT rising edge, `tone_message()` on accepting an
offer, `tone_confirm()` on a long-press forget. Silent when a snoozed glyph returns.

**Not yet verified on hardware.** The seq-gap counting, the colour states, the 800 ms holds and the
snooze behaviour all compile and are logically traced, but none has been exercised on the CYD. The
24 h `SILENT` state is impractical to test as-is — temporarily shrink `MBX_SILENT_MS` to a couple of
minutes to exercise it.

