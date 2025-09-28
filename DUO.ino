#include <FastLED.h>

// ====== LED matrix ======
#define DATA_PIN     6
#define WIDTH        16
#define HEIGHT       16
#define NUM_LEDS     (WIDTH * HEIGHT)
#define CHIPSET      WS2812B
#define COLOR_ORDER  GRB
#define BRIGHTNESS   12
#define SERPENTINE   1

// If orientation looks mirrored/flipped, set these:
#define MIRROR_X 0
#define MIRROR_Y 0

// ====== Joysticks ======
// Player 1
#define JOY1_X   A0
#define JOY1_Y   A1
#define JOY1_SW  2      // LOW when pressed (restart)
// Player 2
#define JOY2_X   A2
#define JOY2_Y   A3
#define JOY2_SW  3

// ====== Tuning ======
#define DEADZONE        40
#define DIR_HYSTERESIS  8

#define BASE_INTERVAL_MS 120   // base move cadence (ms)

// Combo (dash) detection
#define COMBO_MS          180  // double-flick max separation
#define DASH_DIST         4
#define DASH_COOLDOWN_MS  450

// Round timer
#define ROUND_MS  (1UL * 60UL * 1000UL)  // 3 minutes

// Perks
#define PERK_PERIOD_MS    7000
#define PERK_LIFETIME_MS  8000
#define FREEZE_MS         1500
#define SLOW_MS           3000
#define SPEED_MS          2500
#define SHIELD_MS         2500
#define BOMB_RADIUS       1

// ====== Types & game state (declared BEFORE any functions) ======
enum Owner : int8_t { OWN_NONE = -1, OWN_P1 = 0, OWN_P2 = 1 };
enum PerkType : uint8_t { NO_PERK=0, FREEZE_OPP, SLOW_OPP, SPEED_SELF, SHIELD_SELF, PAINT_BOMB };

struct Player {
  // pos & dir
  int x, y;
  int8_t dx, dy;

  // input
  uint8_t joyX, joyY, joySW;
  int xCenter, yCenter;

  // timing
  uint32_t lastStepMs = 0;
  uint16_t moveIntervalMs = BASE_INTERVAL_MS;

  // combo detect
  uint8_t  lastDir = 0;        // 0=none,1=R,2=L,3=D,4=U
  uint32_t lastDirTime = 0;
  uint32_t lastDashTime = 0;

  // status effects
  uint32_t freezeUntil = 0;
  uint32_t slowUntil   = 0;
  uint32_t speedUntil  = 0;
  uint32_t shieldUntil = 0;    // global shield: protects ALL owned tiles

  // colors
  CRGB headColor;
  CRGB fillColor;

  // score
  uint16_t score = 0;
};

struct Perk {
  PerkType type = NO_PERK;
  int x = -1, y = -1;
  uint32_t spawnTime = 0;
  bool active = false;
};

CRGB leds[NUM_LEDS];
Player p1, p2;
int8_t owner[WIDTH][HEIGHT];     // 256 bytes total
Perk perk;

uint32_t roundStart = 0;
uint32_t lastPerkTick = 0;

// ====== Mapping ======
static inline uint16_t XY(uint8_t x, uint8_t y) {
  if (MIRROR_X) x = WIDTH  - 1 - x;
  if (MIRROR_Y) y = HEIGHT - 1 - y;
  return SERPENTINE
    ? (y & 1 ? y * WIDTH + (WIDTH - 1 - x) : y * WIDTH + x)
    : (y * WIDTH + x);
}

// ====== Helpers & game logic ======
void clearGrid() {
  for (uint8_t y=0; y<HEIGHT; ++y)
    for (uint8_t x=0; x<WIDTH; ++x)
      owner[x][y] = OWN_NONE;
}

void calibrateCenter(uint8_t xPin, uint8_t yPin, int &xc, int &yc) {
  long sx=0, sy=0; const int N=80;
  for (int i=0;i<N;i++){ sx += analogRead(xPin); sy += analogRead(yPin); delay(2); }
  xc = sx/N; yc = sy/N;
}

void initPlayers() {
  // P1 top-left heading right
  p1 = Player();
  p1.x=0; p1.y=0; p1.dx=1; p1.dy=0;
  p1.joyX=JOY1_X; p1.joyY=JOY1_Y; p1.joySW=JOY1_SW;
  p1.headColor = CRGB::Lime; p1.fillColor = CRGB(0,70,0);

  // P2 bottom-right heading left
  p2 = Player();
  p2.x=WIDTH-1; p2.y=HEIGHT-1; p2.dx=-1; p2.dy=0;
  p2.joyX=JOY2_X; p2.joyY=JOY2_Y; p2.joySW=JOY2_SW;
  p2.headColor = CRGB::Cyan; p2.fillColor = CRGB(0,60,60);

  calibrateCenter(p1.joyX, p1.joyY, p1.xCenter, p1.yCenter);
  calibrateCenter(p2.joyX, p2.joyY, p2.xCenter, p2.yCenter);
}

