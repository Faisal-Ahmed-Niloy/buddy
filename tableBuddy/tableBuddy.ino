
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

// --------------------------------------------------
// PIN CONFIGURATION

#define OLED_SDA 0
#define OLED_SCL 1

#define BUTTON_PIN 3

#define DHT_PIN 10
#define DHT_TYPE DHT11

// --------------------------------------------------
// OLED CONFIGURATION

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

DHT dht(DHT_PIN, DHT_TYPE);

// --------------------------------------------------
// TIMING

const unsigned long LONELY_MS = 30000;
const unsigned long FRAME_MS = 70;
const unsigned long DEBOUNCE_MS = 30;
const unsigned long DHT_INTERVAL = 2000;

// --------------------------------------------------
// MOODS

enum Mood : uint8_t {
  NEUTRAL,
  HAPPY,
  SURPRISED,
  ANGRY,
  LONELY
};

const char *const MOOD_NAMES[] = {
  "NEUTRAL",
  "HAPPY",
  "SURPRISED",
  "ANGRY",
  "LONELY"
};

Mood currentMood = NEUTRAL;
Mood shownMood = NEUTRAL;

unsigned long moodUntil = 0;
unsigned long lastPet = 0;

// --------------------------------------------------
// BUTTON VARIABLES

bool buttonLastRaw = HIGH;
bool buttonStable = HIGH;

unsigned long buttonChangedAt = 0;
unsigned long buttonDownAt = 0;

bool longPressHandled = false;

uint8_t tapCount = 0;
unsigned long lastTap = 0;

// --------------------------------------------------
// DHT11 VARIABLES

float temperature = 0;
float humidity = 0;

bool dhtValid = false;
unsigned long lastDhtRead = 0;

// --------------------------------------------------
// EYE ANIMATION VARIABLES

int8_t gazeX = 0;
int8_t gazeY = 0;

unsigned long nextGlance = 0;
unsigned long nextBlink = 0;
unsigned long blinkAt = 0;

// --------------------------------------------------
// MOOD CONTROL

void feel(Mood m, unsigned long duration, unsigned long now) {
  currentMood = m;
  moodUntil = now + duration;
}

Mood settleMood(unsigned long now) {

  if (now < moodUntil) {
    return currentMood;
  }

  if (now - lastPet > LONELY_MS) {
    return LONELY;
  }

  return NEUTRAL;
}

// --------------------------------------------------
// BUTTON HANDLING

void handleTap(unsigned long now) {

  if (now - lastTap < 1500) {
    tapCount++;
  } else {
    tapCount = 1;
  }

  lastTap = now;
  lastPet = now;

  if (tapCount >= 5) {

    feel(ANGRY, 4000, now);
    tapCount = 0;

    Serial.println("Mood: ANGRY");

  } else {

    feel(HAPPY, 2500, now);

    Serial.println("Mood: HAPPY");
  }
}

void readButton(unsigned long now) {

  bool raw = digitalRead(BUTTON_PIN);

  if (raw != buttonLastRaw) {
    buttonLastRaw = raw;
    buttonChangedAt = now;
  }

  // Debouncing
  if (now - buttonChangedAt > DEBOUNCE_MS &&
      raw != buttonStable) {

    buttonStable = raw;

    if (buttonStable == LOW) {

      // Button pressed
      buttonDownAt = now;
      longPressHandled = false;

    } else {

      // Button released
      if (!longPressHandled) {
        handleTap(now);
      }
    }
  }

  // Long press detection
  if (buttonStable == LOW &&
      !longPressHandled &&
      now - buttonDownAt >= 800) {

    feel(SURPRISED, 1500, now);

    lastPet = now;
    tapCount = 0;

    longPressHandled = true;

    Serial.println("Mood: SURPRISED");
  }
}

// --------------------------------------------------
// DHT11 SENSOR

void readDHT(unsigned long now) {

  if (now - lastDhtRead < DHT_INTERVAL) {
    return;
  }

  lastDhtRead = now;

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (!isnan(h) && !isnan(t)) {

    humidity = h;
    temperature = t;

    dhtValid = true;

  } else {

    dhtValid = false;

    Serial.println("DHT11 reading failed!");
  }
}

// --------------------------------------------------
// EYE DIMENSIONS

int16_t getEyeWidth(Mood m) {

  switch (m) {

    case HAPPY:     return 29;
    case SURPRISED: return 30;
    case ANGRY:     return 27;
    case LONELY:    return 25;

    default:        return 28;
  }
}

int16_t getEyeHeight(Mood m) {

  switch (m) {

    case HAPPY:     return 16;
    case SURPRISED: return 27;
    case ANGRY:     return 21;
    case LONELY:    return 20;

    default:        return 23;
  }
}

// --------------------------------------------------
// BLINK ANIMATION

