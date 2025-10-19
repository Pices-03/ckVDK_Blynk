#define BLYNK_TEMPLATE_ID "TMPL6IBTIKSK5"
#define BLYNK_TEMPLATE_NAME "Fan"
#define BLYNK_AUTH_TOKEN "0i5yaT9UD8IBxW-qiumACq8JDc6dhb02"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Wi-Fi credentials
char ssid[] = "Lâu đài tình ái";
char pass[] = "tupham2708";

// DHT Sensor
#define DHTPIN 1
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// Motor control
#define MOTOR_IN1 9
#define MOTOR_IN2 8
#define ENA 6
#define CHANNEL 4
#define FREQUENCY 5000
#define RESOLUTION 8

// LEDs
#define LED_AUTO 19
#define LED_MAN 18

// OLED setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// Physical buttons
#define BUTTON_MODE_PIN 5   // Button for mode switching
#define BUTTON_SPEED_PIN 7  // Button for speed control in Manual mode

// Virtual Pins
#define VPIN_MODE V0
#define VPIN_LEVEL V1
#define VPIN_TEMP V2
#define VPIN_FANSPEED V8    // Virtual pin for Fan Speed
#define VPIN_LED_AUTO V4
#define VPIN_LED_MAN V5
#define VPIN_TIME_OFF V7    // Virtual pin for Time Off slider
#define VPIN_TIME_GAUGE V6  // Virtual pin for Time Off radial gauge
#define VPIN_TIME_S V9
// I2C Pins for OLED
#define OLED_SDA 2
#define OLED_SCL 3

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

bool isAutoMode = false;
float temperature = 0;
int fanSpeed = 0;           // Variable for Fan Speed
int manualSpeedLevel = 0;   // 0: Off, 1: Low, 2: Medium, 3: High

// Time Off variables
unsigned long timeOffDuration = 0;   // Time off duration in milliseconds
unsigned long timeOffStart = 0;      // Time when the timer started
bool isTimeOffActive = false;


// Sleep variables
unsigned long motorStopTime = 0;
bool isSleeping = false;
const unsigned long sleepTimeout = 30000; // 30 seconds

// Debounce variables for mode button
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 10;  // Reduced to 10 ms for sensitivity
bool lastButtonState = HIGH;
bool currentButtonState = HIGH;

// Debounce variables for speed button
unsigned long lastSpeedDebounceTime = 0;
unsigned long speedDebounceDelay = 10;  // Reduced to 10 ms for sensitivity
bool lastSpeedButtonState = HIGH;
bool currentSpeedButtonState = HIGH;

void motor(int speed) {
  digitalWrite(MOTOR_IN1, HIGH);
  digitalWrite(MOTOR_IN2, LOW);
  ledcWrite(CHANNEL, speed);
}

void wakeUpSystem() {
  if (isSleeping) {
    isSleeping = false;
    display.ssd1306_command(SSD1306_DISPLAYON);  // Turn on OLED
    //digitalWrite(LED_AUTO, isAutoMode ? HIGH : LOW);
    //digitalWrite(LED_MAN, isAutoMode ? LOW : HIGH);
  }
  motorStopTime = millis(); 
}

void sleepSystem() {
  isSleeping = true;
  display.ssd1306_command(SSD1306_DISPLAYOFF);  // Turn off OLED
//  digitalWrite(LED_AUTO, LOW);  // Turn off Auto LED
//  digitalWrite(LED_MAN, LOW);   // Turn off Manual LED
}

void setup() {
  Serial.begin(115200);

  // Setup pins
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(LED_AUTO, OUTPUT);
  pinMode(LED_MAN, OUTPUT);
  pinMode(BUTTON_MODE_PIN, INPUT_PULLUP);
  pinMode(BUTTON_SPEED_PIN, INPUT_PULLUP);

  // Initialize DHT sensor
  dht.begin();

  // Initialize motor PWM
  ledcSetup(CHANNEL, FREQUENCY, RESOLUTION);
  ledcAttachPin(ENA, CHANNEL);

  // Initialize OLED display
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("OLED allocation failed"));
    for (;;);
  }

  // Clear display and initialize message
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("OLED is working!");
  display.display();

  // Blynk setup
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
}


