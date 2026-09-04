// =============================================================================
// sthymuli_vibrationModule_wLEDs.ino
//
// Sthymuli — Vibration Module with Reactive LEDs
//
// Reads 8 IR proximity sensors, drives two DC motors via MCP23017 +
// TB6612FNG, and animates the WS2812 LED chain.
//
// =============================================================================
// SENSOR → MOTOR MAPPING
// =============================================================================
//
//   Motor A (North/South axis):
//     CH1 = North sensor   CH5 = South sensor
//
//     North only active  → Motor A direction 1,  intensity = North proximity
//     South only active  → Motor A direction 0,  intensity = South proximity
//     Both active        → direction = normalised(North - South) → 1 or 0
//                          intensity = |North proximity - South proximity|
//     Both ≥ 95%         → Motor A STOPS (emphasise saturation behaviour)
//
//   Motor B (East/West axis):
//     CH3 = East sensor    CH7 = West sensor
//     Same logic as Motor A.
//
// =============================================================================
// LED MODES (auto-selected by sensor state)
// =============================================================================
//
//   WELCOME (first 10s after boot):
//     Meteor — orange comet races around the ring once per 1.2s
//
//   ALL IDLE (no sensor detects anything):
//     Global green breathe — all 24 LEDs pulse green together
//
//   ALL SATURATED (all sensors ≥ SATURATION_MARGIN):
//     Global red breathe  — all 24 LEDs pulse red together
//
//   REACTIVE (normal operation):
//     Green→red by quadrant — each quadrant glows green (far) to red (close)
//     at FULL brightness regardless of distance (so users can always see them)
//
// =============================================================================
// LED LAYOUT — 25 physical, 24 active
// =============================================================================
//
//   NORTH (motherboard) : pixels [0..5]   (6 active LEDs)
//   pixel 6             : RESERVED — battery indicator, never written
//   EAST  (cardinal)    : pixels [7..12]  (6 LEDs)
//   SOUTH (cardinal)    : pixels [13..18] (6 LEDs)
//   WEST  (cardinal)    : pixels [19..24] (6 LEDs)
//
// =============================================================================
// HARDWARE CONNECTIONS
// =============================================================================
//
//   MCP23017 @ 0x21 (vibration module):
//     GPA0 → AIN1   GPA1 → AIN2   GPA2 → BIN1   GPA3 → BIN2
//
//   TB6612FNG:
//     PWMA → GPIO7 (EXT_GPIO_1)    PWMB → GPIO8 (EXT_GPIO_2)
//     STBY → 3.3V    VM → 5V    VCC → 3.3V    GND → GND
//
//   WS2812 chain: DIN → GPIO9
//
// =============================================================================

#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>

// ─────────────────────────────────────────────────────────────────────────────
// IR SENSOR CHANNEL ASSIGNMENTS
// ─────────────────────────────────────────────────────────────────────────────
#define CH_NORTH   1    // Motor A, direction 1
#define CH_SOUTH   5    // Motor A, direction 0
#define CH_EAST    3    // Motor B, direction 1
#define CH_WEST    7    // Motor B, direction 0

// ─────────────────────────────────────────────────────────────────────────────
// PIN DEFINITIONS
// ─────────────────────────────────────────────────────────────────────────────
#define I2C_SDA             48
#define I2C_SCL             47
#define IR_MODULATION_PIN   10    // 38 kHz IR emitter drive
#define IR_ANALOG_PIN        4    // IR proximity analog return
#define IR_S0                5    // MUX select — MCP @ 0x20 GPA5
#define IR_S1                6    // GPA6
#define IR_S2                7    // GPA7
#define PWR_LATCH_PIN        6    // must be held HIGH
#define PWMA_PIN             8    // GPIO7 → TB6612FNG PWMA (Motor A speed)
#define PWMB_PIN             7    // GPIO8 → TB6612FNG PWMB (Motor B speed)
#define LED_PIN              9    // WS2812 DIN