int16_t getBlinkHeight(int16_t baseHeight, unsigned long now) {

  if (blinkAt == 0) {
    return baseHeight;
  }

  unsigned long elapsed = now - blinkAt;

  if (elapsed >= 180) {
    blinkAt = 0;
    return baseHeight;
  }

  int16_t squeeze;

  if (elapsed < 90) {
    squeeze = elapsed;
  } else {
    squeeze = 180 - elapsed;
  }

  int16_t h = baseHeight -
    ((baseHeight - 3) * squeeze / 90);

  return max((int16_t)3, h);
}

// --------------------------------------------------
// DRAW ONE EYE

void drawEye(
  int16_t cx,
  int16_t cy,
  int16_t w,
  int16_t h,
  Mood m,
  int8_t gx,
  int8_t gy
) {

  int16_t x = cx - w / 2;
  int16_t y = cy - h / 2;

  // FIXED: Avoid min() type mismatch
  int16_t radius = (h / 2 < 7) ? (int16_t)(h / 2) : 7;

  // White eye
  display.fillRoundRect(
    x, y, w, h,
    radius,
    SSD1306_WHITE
  );

  // Pupil size
  int16_t pupilRadius = 3;

  if (m == HAPPY || m == SURPRISED) {
    pupilRadius = 2;
  }

  // Black pupil
  display.fillCircle(
    cx + gx,
    cy + gy,
    pupilRadius,
    SSD1306_BLACK
  );
}
// --------------------------------------------------
// DRAW FACE

void drawFace(unsigned long now) {

  int16_t eyeW = getEyeWidth(shownMood);
  int16_t eyeH = getEyeHeight(shownMood);

  eyeH = getBlinkHeight(eyeH, now);

  const int16_t centerY = 33;

  display.clearDisplay();

  // Mood label
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(MOOD_NAMES[shownMood]);

  // Angry eyebrows
  if (shownMood == ANGRY) {

    display.drawLine(
      25, 16, 52, 20,
      SSD1306_WHITE
    );

    display.drawLine(
      76, 20, 103, 16,
      SSD1306_WHITE
    );
  }

  // Animated eyes
  drawEye(
    40, centerY,
    eyeW, eyeH,
    shownMood,
    gazeX, gazeY
  );

  drawEye(
    88, centerY,
    eyeW, eyeH,
    shownMood,
    gazeX, gazeY
  );

  // Happy cheeks
  if (shownMood == HAPPY) {

    display.fillCircle(
      24, 48, 2,
      SSD1306_WHITE
    );

    display.fillCircle(
      104, 48, 2,
      SSD1306_WHITE
    );
  }

  // Lonely tear
  if (shownMood == LONELY) {

    int16_t tearY = 48 + (now / 200) % 5;

    display.fillCircle(
      56, tearY, 2,
      SSD1306_WHITE
    );
  }

  // Separator above sensor readings
  display.drawLine(
    0, 54, 127, 54,
    SSD1306_WHITE
  );

  // Temperature and humidity
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(24, 56);

  if (dhtValid) {

    display.print("T:");
    display.print(temperature, 0);
    display.print("C H:");
    display.print(humidity, 0);
    display.print("%");

  } else {

    display.print("T:--C H:--%");
  }

  // Send framebuffer to OLED
  display.display();
}

// --------------------------------------------------
// SETUP

void setup() {

  Serial.begin(115200);

  // I2C initialization
  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setClock(400000);

  // OLED initialization
  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        OLED_ADDRESS
      )) {

    Serial.println("OLED initialization failed!");

    while (true) {
      delay(100);
    }
  }

  display.clearDisplay();
  display.display();

  // Button initialization
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // DHT11 initialization
  dht.begin();

  // Random animation seed
  randomSeed(micros());

  lastPet = millis();

  nextBlink = millis() + 1800;
  nextGlance = millis() + 1200;

  feel(HAPPY, 1500, millis());

  Serial.println("-------------------------");
  Serial.println("TABLE BUDDY STARTED!");
  Serial.println("ESP32-C3 Super Mini");
  Serial.println("OLED: SSD1306");
  Serial.println("DHT11: GPIO10");
  Serial.println("BUTTON: GPIO3");
  Serial.println("-------------------------");
}

// --------------------------------------------------
// LOOP

unsigned long lastFrame = 0;

void loop() {

  unsigned long now = millis();

  // Read physical button
  readButton(now);

  // Update temperature and humidity
  readDHT(now);

  // Update mood
  Mood newMood = settleMood(now);

  if (newMood != shownMood) {

    shownMood = newMood;

    Serial.print("Mood changed: ");
    Serial.println(MOOD_NAMES[shownMood]);
  }

  // Blinking
  if (now >= nextBlink) {

    blinkAt = now;

    nextBlink = now + 2500 + random(3000);
  }

  // Random eye movement
  if (now >= nextGlance) {

    bool calm =
      shownMood == NEUTRAL ||
      shownMood == LONELY;

    if (calm) {

      gazeX = random(-3, 4);
      gazeY = random(-2, 3);

    } else {

      gazeX = 0;
      gazeY = 0;
    }

    nextGlance = now + 1200 + random(2000);
  }

  // OLED frame update
  if (now - lastFrame >= FRAME_MS) {

    lastFrame = now;

    drawFace(now);
  }
}
