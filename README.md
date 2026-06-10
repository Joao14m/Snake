# LED Matrix Territory Game

A two player, real time territory control game on a 16×16 WS2812B LED matrix, built in C++ for Arduino with [FastLED](https://github.com/FastLED/FastLED). Each player drives a head around the board with a joystick, painting tiles their color and stealing the opponent's. Whoever controls the most ground when the round timer hits zero wins. The project also ships with a browser based USB live viewer that mirrors the matrix and scoreboard on screen.

## Inspiration

We wanted to bring Splatoon style "paint the floor" competition onto real hardware that people could crowd around. The interesting constraint was fitting a full multiplayer game (movement, power ups, scoring, and timers) onto an Arduino with only 256 bytes of grid memory to work with.

## What it does

* Two players each drive a head around a 16×16 grid using a joystick.
* Moving onto a tile paints it your color and scores a point.
* Moving over an opponent's tile steals it: they lose a point, you gain one.
* The board wraps at the edges, so there are no walls to hide behind.
* A progress bar across the top row counts down the round.
* When time runs out, the board flashes the winner's color and a new round starts automatically.
* Either joystick button restarts the match instantly.

## Power Ups

Power ups spawn on neutral tiles for fairness and disappear after a short lifetime. There are five types:

| Power Up | Effect |
|---|---|
| Freeze | Locks the opponent in place for a moment |
| Slow | Increases the opponent's move interval, slowing them down |
| Speed | Decreases your own move interval, speeding you up |
| Shield | Protects all of your owned tiles from being stolen |
| Paint Bomb | Instantly paints a radius of tiles around you |

## How we built it

**C++ · Arduino · FastLED · Web Serial API · HTML5 Canvas**

Key implementation details:

* **Joystick auto calibration** samples each stick's resting center at startup, so off center hardware doesn't cause drift.
* **Deadzone filtering and directional hysteresis** remove jitter and prevent diagonal flicker by requiring a clear dominant axis before changing direction.
* **Frame rate independent movement** built on `millis()` timing keeps cadence consistent regardless of loop speed.
* **Dash combos**: a quick double flick in the same direction triggers a multi tile dash, with a cooldown to prevent spam.
* **A fixed 256 byte grid** (`owner[16][16]`) tracks ownership for all 256 tiles, with concurrent timed status effects layered on top.
* **Shielded territory renders in a brighter shade** so the board stays readable at a glance.

## Hardware

| Component | Detail |
|---|---|
| LED matrix | 16×16 WS2812B (256 LEDs), serpentine wiring |
| Microcontroller | Arduino (Uno/Nano class) |
| Controllers | 2× analog joysticks with push buttons |
| Library | FastLED |

**Wiring**

| Signal | Pin |
|---|---|
| LED data | D6 |
| Player 1 X / Y / Button | A0 / A1 / D2 |
| Player 2 X / Y / Button | A2 / A3 / D3 |

Joystick buttons use `INPUT_PULLUP` (active LOW).

## Configuration

All tuning lives in `#define`s at the top of the sketch:

```cpp
#define BRIGHTNESS        12       // global LED brightness (keep low; 256 LEDs draw real current)
#define SERPENTINE        1        // 1 if rows alternate direction; 0 for progressive wiring
#define MIRROR_X / MIRROR_Y        // flip orientation if the display looks mirrored
#define BASE_INTERVAL_MS  120      // base move cadence in ms (lower = faster)
#define ROUND_MS          60000    // round length in ms
#define PERK_PERIOD_MS    7000     // how often a power up can spawn
#define DASH_DIST         4        // tiles travelled on a dash combo
```

> Power note: 256 WS2812B LEDs at full brightness can exceed what USB supplies. Keep `BRIGHTNESS` low and use a proper 5V supply with a common ground.

## Getting Started

1. Install FastLED via the Arduino Library Manager.
2. Open the sketch in the Arduino IDE.
3. Confirm `WIDTH`, `HEIGHT`, `DATA_PIN`, and `SERPENTINE` match your hardware.
4. Select your board and port, then upload.
5. On power up, leave both joysticks centered for a second so calibration reads a clean center.

## USB Live Viewer

`viewer.html` is a single file browser app that mirrors the matrix and a P1/P2 scoreboard live over USB using the [Web Serial API](https://developer.mozilla.org/en-US/docs/Web/API/Web_Serial_API).

**Usage**

1. Open `viewer.html` in a desktop Chromium browser (Chrome or Edge; Web Serial is not supported in Firefox or Safari).
2. Click Connect and select the Arduino's serial port.
3. The 16×16 grid and live tile percentages stream in.

**Serial protocol** (1,000,000 baud)

```
LEDS : 'L','E','D','S', W(1), H(1), serp(1), payload[W*H*3]   // RGB per LED
STAT : 'S','T','A','T', p1Count(LE16), p2Count(LE16)          // tile counts
```

> The included sketch focuses on on matrix gameplay. To use the viewer, stream `LEDS`/`STAT` packets in this format over `Serial`.

## Challenges we ran into

* Fitting the entire game state into 256 bytes of grid memory while managing several simultaneous timed status effects.
* Making joystick input feel responsive: raw analog reads are noisy, so calibration, deadzones, and hysteresis all had to work together.
* Keeping movement speed consistent without `delay()` blocking the render loop, which is what led us to `millis()` based timing.
