#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define SDA_PIN 32
#define SCL_PIN 33

#define ADC_PIN 34
#define POT_PIN 35
#define SWITCH_PIN 27

#define SAMPLE_RATE 5000
#define SAMPLE_US 200
#define N 1024

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);

int samples[N];

const char* menuItems[] = {
  "Live Data",
  "Spectrum",
  "Settings"
};

const int MENU_COUNT = 3;

int selectedItem = 0;
int currentPage = 0;

bool previousSwitchState = HIGH;


// ---------------- MEASUREMENTS ----------------

float vmax = 0;
float vmin = 0;
float vpp = 0;
float vavg = 0;
float vrms = 0;
float frequency = 0;
float dutyCycle = 0;

bool hasFrequency = false;

String waveform = "UNKNOWN";


// ---------------- SETUP ----------------

void setup() {

  // Faster because packets are now 1024 samples
  Serial.begin(460800);

  pinMode(SWITCH_PIN, INPUT_PULLUP);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(800000);

  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        0x3C
      )) {

    while (true) {
      delay(1000);
    }
  }

  display.setTextColor(SSD1306_WHITE);
}


// ---------------- MAIN LOOP ----------------

void loop() {

  captureSamples();
  calculateMeasurements();
  detectWaveform();

  sendSamplesToPC();

  handleControls();
  drawCurrentPage();
}


// ---------------- SAMPLE CAPTURE ----------------

void captureSamples() {

  unsigned long nextSample = micros();

  for (int i = 0; i < N; i++) {

    while (micros() < nextSample);

    samples[i] = analogRead(ADC_PIN);

    nextSample += SAMPLE_US;
  }
}


// ---------------- BASIC MEASUREMENTS ----------------

void calculateMeasurements() {

  int rawMax = samples[0];
  int rawMin = samples[0];

  float sum = 0;
  float sumSquares = 0;

  for (int i = 0; i < N; i++) {

    if (samples[i] > rawMax)
      rawMax = samples[i];

    if (samples[i] < rawMin)
      rawMin = samples[i];

    float voltage =
      samples[i] * 3.3 / 4095.0;

    sum += voltage;
    sumSquares += voltage * voltage;
  }

  vmax = rawMax * 3.3 / 4095.0;
  vmin = rawMin * 3.3 / 4095.0;

  vpp = vmax - vmin;
  vavg = sum / N;
  vrms = sqrt(sumSquares / N);


  // -------- FREQUENCY / DUTY --------

  hasFrequency = false;
  frequency = 0;
  dutyCycle = 0;

  if (vpp > 0.2) {

    float midpoint =
      (vmax + vmin) / 2.0;

    int edgeCount = 0;
    int lastEdge = -1;

    float periodSum = 0;

    int highSamples = 0;

    bool previousHigh =
      (samples[0] * 3.3 / 4095.0)
      > midpoint;

    for (int i = 1; i < N; i++) {

      float voltage =
        samples[i] * 3.3 / 4095.0;

      bool currentHigh =
        voltage > midpoint;

      if (currentHigh)
        highSamples++;

      if (!previousHigh && currentHigh) {

        if (lastEdge >= 0) {
          periodSum += i - lastEdge;
          edgeCount++;
        }

        lastEdge = i;
      }

      previousHigh = currentHigh;
    }

    dutyCycle =
      ((float)highSamples / N) * 100.0;

    if (edgeCount > 0) {

      float avgPeriod =
        periodSum / edgeCount;

      frequency =
        SAMPLE_RATE / avgPeriod;

      hasFrequency = true;
    }
  }
}


// ---------------- WAVEFORM DETECTION ----------------

void detectWaveform() {

  if (vpp < 0.2) {

    waveform = "DC";
    return;
  }

  float highThreshold =
    vmax - (0.15 * vpp);

  float lowThreshold =
    vmin + (0.15 * vpp);

  int extremeSamples = 0;

  for (int i = 0; i < N; i++) {

    float voltage =
      samples[i] * 3.3 / 4095.0;

    if (
      voltage > highThreshold ||
      voltage < lowThreshold
    ) {
      extremeSamples++;
    }
  }

  float extremeRatio =
    (float)extremeSamples / N;

  if (extremeRatio > 0.8) {

    waveform = "SQUARE";
    return;
  }


  // -------- SINE --------

  float variance = 0;

  for (int i = 0; i < N; i++) {

    float voltage =
      samples[i] * 3.3 / 4095.0;

    float normalized =
      (voltage - vavg) /
      (vpp / 2.0);

    variance +=
      normalized * normalized;
  }

  variance /= N;

  float stdDev =
    sqrt(variance);

  if (
    stdDev > 0.65 &&
    stdDev < 0.8
  ) {

    waveform = "SINE";
    return;
  }


  // -------- TRIANGLE --------

  int bins[10] = {0};

  for (int i = 0; i < N; i++) {

    float voltage =
      samples[i] * 3.3 / 4095.0;

    float normalized =
      (voltage - vmin) / vpp;

    int bin =
      normalized * 10;

    if (bin >= 10)
      bin = 9;

    if (bin < 0)
      bin = 0;

    bins[bin]++;
  }

  float binMean =
    N / 10.0;

  float binVariance = 0;

  for (int i = 0; i < 10; i++) {

    float diff =
      bins[i] - binMean;

    binVariance +=
      diff * diff;
  }

  binVariance /= 10;

  float uniformity =
    sqrt(binVariance) /
    binMean;

  if (uniformity < 0.35) {

    waveform = "TRIANGLE";
    return;
  }

  waveform = "UNKNOWN";
}