// ─────────────────────────────────────────────────────────────────────────────
// MCP23017
// ─────────────────────────────────────────────────────────────────────────────
#define MAIN_MCP_ADDR   0x20    // existing — IR mux select
#define VIB_MCP_ADDR    0x21    // vibration module — motor direction

#define MOTOR_A_IN1      0    // GPA0 → AIN1
#define MOTOR_A_IN2      1    // GPA1 → AIN2
#define MOTOR_B_IN1      2    // GPA2 → BIN1
#define MOTOR_B_IN2      3    // GPA3 → BIN2

// ─────────────────────────────────────────────────────────────────────────────
// LEDC PWM — ESP32 Arduino core v3.x
// ─────────────────────────────────────────────────────────────────────────────
#define LEDC_CHANNEL_A   4
#define LEDC_CHANNEL_B   5
#define LEDC_FREQ_HZ  1000
#define LEDC_RESOLUTION  8

// ─────────────────────────────────────────────────────────────────────────────
// IR SENSOR CONFIGURATION
// ─────────────────────────────────────────────────────────────────────────────
#define IR_CHANNELS        8
#define IR_SAMPLES        10     // ADC averages per channel
#define IR_POLL_MS       100     // read all 8 channels every 100 ms
#define IR_EMA_ALPHA     0.2f   // exponential moving average smoothing

// Tune these by watching Serial Monitor with nothing near the robot:
//   IR_THRESHOLD_LOW  → ~50% above idle noise floor
//   IR_THRESHOLD_HIGH → reading when hand is ~5 cm away
#define IR_THRESHOLD_LOW    500
#define IR_THRESHOLD_HIGH  2500

// Dead zone — proximity must exceed this before any motor/LED reaction
#define IR_DEAD_ZONE       0.05f

// Saturation margin — fraction above which a sensor counts as "saturated"
// Set slightly below 1.0 to absorb sensor noise near maximum
#define SATURATION_MARGIN  0.90f

// ─────────────────────────────────────────────────────────────────────────────
// MOTOR / VIBRATION CONFIGURATION
// ─────────────────────────────────────────────────────────────────────────────
#define VIB_UPDATE_MS   50
#define PWM_MIN_DUTY    10     // minimum duty to overcome motor stall
#define PWM_MAX_DUTY    100     // maximum duty — cap to avoid excessive noise

// ─────────────────────────────────────────────────────────────────────────────
// LED CONFIGURATION
// ─────────────────────────────────────────────────────────────────────────────
#define NUM_LEDS_TOTAL   25    // physical LEDs in chain
#define NUM_LEDS         24    // active LEDs (pixel 6 reserved for battery)
#define LED_BRIGHTNESS  220    // global brightness cap 0–255
#define LED_UPDATE_MS    16    // ~60 Hz

#define WELCOME_DURATION_MS  5000   // meteor runs for 10 seconds on boot

// Quadrant layout — 6 active LEDs each
// NORTH uses physical [0..5], skips physical 6, then EAST starts at 7
const int QUAD_START[4] = {  0,  7, 13, 19 };   // physical pixel start
const int QUAD_COUNT[4] = {  6,  6,  6,  6 };

// Which IR channel feeds each quadrant's LED colour
// NORTH=CH1, EAST=CH3, SOUTH=CH5, WEST=CH7
const int QUAD_CH[4] = { CH_NORTH, CH_EAST, CH_SOUTH, CH_WEST };

// ─────────────────────────────────────────────────────────────────────────────
// STATE
// ─────────────────────────────────────────────────────────────────────────────
Adafruit_MCP23X17  mainMcp;
Adafruit_MCP23X17  vibMcp;
Adafruit_NeoPixel  strip(NUM_LEDS_TOTAL, LED_PIN, NEO_GRB + NEO_KHZ800);

bool mainMcp_ok = false;
bool vibMcp_ok  = false;

int   irValues[IR_CHANNELS];
float irFiltered[IR_CHANNELS];

uint32_t lastIRPoll      = 0;
uint32_t lastVibUpdate   = 0;
uint32_t lastLEDUpdate   = 0;
uint32_t lastSerialPrint = 0;
uint32_t bootTime        = 0;