void loop() {
  Blynk.run();

  if (isSleeping==false){
    display.ssd1306_command(SSD1306_DISPLAYON);  
    }
  
  // Wake up system if any button is pressed
  if (digitalRead(BUTTON_MODE_PIN) == LOW || digitalRead(BUTTON_SPEED_PIN) == LOW) {
    wakeUpSystem();
  }

  // Read mode button state with debounce
  bool modeReading = digitalRead(BUTTON_MODE_PIN);
  if (modeReading == LOW) {
    wakeUpSystem();  // Wake up immediately on button press
  }

  if (modeReading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (modeReading != currentButtonState) {
      currentButtonState = modeReading;
      if (currentButtonState == LOW) {
        isAutoMode = !isAutoMode;
        manualSpeedLevel = 0;
        Serial.print("Mode changed to: ");
        Serial.println(isAutoMode ? "Auto" : "Manual");

        Blynk.virtualWrite(VPIN_MODE, isAutoMode ? 1 : 0);
      }
    }
  }
  lastButtonState = modeReading;

  // Read speed button state with debounce
  bool speedReading = digitalRead(BUTTON_SPEED_PIN);
  if (speedReading == LOW) {
    wakeUpSystem();  // Wake up immediately on button press
  }

  if (speedReading != lastSpeedButtonState) {
    lastSpeedDebounceTime = millis();
  }

  if ((millis() - lastSpeedDebounceTime) > speedDebounceDelay) {
    if (speedReading != currentSpeedButtonState) {
      currentSpeedButtonState = speedReading;
      if (currentSpeedButtonState == LOW && !isAutoMode) {
        manualSpeedLevel = (manualSpeedLevel + 1) % 4;
        Serial.print("Manual Speed Level: ");
        Serial.println(manualSpeedLevel);

        Blynk.virtualWrite(VPIN_LEVEL, manualSpeedLevel);
      }
    }
  }
  lastSpeedButtonState = speedReading;

  // Read temperature
  temperature = dht.readTemperature();

  // Update temperature on Blynk
  Blynk.virtualWrite(VPIN_TEMP, temperature);

  // Check if system should sleep
  if (fanSpeed == 0 && millis() - motorStopTime >= sleepTimeout) {
    sleepSystem();
  } else if (fanSpeed > 0) {
    motorStopTime = millis();  // Reset the motor stop timer if the motor is running
  }

  // Update Time Off timer
  // Update Time Off timer
if (isTimeOffActive) {
  unsigned long elapsedTime = millis() - timeOffStart;
  unsigned long remainingTime = (elapsedTime >= timeOffDuration) ? 0 : (timeOffDuration - elapsedTime);
  wakeUpSystem();
  // Kiểm tra nếu thời gian đã hết
  if (remainingTime == 0 && isTimeOffActive) {
    fanSpeed = 0;             // Tắt quạt
    motor(0);                 // Dừng motor
    isTimeOffActive = false;  // Tắt trạng thái hẹn giờ
    isAutoMode = false;       // Chuyển sang chế độ Manual
    manualSpeedLevel = 0;     // Đặt mức tốc độ về 0

    Serial.println("Time Off reached, Fan Speed set to 0");

    // Cập nhật trạng thái lên Blynk
    Blynk.virtualWrite(VPIN_FANSPEED, fanSpeed);
    Blynk.virtualWrite(VPIN_MODE, 0);  // Chuyển chế độ về Manual
    Blynk.virtualWrite(VPIN_TIME_GAUGE, 0);  // Đặt thanh đo về 0
    Blynk.virtualWrite(VPIN_LEVEL, 0);
    Blynk.virtualWrite(VPIN_TIME_OFF, 0);
  }

  // Tính toán giờ, phút, giây còn lại
  unsigned long hours = remainingTime / 3600000;
  unsigned long minutes = (remainingTime % 3600000) / 60000;
  unsigned long seconds = (remainingTime % 60000) / 1000;

  // Cập nhật OLED hiển thị
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Temp: ");
  display.print(temperature);
  display.print(" C");

  display.setCursor(0, 20);
  display.print("Mode: ");
  display.print(isAutoMode ? "Auto" : "Manual");

  display.setCursor(0, 40);
  display.print("Fan Speed: ");
  display.print(fanSpeed);

  display.setCursor(0, 50);
  display.print("Time Off: ");
  display.printf("%02d:%02d:%02d", hours, minutes, seconds);
  display.display();

  // Cập nhật thanh đo trên Blynk
  int timeOffGaugeValue = map(remainingTime, 0, timeOffDuration, 0, 100);
  Blynk.virtualWrite(VPIN_TIME_GAUGE, hours * 60 + minutes);
  Blynk.virtualWrite(VPIN_TIME_S, seconds);
} else {
  // Khi không có hẹn giờ hoạt động, cập nhật OLED bình thường
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Temp: ");
  display.print(temperature);
  display.print(" C");

  display.setCursor(0, 20);
  display.print("Mode: ");
  display.print(isAutoMode ? "Auto" : "Manual");

  display.setCursor(0, 40);
  display.print("Fan Speed: ");
  display.print(fanSpeed);
  display.display();
}

  // Motor and LED control
  if (isAutoMode) {
    digitalWrite(LED_AUTO, HIGH);  // Auto mode LED on
    digitalWrite(LED_MAN, LOW);    // Manual mode LED off

    if (temperature < 15) {
      fanSpeed = 0;  motor(0);
    } else if (temperature < 25) {
      fanSpeed = 1;  motor(150);
    } else if (temperature < 30) {
      fanSpeed = 2;  motor(200);
    } else {
      fanSpeed = 3;  motor(255);
    }

    // Update virtual LEDs on Blynk
    Blynk.virtualWrite(VPIN_LED_AUTO, 255);  // Auto LED ON
    Blynk.virtualWrite(VPIN_LED_MAN, 0);     // Manual LED OFF
  } else {
    digitalWrite(LED_AUTO, LOW);   // Auto mode LED off
    digitalWrite(LED_MAN, HIGH);   // Manual mode LED on

    switch (manualSpeedLevel) {
      case 0: fanSpeed = 0;  motor(0); break;
      case 1: fanSpeed = 1;  motor(150); break;
      case 2: fanSpeed = 2;  motor(200); break;
      case 3: fanSpeed = 3;  motor(255); break;
    }

    // Update virtual LEDs on Blynk
    Blynk.virtualWrite(VPIN_LED_AUTO, 0);    // Auto LED OFF
    Blynk.virtualWrite(VPIN_LED_MAN, 255);   // Manual LED ON
  }

  // Update motor speed
  // Motor speed mapped from fanSpeed (0 to 3)
  Blynk.virtualWrite(VPIN_FANSPEED, fanSpeed); // Update fan speed on Blynk
}

// Blynk button for Mode (Auto/Manual)
BLYNK_WRITE(VPIN_MODE) {
  wakeUpSystem();
  isAutoMode = param.asInt();  // Set mode from virtual pin
  manualSpeedLevel = 0;        // Reset speed to off in auto mode
  
  // Update virtual LEDs when mode changes
  if (isAutoMode) {
    Blynk.virtualWrite(VPIN_LED_AUTO, 255);  // Auto LED ON
    Blynk.virtualWrite(VPIN_LED_MAN, 0);     // Manual LED OFF
  } else {
    Blynk.virtualWrite(VPIN_LED_AUTO, 0);    // Auto LED OFF
    Blynk.virtualWrite(VPIN_LED_MAN, 255);   // Manual LED ON
  }
}

// Blynk number input for Speed Level
BLYNK_WRITE(VPIN_LEVEL) {
  wakeUpSystem();
  manualSpeedLevel = param.asInt();
}

// Blynk slider for Time Off
BLYNK_WRITE(VPIN_TIME_OFF) {
  timeOffDuration = param.asInt() * 60000;  // Convert from minutes to milliseconds
  timeOffStart = millis();
  isTimeOffActive = true;
}