// ---------------- SERIAL ----------------

void sendSamplesToPC() {

  for (int i = 0; i < N; i++) {

    Serial.print(samples[i]);

    if (i < N - 1)
      Serial.print(",");
  }

  Serial.println();
}


// ---------------- CONTROLS ----------------

void handleControls() {

  int potValue =
    analogRead(POT_PIN);

  bool currentSwitchState =
    digitalRead(SWITCH_PIN);

  if (
    previousSwitchState == HIGH &&
    currentSwitchState == LOW
  ) {

    if (currentPage == 0)
      currentPage = selectedItem + 1;

    else
      currentPage = 0;
  }

  previousSwitchState =
    currentSwitchState;


  if (currentPage == 0) {

    selectedItem = map(
      potValue,
      0,
      4095,
      0,
      MENU_COUNT
    );

    if (selectedItem >= MENU_COUNT)
      selectedItem = MENU_COUNT - 1;
  }
}


// ---------------- DISPLAY ----------------

void drawCurrentPage() {

  if (currentPage == 0)
    drawMenu();

  else if (currentPage == 1)
    drawLiveData();

  else if (currentPage == 2)
    drawSpectrum();

  else if (currentPage == 3)
    drawSettings();
}


// ---------------- MENU ----------------

void drawMenu() {

  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(0, 0);

  display.println("SIGNAL ANALYZER");

  display.drawLine(
    0, 10, 127, 10,
    SSD1306_WHITE
  );

  for (int i = 0; i < MENU_COUNT; i++) {

    int y = 18 + i * 14;

    display.setCursor(5, y);

    if (i == selectedItem)
      display.print("> ");
    else
      display.print("  ");

    display.println(menuItems[i]);
  }

  display.display();
}


// ---------------- LIVE DATA ----------------

void drawLiveData() {

  display.clearDisplay();

  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("LIVE DATA");

  display.drawLine(
    0, 10, 127, 10,
    SSD1306_WHITE
  );

  display.setCursor(0, 14);
  display.print("Wave: ");
  display.println(waveform);

  display.setCursor(0, 24);
  display.print("Vpp : ");
  display.print(vpp, 2);
  display.println("V");

  display.setCursor(0, 34);
  display.print("Vrms: ");
  display.print(vrms, 2);
  display.println("V");

  display.setCursor(0, 44);
  display.print("Freq: ");

  if (hasFrequency)
    display.print(frequency, 1);
  else
    display.print("N/A");

  display.println("Hz");

  display.setCursor(0, 54);
  display.print("Duty: ");

  if (
    waveform == "SQUARE" &&
    hasFrequency
  ) {
    display.print(dutyCycle, 1);
    display.print("%");
  }
  else {
    display.print("N/A");
  }

  display.display();
}


// ---------------- SPECTRUM ----------------

void drawSpectrum() {

  display.clearDisplay();

  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("SPECTRUM");

  display.drawLine(
    0, 10, 127, 10,
    SSD1306_WHITE
  );

  display.setCursor(0, 20);
  display.println("FFT + Harmonics");

  display.setCursor(0, 34);
  display.println("PC Processing");

  display.setCursor(0, 48);
  display.println("THD / SPECTRUM");

  display.display();
}


// ---------------- SETTINGS ----------------

void drawSettings() {

  display.clearDisplay();

  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("SETTINGS");

  display.drawLine(
    0, 10, 127, 10,
    SSD1306_WHITE
  );

  display.setCursor(0, 18);
  display.println("Sample: 5 kS/s");

  display.setCursor(0, 30);
  display.println("Buffer: 1024");

  display.setCursor(0, 42);
  display.println("ADC: GPIO34");

  display.setCursor(0, 54);
  display.println("USB: 460800");

  display.display();
}