uint8_t dirA = 0, dirB = 0;
uint8_t speedA = 0, speedB = 0;

// ─────────────────────────────────────────────────────────────────────────────
// LED HELPERS
// ─────────────────────────────────────────────────────────────────────────────

// Write to a logical LED index (0–23), skipping physical pixel 6 (battery).
// Logical 0–5  → physical 0–5
// Logical 6–23 → physical 7–24
void setLED(int logical, uint32_t color)
{
  if (logical < 0 || logical >= NUM_LEDS) return;
  int physical = (logical < 6) ? logical : logical + 1;
  strip.setPixelColor(physical, color);
}

// Clear all active LEDs (leaves pixel 6 untouched)
void clearActive()
{
  for (int i = 0; i < NUM_LEDS; i++) setLED(i, 0);
}

// Paint a full quadrant by quadrant index (0=N,1=E,2=S,3=W)
// using physical pixel addresses directly (quadrant segments don't straddle pixel 6)
void setQuadrant(int q, uint32_t color)
{
  for (int i = QUAD_START[q]; i < QUAD_START[q] + QUAD_COUNT[q]; i++)
    strip.setPixelColor(i, color);
}

// Sine breathe oscillator — returns 0.0→1.0
// periodMs: full cycle length. phaseOffset: 0.0–1.0 shifts phase.
float breathe(uint32_t now, uint32_t periodMs, float phaseOffset = 0.0f)
{
  float t = fmod((float)(now % periodMs) / (float)periodMs + phaseOffset, 1.0f);
  return 0.5f + 0.5f * sinf(t * 2.0f * PI);
}

// Green→red colour at full brightness.
// t=0.0 → pure green (far)   t=1.0 → pure red (close)
// Brightness is FIXED at maximum — does not scale with proximity.
uint32_t greenToRed(float t)
{
  t = constrain(t, 0.0f, 1.0f);
  uint8_t r = (uint8_t)(255.0f * t);
  uint8_t g = (uint8_t)(255.0f * (1.0f - t));
  return strip.Color(r, g, 0);
}

// Interpolate between two colours
uint32_t lerpColor(uint32_t a, uint32_t b, float t)
{
  t = constrain(t, 0.0f, 1.0f);
  uint8_t ar=(a>>16)&0xFF, ag=(a>>8)&0xFF, ab=a&0xFF;
  uint8_t br=(b>>16)&0xFF, bg=(b>>8)&0xFF, bb=b&0xFF;
  return strip.Color(
    (uint8_t)(ar + t*(br-ar)),
    (uint8_t)(ag + t*(bg-ag)),
    (uint8_t)(ab + t*(bb-ab))
  );
}

// ─────────────────────────────────────────────────────────────────────────────
// IR READING
// ─────────────────────────────────────────────────────────────────────────────

void setIRChannel(uint8_t ch)
{
  mainMcp.digitalWrite(IR_S0,  ch       & 0x01);
  mainMcp.digitalWrite(IR_S1, (ch >> 1) & 0x01);
  mainMcp.digitalWrite(IR_S2, (ch >> 2) & 0x01);
}

// Returns normalised proximity for one channel (0.0 = nothing, 1.0 = very close)
float channelProximity(int ch)
{
  float v = irFiltered[ch];
  if (v <= IR_THRESHOLD_LOW) return 0.0f;
  return constrain(
    (v - IR_THRESHOLD_LOW) / (float)(IR_THRESHOLD_HIGH - IR_THRESHOLD_LOW),
    0.0f, 1.0f
  );
}