void applyPaint(int x, int y, Owner who, uint32_t now) {
  if ((unsigned)x >= WIDTH || (unsigned)y >= HEIGHT) return;

  Owner prev = (Owner)owner[x][y];
  if (prev == who) return;

  // respect global shield on the current owner
  if (prev == OWN_P1 && p1.shieldUntil > now) return;
  if (prev == OWN_P2 && p2.shieldUntil > now) return;

  owner[x][y] = who;

  if (who == OWN_P1) {
    if (prev == OWN_P2) { if (p2.score) p2.score--; }
    if (prev != OWN_P1) p1.score++;
  } else if (who == OWN_P2) {
    if (prev == OWN_P1) { if (p1.score) p1.score--; }
    if (prev != OWN_P2) p2.score++;
  }
}

uint8_t dirFromDelta(int8_t dx, int8_t dy) {
  if (dx==1 && dy==0) return 1;
  if (dx==-1 && dy==0) return 2;
  if (dx==0 && dy==1) return 3;
  if (dx==0 && dy==-1) return 4;
  return 0;
}

void readStickDir(Player &plr) {
  int rx = analogRead(plr.joyX) - plr.xCenter;
  int ry = analogRead(plr.joyY) - plr.yCenter;

  if (abs(rx) > abs(ry) + DIR_HYSTERESIS) {
    if (rx > DEADZONE)  { plr.dx =  1; plr.dy =  0; }
    if (rx < -DEADZONE) { plr.dx = -1; plr.dy =  0; }
  } else if (abs(ry) > abs(rx) + DIR_HYSTERESIS) {
    if (ry > DEADZONE)  { plr.dx =  0; plr.dy =  1; }
    if (ry < -DEADZONE) { plr.dx =  0; plr.dy = -1; }
  }
}

// renamed to avoid any auto-prototype weirdness
bool checkDashCombo(Player &plr, uint32_t now) {
  uint8_t d = dirFromDelta(plr.dx, plr.dy);
  if (!d) return false;
  if (now - plr.lastDashTime < DASH_COOLDOWN_MS) return false;

  if (plr.lastDir == d && (now - plr.lastDirTime) <= COMBO_MS) {
    plr.lastDashTime = now;
    plr.lastDir = 0;
    return true;
  }
  plr.lastDir = d;
  plr.lastDirTime = now;
  return false;
}

void moveStep(Player &plr, Owner who, uint8_t steps) {
  uint32_t now = millis();
  for (uint8_t i=0;i<steps;i++) {
    plr.x = (plr.x + plr.dx + WIDTH ) % WIDTH;
    plr.y = (plr.y + plr.dy + HEIGHT) % HEIGHT;
    applyPaint(plr.x, plr.y, who, now);
  }
}

void paintBomb(Player &plr, Owner who) {
  uint32_t now = millis();
  applyPaint(plr.x, plr.y, who, now);
  for (int r=1; r<=BOMB_RADIUS; ++r) {
    applyPaint(plr.x+r, plr.y, who, now);
    applyPaint(plr.x-r, plr.y, who, now);
    applyPaint(plr.x, plr.y+r, who, now);
    applyPaint(plr.x, plr.y-r, who, now);
  }
}

void applyStatusCadence(Player &plr) {
  uint16_t interval = BASE_INTERVAL_MS;
  uint32_t now = millis();
  if (plr.slowUntil  > now) interval += 80;
  if (plr.speedUntil > now) interval = (interval > 50) ? (interval - 50) : interval;
  plr.moveIntervalMs = interval;
}

void spawnPerk() {
  // try to place on a neutral tile for fairness
  int tries = 200;
  while (tries--) {
    int x = random(WIDTH), y = random(HEIGHT);
    if (owner[x][y] != OWN_NONE) continue;
    perk.x = x; perk.y = y;
    perk.type = (PerkType)random(1, 6); // 1..5
    perk.spawnTime = millis();
    perk.active = true;
    return;
  }
  perk.active = false;
}

void consumePerk(Player &collector, Player &opponent, Owner who) {
  uint32_t now = millis();
  switch (perk.type) {
    case FREEZE_OPP: opponent.freezeUntil = now + FREEZE_MS; break;
    case SLOW_OPP:   opponent.slowUntil   = now + SLOW_MS;   break;
    case SPEED_SELF: collector.speedUntil = now + SPEED_MS;  break;
    case SHIELD_SELF: collector.shieldUntil = now + SHIELD_MS; break;
    case PAINT_BOMB: paintBomb(collector, who); break;
    default: break;
  }
  perk.active = false;
}

