#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <ESP32Servo.h>

#if __has_include(<esp_arduino_version.h>)
  #include <esp_arduino_version.h>
#endif

#ifndef ESP_ARDUINO_VERSION_MAJOR
  #define ESP_ARDUINO_VERSION_MAJOR 2
#endif

// ======================
// Configuração Wi-Fi AP
// ======================

const char* ssid = "ESP32_PWM_Control";
const char* password = "12345678";

WebServer server(80);

// ======================
// NeoPixel onboard
// ======================

#define NEOPIXEL_PIN 8
#define NEOPIXEL_COUNT 1

Adafruit_NeoPixel onboardLed(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// ======================
// Pinagem
// ======================
//
// LED_PWM_PIN:
// GPIO7 controla o brilho do LED externo.
//
// SERVO_PWM_PIN:
// GPIO4 controla o sinal do servo, igual ao código que funcionou.

#define LED_PWM_PIN 7
#define SERVO_PWM_PIN 4

// ======================
// PWM do LED
// ======================

#define LED_PWM_CHANNEL 4
#define LED_PWM_FREQ 5000
#define LED_PWM_RESOLUTION 8

// ======================
// Servo com ESP32Servo
// ======================
//
// Ajustado para seguir o código que funcionou:
// 50 Hz
// attach com 500 us a 2400 us
// controle por write(angle)

#define SERVO_PWM_FREQ 50
#define SERVO_MIN_US 500
#define SERVO_MAX_US 2400

Servo servoMotor;

// ======================
// Estados atuais
// ======================

int ledPercent = 0;
int servoAngle = 90;

// ======================
// Funções auxiliares LED PWM
// ======================

uint32_t maxDuty(uint8_t resolutionBits) {
  return (1UL << resolutionBits) - 1;
}

bool attachPwmChannel(uint8_t pin, uint8_t channel, uint32_t freq, uint8_t resolution) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  return ledcAttachChannel(pin, freq, resolution, channel);
#else
  double actualFreq = ledcSetup(channel, freq, resolution);
  ledcAttachPin(pin, channel);
  return actualFreq > 0;
#endif
}

void writePwmChannel(uint8_t channel, uint32_t duty) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWriteChannel(channel, duty);
#else
  ledcWrite(channel, duty);
#endif
}

void setLedBrightness(int percent) {
  ledPercent = constrain(percent, 0, 100);

  uint32_t duty = map(
    ledPercent,
    0,
    100,
    0,
    maxDuty(LED_PWM_RESOLUTION)
  );

  writePwmChannel(LED_PWM_CHANNEL, duty);
}

// ======================
// Funções auxiliares Servo
// ======================

void setServoAngle(int angle) {
  servoAngle = constrain(angle, 0, 180);

  servoMotor.write(servoAngle);

  Serial.print("Servo angle: ");
  Serial.println(servoAngle);
}

void setOnboardStatus(uint8_t r, uint8_t g, uint8_t b) {
  onboardLed.setPixelColor(0, onboardLed.Color(r, g, b));
  onboardLed.show();
}

// ======================
// Página HTML
// ======================

String makePage() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32-C3 PWM Control</title>

  <style>
    body {
      font-family: Arial, sans-serif;
      background: #f4f6f8;
      margin: 0;
      padding: 20px;
      color: #222;
    }

    .box {
      max-width: 560px;
      margin: auto;
      background: white;
      padding: 20px;
      border-radius: 12px;
      box-shadow: 0 0 12px #ccc;
    }

    h1 {
      text-align: center;
      font-size: 25px;
      margin-bottom: 8px;
    }

    .subtitle {
      text-align: center;
      font-size: 14px;
      color: #666;
      margin-bottom: 24px;
    }

    .control {
      margin-top: 22px;
      padding: 16px;
      background: #eef3f8;
      border-radius: 10px;
    }

    label {
      display: block;
      font-weight: bold;
      margin-bottom: 10px;
      font-size: 18px;
    }

    input[type=range] {
      width: 100%;
    }

    .value {
      font-size: 22px;
      font-weight: bold;
      text-align: center;
      margin-top: 8px;
    }

    .buttons {
      display: flex;
      gap: 10px;
      margin-top: 14px;
    }

    button {
      flex: 1;
      font-size: 17px;
      padding: 12px;
      border: 0;
      border-radius: 8px;
      background: #1e88e5;
      color: white;
      cursor: pointer;
    }

    button:active {
      background: #1565c0;
    }

    .status {
      margin-top: 22px;
      padding: 12px;
      background: #e8f5e9;
      border-radius: 8px;
      font-weight: bold;
      color: #1b5e20;
      text-align: center;
    }

    .small {
      font-size: 14px;
      color: #666;
      margin-top: 20px;
      line-height: 1.5;
    }
  </style>
</head>