void readIR()
{
  if (!mainMcp_ok) return;

  for (int ch = 0; ch < IR_CHANNELS; ch++) {
    setIRChannel(ch);
    delayMicroseconds(200);
    int sum = 0;
    for (int s = 0; s < IR_SAMPLES; s++) {
      sum += analogRead(IR_ANALOG_PIN);
      delayMicroseconds(100);
    }
    irValues[ch] = sum / IR_SAMPLES;

    // EMA filter — seed with first real reading
    if (irFiltered[ch] == 0.0f)
      irFiltered[ch] = (float)irValues[ch];
    else
      irFiltered[ch] = IR_EMA_ALPHA * (float)irValues[ch]
                     + (1.0f - IR_EMA_ALPHA) * irFiltered[ch];
  }

  // Serial output once per second
  uint32_t now = millis();
  if (now - lastSerialPrint >= 1000) {
    lastSerialPrint = now;
    Serial.print("[IR] raw:      ");
    for (int ch = 0; ch < IR_CHANNELS; ch++) Serial.printf("%4d ", irValues[ch]);
    Serial.println();
    Serial.print("[IR] filtered: ");
    for (int ch = 0; ch < IR_CHANNELS; ch++) Serial.printf("%4d ", (int)irFiltered[ch]);
    Serial.println();
    Serial.printf("[IR] N=%.2f S=%.2f E=%.2f W=%.2f\n",
      channelProximity(CH_NORTH), channelProximity(CH_SOUTH),
      channelProximity(CH_EAST),  channelProximity(CH_WEST));
    Serial.println("─────────────────────────────────────────────────────");
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// MOTOR CONTROL
// ─────────────────────────────────────────────────────────────────────────────

void setMotorA(uint8_t dir) {
  switch (dir) {
    case 1:  vibMcp.digitalWrite(MOTOR_A_IN1,HIGH); vibMcp.digitalWrite(MOTOR_A_IN2,LOW);  break;
    case 2:  vibMcp.digitalWrite(MOTOR_A_IN1,LOW);  vibMcp.digitalWrite(MOTOR_A_IN2,HIGH); break;
    default: vibMcp.digitalWrite(MOTOR_A_IN1,LOW);  vibMcp.digitalWrite(MOTOR_A_IN2,LOW);  break;
  }
  dirA = dir;
}

void setMotorB(uint8_t dir) {
  switch (dir) {
    case 1:  vibMcp.digitalWrite(MOTOR_B_IN1,HIGH); vibMcp.digitalWrite(MOTOR_B_IN2,LOW);  break;
    case 2:  vibMcp.digitalWrite(MOTOR_B_IN1,LOW);  vibMcp.digitalWrite(MOTOR_B_IN2,HIGH); break;
    default: vibMcp.digitalWrite(MOTOR_B_IN1,LOW);  vibMcp.digitalWrite(MOTOR_B_IN2,LOW);  break;
  }
  dirB = dir;
}

void setSpeedA(uint8_t duty) { ledcWrite(PWMA_PIN, duty); speedA = duty; }
void setSpeedB(uint8_t duty) { ledcWrite(PWMB_PIN, duty); speedB = duty; }

void stopMotors() {
  setSpeedA(0); setSpeedB(0);
  setMotorA(0); setMotorB(0);
}

// Apply a motor safely — cuts speed before reversing direction
void applyMotorA(uint8_t newDir, uint8_t duty) {
  if (newDir == 0 || duty == 0) { setSpeedA(0); setMotorA(0); return; }
  if (dirA != 0 && newDir != dirA) { setSpeedA(0); delayMicroseconds(500); }
  setMotorA(newDir);
  setSpeedA(duty);
}

void applyMotorB(uint8_t newDir, uint8_t duty) {
  if (newDir == 0 || duty == 0) { setSpeedB(0); setMotorB(0); return; }
  if (dirB != 0 && newDir != dirB) { setSpeedB(0); delayMicroseconds(500); }
  setMotorB(newDir);
  setSpeedB(duty);
}

// Map a proximity fraction to a PWM duty cycle
uint8_t proximityToDuty(float prox) {
  if (prox < IR_DEAD_ZONE) return 0;
  return (uint8_t)map((long)(prox * 1000), 0, 1000, PWM_MIN_DUTY, PWM_MAX_DUTY);
}

// =============================================================================
// updateVibration()
//
// Motor A — driven by North (CH1) and South (CH5):
//
//   Case 1 — North only active (South below dead zone):
//     direction = 1 (forward)
//     intensity = North proximity
//
//   Case 2 — South only active (North below dead zone):
//     direction = 0 (backward)
//     intensity = South proximity
//
//   Case 3 — Both active:
//     direction = 1 if North > South, else 0
//     intensity = |North - South|  (difference drives the motor,
//                 pure symmetry = zero vibration on this axis)
//
//   Case 4 — Both >= SATURATION_MARGIN:
//     Motor A STOPS — emphasise the fully-surrounded state
//
// Motor B — identical logic with East (CH3) and West (CH7).
// =============================================================================
void updateVibration()
{
  if (!vibMcp_ok) return;
  uint32_t now = millis();
  if (now - lastVibUpdate < VIB_UPDATE_MS) return;
  lastVibUpdate = now;

  float pN = channelProximity(CH_NORTH);
  float pS = channelProximity(CH_SOUTH);
  float pE = channelProximity(CH_EAST);
  float pW = channelProximity(CH_WEST);

  bool northActive     = (pN >= IR_DEAD_ZONE);
  bool southActive     = (pS >= IR_DEAD_ZONE);
  bool bothSaturatedNS = (pN >= SATURATION_MARGIN && pS >= SATURATION_MARGIN);
  bool eastActive      = (pE >= IR_DEAD_ZONE);
  bool westActive      = (pW >= IR_DEAD_ZONE);
  bool bothSaturatedEW = (pE >= SATURATION_MARGIN && pW >= SATURATION_MARGIN);

  uint8_t dirA_new = 0, dutyA = 0;
  uint8_t dirB_new = 0, dutyB = 0;

  // ── Motor A: North / South ───────────────────────────────────────────────
  if (bothSaturatedNS) {
    // Case 4: both maxed → stop to emphasise saturation state
    dirA_new = 0; dutyA = 0;

  } else if (northActive && southActive) {
    // Case 3: both active → direction from dominant side.
    // Intensity = average of both (NOT the difference) so the motor always
    // runs when objects are detected. The difference only sets direction.
    float diff      = pN - pS;
    dirA_new        = (diff >= 0) ? 1 : 2;
    float intensity = (pN + pS) / 2.0f;          // average = always strong enough
    dutyA           = proximityToDuty(intensity);

  } else if (northActive) {
    // Case 1: North only → forward
    dirA_new = 1;
    dutyA    = proximityToDuty(pN);

  } else if (southActive) {
    // Case 2: South only → backward
    dirA_new = 2;
    dutyA    = proximityToDuty(pS);

  } else {
    dirA_new = 0; dutyA = 0;
  }

  // ── Motor B: East / West ─────────────────────────────────────────────────
  if (bothSaturatedEW) {
    dirB_new = 0; dutyB = 0;

  } else if (eastActive && westActive) {
    float diff      = pE - pW;
    dirB_new        = (diff >= 0) ? 1 : 2;
    float intensity = (pE + pW) / 2.0f;
    dutyB           = proximityToDuty(intensity);

  } else if (eastActive) {
    dirB_new = 1;
    dutyB    = proximityToDuty(pE);

  } else if (westActive) {
    dirB_new = 2;
    dutyB    = proximityToDuty(pW);

  } else {
    dirB_new = 0; dutyB = 0;
  }

  // ── Diagnostics — printed every update so you can follow the logic ────────
  Serial.printf(
    "[VIB] pN=%.2f pS=%.2f pE=%.2f pW=%.2f | "
    "Nact=%d Sact=%d Eact=%d Wact=%d | "
    "NS_sat=%d EW_sat=%d | "
    "A→dir=%d duty=%d  B→dir=%d duty=%d\n",
    pN, pS, pE, pW,
    northActive, southActive, eastActive, westActive,
    bothSaturatedNS, bothSaturatedEW,
    dirA_new, dutyA, dirB_new, dutyB
  );

  applyMotorA(dirA_new, dutyA);
  applyMotorB(dirB_new, dutyB);
}

// ─────────────────────────────────────────────────────────────────────────────
// LED PATTERNS
// ─────────────────────────────────────────────────────────────────────────────

// WELCOME: meteor — orange comet races clockwise around the 24 active LEDs
void ledMeteor(uint32_t now)
{
  clearActive();
  float t = fmod((float)now / 1200.0f, 1.0f);   // one lap per 1.2 s
  int head = (int)(t * NUM_LEDS);
  uint32_t headColor = strip.Color(255, 120,  0);  // bright orange
  uint32_t tailColor = strip.Color( 60,   0,  0);  // dark red
  for (int tail = 0; tail < 7; tail++) {
    int idx = (head - tail + NUM_LEDS) % NUM_LEDS;
    float fade = 1.0f - (float)tail / 7.0f;
    setLED(idx, lerpColor(tailColor, headColor, fade));
  }
}

// ALL IDLE: slow green breathe — 2.5 s cycle
void ledGreenBreathe(uint32_t now)
{
  float b  = breathe(now, 2500);
  uint8_t gv = (uint8_t)(30 + b * 220);   // 30–250
  uint32_t color = strip.Color(0, gv, 0);
  for (int i = 0; i < NUM_LEDS; i++) setLED(i, color);
}

// ALL SATURATED: slow red breathe — 2 s cycle
void ledRedBreathe(uint32_t now)
{
  float b  = breathe(now, 2000);
  uint8_t rv = (uint8_t)(30 + b * 220);   // 30–250
  uint32_t color = strip.Color(rv, 0, 0);
  for (int i = 0; i < NUM_LEDS; i++) setLED(i, color);
}

// REACTIVE: green→red per quadrant, FULL brightness regardless of distance.
// Each quadrant uses its single directional sensor for colour.
// Green = far (proximity = 0), Red = close (proximity = 1).
void ledReactive(uint32_t now)
{
  float prox[4] = {
    channelProximity(CH_NORTH),   // NORTH quadrant
    channelProximity(CH_EAST),    // EAST  quadrant
    channelProximity(CH_SOUTH),   // SOUTH quadrant
    channelProximity(CH_WEST),    // WEST  quadrant
  };

  for (int q = 0; q < 4; q++) {
    uint32_t color;
    if (prox[q] < IR_DEAD_ZONE) {
      // Nothing detected on this side — dim green so LEDs are never dark
      color = strip.Color(0, 40, 0);
    } else {
      // Full brightness green→red — no scaling by proximity
      color = greenToRed(prox[q]);
    }
    setQuadrant(q, color);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// LED STATE MACHINE
// ─────────────────────────────────────────────────────────────────────────────
void updateLEDs()
{
  uint32_t now = millis();
  if (now - lastLEDUpdate < LED_UPDATE_MS) return;
  lastLEDUpdate = now;

  // ── WELCOME mode: first 10 seconds after boot ─────────────────────────────
  if (now - bootTime < WELCOME_DURATION_MS) {
    ledMeteor(now);
    strip.show();
    return;
  }

  // ── Count idle and saturated sensors (using 4 directional channels only) ──
  float pN = channelProximity(CH_NORTH);
  float pS = channelProximity(CH_SOUTH);
  float pE = channelProximity(CH_EAST);
  float pW = channelProximity(CH_WEST);

  bool nIdle = (pN < IR_DEAD_ZONE);
  bool sIdle = (pS < IR_DEAD_ZONE);
  bool eIdle = (pE < IR_DEAD_ZONE);
  bool wIdle = (pW < IR_DEAD_ZONE);

  bool allIdle = nIdle && sIdle && eIdle && wIdle;

  bool nSat = (pN >= SATURATION_MARGIN);
  bool sSat = (pS >= SATURATION_MARGIN);
  bool eSat = (pE >= SATURATION_MARGIN);
  bool wSat = (pW >= SATURATION_MARGIN);

  bool allSaturated = nSat && sSat && eSat && wSat;

  // ── Select and run LED mode ───────────────────────────────────────────────
  if (allSaturated) {
    ledRedBreathe(now);           // all saturated → red breathe
  } else if (allIdle) {
    ledGreenBreathe(now);         // all idle → green breathe
  } else {
    ledReactive(now);             // mixed → green→red by quadrant
  }

  strip.show();
}

// ─────────────────────────────────────────────────────────────────────────────
// SETUP
// ─────────────────────────────────────────────────────────────────────────────
void setup()
{
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n[STHYMULI] Vibration + LED module starting...");

  bootTime = millis();

  // Power latch — must be first
  pinMode(PWR_LATCH_PIN, OUTPUT);
  digitalWrite(PWR_LATCH_PIN, HIGH);

  // IR emitters on
  pinMode(IR_MODULATION_PIN, OUTPUT);
  digitalWrite(IR_MODULATION_PIN, HIGH);

  // LEDC PWM (v3.x API)
  ledcAttachChannel(PWMA_PIN, LEDC_FREQ_HZ, LEDC_RESOLUTION, LEDC_CHANNEL_A);
  ledcAttachChannel(PWMB_PIN, LEDC_FREQ_HZ, LEDC_RESOLUTION, LEDC_CHANNEL_B);
  ledcWrite(PWMA_PIN, 0);
  ledcWrite(PWMB_PIN, 0);
  Serial.println("[SETUP] LEDC PWM ready — GPIO7 (PWMA), GPIO8 (PWMB)");

  // LEDs
  strip.begin();
  strip.setBrightness(LED_BRIGHTNESS);
  strip.clear();
  // Mark battery pixel with dim warm white so it is visually identifiable
  strip.setPixelColor(6, strip.Color(10, 6, 0));
  strip.show();
  Serial.printf("[SETUP] WS2812 ready — %d active LEDs, pixel 6 reserved\n", NUM_LEDS);

  // I²C
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setTimeout(50);

  // Main MCP @ 0x20 — IR mux
  mainMcp_ok = mainMcp.begin_I2C(MAIN_MCP_ADDR);
  if (mainMcp_ok) {
    mainMcp.pinMode(IR_S0, OUTPUT);
    mainMcp.pinMode(IR_S1, OUTPUT);
    mainMcp.pinMode(IR_S2, OUTPUT);
    Serial.println("[SETUP] Main MCP23017 @ 0x20 ✓");
  } else {
    Serial.println("[SETUP] Main MCP23017 @ 0x20 NOT FOUND — IR disabled");
  }

  // Vibration MCP @ 0x21 — motor direction
  vibMcp_ok = vibMcp.begin_I2C(VIB_MCP_ADDR);
  if (vibMcp_ok) {
    vibMcp.pinMode(MOTOR_A_IN1, OUTPUT);
    vibMcp.pinMode(MOTOR_A_IN2, OUTPUT);
    vibMcp.pinMode(MOTOR_B_IN1, OUTPUT);
    vibMcp.pinMode(MOTOR_B_IN2, OUTPUT);
    stopMotors();
    Serial.println("[SETUP] Vibration MCP23017 @ 0x21 ✓");
  } else {
    Serial.println("[SETUP] Vibration MCP23017 @ 0x21 NOT FOUND — motors disabled");
  }

  // Init IR arrays
  for (int i = 0; i < IR_CHANNELS; i++) {
    irValues[i]   = 0;
    irFiltered[i] = 0.0f;
  }

  Serial.println("[STHYMULI] Welcome mode — meteor for 10 s");
  Serial.println("[STHYMULI] Setup complete — running.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
// LOOP
// ─────────────────────────────────────────────────────────────────────────────
void loop()
{
  // Read IR sensors at 10 Hz
  if (millis() - lastIRPoll >= IR_POLL_MS) {
    lastIRPoll = millis();
    readIR();
  }

  // Update vibration motors at 20 Hz (skips during welcome — motors stay off)
  if (millis() - bootTime >= WELCOME_DURATION_MS)
    updateVibration();

  // Update LEDs at ~60 Hz
  updateLEDs();
}