void drawBoard() {
  uint32_t now = millis();

  // tiles
  for (uint8_t y=0; y<HEIGHT; ++y) {
    for (uint8_t x=0; x<WIDTH; ++x) {
      CRGB c = CRGB::Black;
      if (owner[x][y] == OWN_P1) {
        c = (p1.shieldUntil > now) ? CRGB(0,120,0) : p1.fillColor;
      } else if (owner[x][y] == OWN_P2) {
        c = (p2.shieldUntil > now) ? CRGB(0,120,120) : p2.fillColor;
      }
      leds[XY(x,y)] = c;
    }
  }

  // perk
  if (perk.active) leds[XY(perk.x, perk.y)] = CRGB(150, 0, 150);

  // heads
  leds[XY(p1.x, p1.y)] = p1.headColor;
  leds[XY(p2.x, p2.y)] = p2.headColor;

  // time bar on row 0
  uint32_t elapsed = now - roundStart;
  uint8_t filled = (elapsed >= ROUND_MS) ? WIDTH : (uint8_t)((uint32_t)WIDTH * elapsed / ROUND_MS);
  for (uint8_t x=0; x<filled; ++x) leds[XY(x,0)] = CRGB::White;

  FastLED.show();
}

void endRoundFlash() {
  CRGB c = (p1.score > p2.score) ? p1.headColor :
           (p2.score > p1.score) ? p2.headColor : CRGB(128,0,128);
  for (int i=0;i<6;i++) {
    fill_solid(leds, NUM_LEDS, c); FastLED.show(); delay(140);
    fill_solid(leds, NUM_LEDS, CRGB::Black); FastLED.show(); delay(120);
  }
}

// ====== Arduino entry points ======
void setup() {
  FastLED.addLeds<CHIPSET, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  pinMode(JOY1_SW, INPUT_PULLUP);
  pinMode(JOY2_SW, INPUT_PULLUP);
  randomSeed(analogRead(JOY1_X) ^ analogRead(JOY1_Y) ^ analogRead(JOY2_X) ^ analogRead(JOY2_Y) ^ micros());

  clearGrid();
  initPlayers();

  // paint starting tiles
  owner[p1.x][p1.y] = OWN_P1; p1.score = 1;
  owner[p2.x][p2.y] = OWN_P2; p2.score = 1;

  roundStart = millis();
  lastPerkTick = millis();
}

void loop() {
  uint32_t now = millis();

  // restart with either button after round end (or mid-round if desired)
  static bool lb1=HIGH, lb2=HIGH;
  bool b1 = digitalRead(JOY1_SW);
  bool b2 = digitalRead(JOY2_SW);
  bool pressed = (b1==LOW && lb1==HIGH) || (b2==LOW && lb2==HIGH);
  lb1=b1; lb2=b2;

  if (now - roundStart >= ROUND_MS) {
    drawBoard();
    endRoundFlash();

    // reset round
    clearGrid();
    initPlayers();
    owner[p1.x][p1.y] = OWN_P1; p1.score = 1;
    owner[p2.x][p2.y] = OWN_P2; p2.score = 1;
    roundStart = millis();
    perk.active = false;
    lastPerkTick = millis();
    return;
  }

  if (pressed) {
    // manual restart
    clearGrid();
    initPlayers();
    owner[p1.x][p1.y] = OWN_P1; p1.score = 1;
    owner[p2.x][p2.y] = OWN_P2; p2.score = 1;
    roundStart = millis();
    perk.active = false;
    lastPerkTick = millis();
  }

  // input
  readStickDir(p1);
  readStickDir(p2);

  // cadence (buff/debuff)
  applyStatusCadence(p1);
  applyStatusCadence(p2);

  // movement gates (freeze blocks moves)
  if (now >= p1.freezeUntil && now - p1.lastStepMs >= p1.moveIntervalMs) {
    bool dash = checkDashCombo(p1, now);
    moveStep(p1, OWN_P1, dash ? DASH_DIST : 1);
    p1.lastStepMs = now;
  }
  if (now >= p2.freezeUntil && now - p2.lastStepMs >= p2.moveIntervalMs) {
    bool dash = checkDashCombo(p2, now);
    moveStep(p2, OWN_P2, dash ? DASH_DIST : 1);
    p2.lastStepMs = now;
  }

  // perks: spawn/expire
  if (now - lastPerkTick >= PERK_PERIOD_MS) {
    lastPerkTick = now;
    if (!perk.active) spawnPerk();
  }
  if (perk.active && (now - perk.spawnTime >= PERK_LIFETIME_MS)) {
    perk.active = false;
  }

  // perk pickup
  if (perk.active) {
    if (p1.x == perk.x && p1.y == perk.y) consumePerk(p1, p2, OWN_P1);
    else if (p2.x == perk.x && p2.y == perk.y) consumePerk(p2, p1, OWN_P2);
  }

  drawBoard();
  delay(2);
}