<body>
  <div class="box">
    <h1>ESP32-C3 PWM Control</h1>
    <div class="subtitle">Controle simultâneo de LED e servomotor via interface web</div>

    <div class="control">
      <label for="ledSlider">Brilho do LED</label>
      <input 
        type="range" 
        id="ledSlider" 
        min="0" 
        max="100" 
        step="1" 
        value="{{LED}}"
      >
      <div class="value" id="ledValue">{{LED}}%</div>
    </div>

    <div class="control">
      <label for="servoSlider">Posição do servomotor</label>
      <input 
        type="range" 
        id="servoSlider" 
        min="0" 
        max="180" 
        step="1" 
        value="{{SERVO}}"
      >
      <div class="value" id="servoValue">{{SERVO}}°</div>

      <div class="buttons">
        <button onclick="setServoPreset(0)">0°</button>
        <button onclick="setServoPreset(90)">90°</button>
        <button onclick="setServoPreset(180)">180°</button>
      </div>
    </div>

    <div class="status" id="status">
      Sistema iniciado
    </div>

    <div class="small">
      <p><strong>LED PWM:</strong> GPIO{{LED_PIN}}, 5 kHz, duty cycle de 0 a 100%.</p>
      <p><strong>Servo:</strong> GPIO{{SERVO_PIN}}, 50 Hz, controle por ESP32Servo.</p>
      <p>O LED usa PWM de hardware por LEDC e o servo usa a biblioteca ESP32Servo.</p>
    </div>
  </div>

  <script>
    const ledSlider = document.getElementById("ledSlider");
    const servoSlider = document.getElementById("servoSlider");

    const ledValue = document.getElementById("ledValue");
    const servoValue = document.getElementById("servoValue");

    const statusBox = document.getElementById("status");

    let updateTimer = null;

    function updateValuesOnScreen() {
      ledValue.textContent = ledSlider.value + "%";
      servoValue.textContent = servoSlider.value + "°";
    }

    function sendCommand() {
      updateValuesOnScreen();

      clearTimeout(updateTimer);

      updateTimer = setTimeout(() => {
        const led = ledSlider.value;
        const servo = servoSlider.value;

        fetch(`/set?led=${led}&servo=${servo}`)
          .then(response => response.json())
          .then(data => {
            statusBox.textContent = `Aplicado: LED ${data.led}% | Servo ${data.servo}°`;
          })
          .catch(error => {
            statusBox.textContent = "Erro ao enviar comando para o ESP32";
          });
      }, 120);
    }

    function setServoPreset(angle) {
      servoSlider.value = angle;
      sendCommand();
    }

    ledSlider.addEventListener("input", sendCommand);
    servoSlider.addEventListener("input", sendCommand);
  </script>
</body>
</html>
)rawliteral";

  html.replace("{{LED}}", String(ledPercent));
  html.replace("{{SERVO}}", String(servoAngle));
  html.replace("{{LED_PIN}}", String(LED_PWM_PIN));
  html.replace("{{SERVO_PIN}}", String(SERVO_PWM_PIN));

  return html;
}

// ======================
// Rotas HTTP
// ======================

void handleRoot() {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/html", makePage());
}

void handleSet() {
  if (server.hasArg("led")) {
    int newLedPercent = server.arg("led").toInt();
    setLedBrightness(newLedPercent);
  }

  if (server.hasArg("servo")) {
    int newServoAngle = server.arg("servo").toInt();
    setServoAngle(newServoAngle);
  }

  String json = "{";
  json += "\"led\":" + String(ledPercent) + ",";
  json += "\"servo\":" + String(servoAngle);
  json += "}";

  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("Connection", "close");
  server.send(200, "application/json", json);
}

void handleNotFound() {
  server.send(404, "text/plain", "Rota nao encontrada");
}

// ======================
// Setup e loop
// ======================

void setup() {
  Serial.begin(115200);

  onboardLed.begin();
  onboardLed.setBrightness(25);
  setOnboardStatus(0, 0, 80);

  // Configuração dos timers da ESP32Servo,
  // igual ao código que funcionou.
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  servoMotor.setPeriodHertz(SERVO_PWM_FREQ);
  servoMotor.attach(SERVO_PWM_PIN, SERVO_MIN_US, SERVO_MAX_US);
  servoMotor.write(servoAngle);

  bool ledOk = attachPwmChannel(
    LED_PWM_PIN,
    LED_PWM_CHANNEL,
    LED_PWM_FREQ,
    LED_PWM_RESOLUTION
  );

  bool servoOk = servoMotor.attached();

  if (!ledOk || !servoOk) {
    Serial.println("Erro ao configurar PWM ou servo");
    setOnboardStatus(80, 0, 0);
  } else {
    Serial.println("PWM do LED e servo configurados com sucesso");
    setOnboardStatus(0, 80, 0);
  }

  setLedBrightness(0);
  setServoAngle(90);

  WiFi.mode(WIFI_AP);
  bool apOk = WiFi.softAP(ssid, password);

  Serial.println();

  if (apOk) {
    Serial.println("WiFi AP iniciado");
    Serial.print("SSID: ");
    Serial.println(ssid);
    Serial.print("Senha: ");
    Serial.println(password);
    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("Falha ao iniciar WiFi AP");
    setOnboardStatus(80, 0, 0);
  }

  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.onNotFound(handleNotFound);

  server.begin();

  Serial.println("Servidor HTTP iniciado");
}

void loop() {
  server.handleClient();
}