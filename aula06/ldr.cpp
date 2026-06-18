#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>

// =======================
// CONFIGURAÇÃO DO ACCESS POINT
// =======================
const char* AP_SSID = "aula06_grupoC";
const char* AP_PASSWORD = "12345678"; // mínimo de 8 caracteres

// =======================
// PINOS DO ESP32-C3
// =======================
#define LDR_PIN 0          // GPIO0 - entrada analógica ADC
#define BUTTON_SOS_PIN 3   // GPIO3 - botão SOS com interrupção
#define RGB_LED_PIN 8      // GPIO8 - LED RGB interno NeoPixel
#define RGB_LED_COUNT 1

Adafruit_NeoPixel led(RGB_LED_COUNT, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

// =======================
// SERVIDOR WEB
// =======================
WebServer server(80);

// =======================
// PARÂMETROS DO SISTEMA
// =======================
const int ADC_MAX_VALUE = 4095;

const int LDR_DARK_THRESHOLD = 2500;

// Leitura do LDR a 1 Hz
const unsigned long LDR_READ_INTERVAL_MS = 1000;

// Debounce do botão
const unsigned long DEBOUNCE_TIME_MS = 50;

// SOS ativo por 3 segundos
const unsigned long SOS_DURATION_MS = 3000;

// Pisca amarelo em 1 Hz:
// 500 ms aceso + 500 ms apagado = 1 ciclo por segundo.
const unsigned long YELLOW_BLINK_HALF_PERIOD_MS = 500;

// =======================
// VARIÁVEIS GLOBAIS
// =======================
volatile bool sosInterruptFlag = false;

int ldrValue = 0;
int lightPercent = 0;

unsigned long lastLdrReadTime = 0;
unsigned long lastValidButtonTime = 0;
unsigned long sosActiveUntil = 0;

unsigned long sosCounter = 0;

// =======================
// INTERRUPÇÃO DO BOTÃO SOS
// =======================
void IRAM_ATTR handleSosInterrupt() {
  sosInterruptFlag = true;
}

// =======================
// CONTROLE DO LED RGB
// =======================
void setRgbLed(uint8_t red, uint8_t green, uint8_t blue) {
  led.setPixelColor(0, led.Color(red, green, blue));
  led.show();
}

void ledOff() {
  setRgbLed(0, 0, 0);
}

void ledRed() {
  setRgbLed(255, 0, 0);
}

void ledYellow() {
  setRgbLed(255, 255, 0);
}

// =======================
// LÓGICA DO SISTEMA
// =======================
bool isLowLight() {
  return ldrValue >= LDR_DARK_THRESHOLD;
}

bool isSosActive() {
  return millis() < sosActiveUntil;
}

String getSystemState() {
  if (isSosActive()) {
    return "EMERGENCIA_SOS";
  }

  if (isLowLight()) {
    return "BAIXA_LUMINOSIDADE";
  }

  return "NORMAL";
}

void updateLedState() {
  unsigned long now = millis();

  // Prioridade máxima: SOS
  if (isSosActive()) {
    ledRed();
    return;
  }

  // Segunda prioridade: baixa luminosidade
  if (isLowLight()) {
    bool blinkOn = ((now / YELLOW_BLINK_HALF_PERIOD_MS) % 2) == 0;

    if (blinkOn) {
      ledYellow();
    } else {
      ledOff();
    }

    return;
  }

  // Estado normal
  ledOff();
}

void readLdrIfNeeded() {
  unsigned long now = millis();

  if (now - lastLdrReadTime >= LDR_READ_INTERVAL_MS) {
    lastLdrReadTime = now;

    ldrValue = analogRead(LDR_PIN);

    // Nesta montagem, ADC alto = menos luz.
    lightPercent = map(ldrValue, 0, ADC_MAX_VALUE, 100, 0);

    Serial.print("ADC LDR: ");
    Serial.print(ldrValue);
    Serial.print(" | Luminosidade estimada: ");
    Serial.print(lightPercent);
    Serial.print("% | Estado: ");
    Serial.println(getSystemState());
  }
}

void processSosInterruptIfNeeded() {
  if (!sosInterruptFlag) {
    return;
  }

  noInterrupts();
  sosInterruptFlag = false;
  interrupts();

  unsigned long now = millis();

  // Debounce por software
  if (now - lastValidButtonTime >= DEBOUNCE_TIME_MS) {
    lastValidButtonTime = now;

    sosActiveUntil = now + SOS_DURATION_MS;
    sosCounter++;

    Serial.println("SOS acionado: LED vermelho fixo por 3 segundos.");
  }
}

// =======================
// PÁGINA WEB
// =======================
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <title>ESP32-C3 - Sistema de Monitoramento</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      margin: 40px;
      background: #f4f4f4;
      color: #222;
    }
    .card {
      background: white;
      padding: 24px;
      border-radius: 12px;
      max-width: 650px;
      box-shadow: 0 2px 10px rgba(0,0,0,0.15);
    }
    h1 {
      margin-top: 0;
    }
    .value {
      font-size: 1.4em;
      font-weight: bold;
    }
    .normal {
      color: green;
    }
    .low {
      color: orange;
    }
    .sos {
      color: red;
    }
  </style>
</head>
<body>
  <div class="card">
    <h1>ESP32-C3 - Sistema de Monitoramento Inteligente</h1>

    <p>Valor ADC do LDR:</p>
    <p class="value" id="adc">--</p>

    <p>Luminosidade estimada:</p>
    <p class="value" id="light">--</p>

    <p>Estado do sistema:</p>
    <p class="value" id="state">--</p>

    <p>Eventos SOS registrados:</p>
    <p class="value" id="sosCounter">--</p>

    <p>Atualização automática a cada 1 segundo.</p>
  </div>

  <script>
    async function updateData() {
      try {
        const response = await fetch('/dados');
        const data = await response.json();

        document.getElementById('adc').textContent = data.ldr_adc;
        document.getElementById('light').textContent = data.luminosidade_percentual + '%';
        document.getElementById('sosCounter').textContent = data.sos_counter;

        const stateElement = document.getElementById('state');
        stateElement.textContent = data.estado;

        stateElement.className = 'value';

        if (data.estado === 'NORMAL') {
          stateElement.classList.add('normal');
        } else if (data.estado === 'BAIXA_LUMINOSIDADE') {
          stateElement.classList.add('low');
        } else if (data.estado === 'EMERGENCIA_SOS') {
          stateElement.classList.add('sos');
        }
      } catch (error) {
        console.log('Erro ao atualizar dados:', error);
      }
    }

    setInterval(updateData, 1000);
    updateData();
  </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

void handleData() {
  unsigned long now = millis();

  long remainingSosMs = 0;
  if (isSosActive()) {
    remainingSosMs = sosActiveUntil - now;
  }

  String json = "{";
  json += "\"ldr_adc\":";
  json += ldrValue;
  json += ",";
  json += "\"luminosidade_percentual\":";
  json += lightPercent;
  json += ",";
  json += "\"limiar_baixa_luminosidade\":";
  json += LDR_DARK_THRESHOLD;
  json += ",";
  json += "\"estado\":\"";
  json += getSystemState();
  json += "\",";
  json += "\"sos_ativo\":";
  json += isSosActive() ? "true" : "false";
  json += ",";
  json += "\"sos_restante_ms\":";
  json += remainingSosMs;
  json += ",";
  json += "\"sos_counter\":";
  json += sosCounter;
  json += "}";

  server.send(200, "application/json", json);
}

void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/dados", handleData);
  server.begin();

  Serial.println("Servidor web iniciado.");
}

// =======================
// SETUP DO ACCESS POINT
// =======================
void setupAccessPoint() {
  WiFi.mode(WIFI_AP);

  bool apStarted = WiFi.softAP(AP_SSID, AP_PASSWORD);

  if (apStarted) {
    Serial.println("Access Point criado com sucesso.");
    Serial.print("Nome da rede: ");
    Serial.println(AP_SSID);
    Serial.print("Senha: ");
    Serial.println(AP_PASSWORD);
    Serial.print("IP do ESP32-C3: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("Erro ao criar o Access Point.");
  }
}

// =======================
// SETUP
// =======================
void setup() {
  Serial.begin(115200);
  delay(1000);

  led.begin();
  led.setBrightness(30);
  ledOff();

  pinMode(BUTTON_SOS_PIN, INPUT_PULLUP);

  attachInterrupt(
    digitalPinToInterrupt(BUTTON_SOS_PIN),
    handleSosInterrupt,
    FALLING
  );

  analogReadResolution(12);
  analogSetPinAttenuation(LDR_PIN, ADC_11db);

  Serial.println();
  Serial.println("Iniciando ESP32-C3 em modo Access Point...");

  setupAccessPoint();
  setupWebServer();

  ldrValue = analogRead(LDR_PIN);
  lightPercent = map(ldrValue, 0, ADC_MAX_VALUE, 100, 0);
}

// =======================
// LOOP PRINCIPAL
// =======================
void loop() {
  server.handleClient();

  processSosInterruptIfNeeded();

  readLdrIfNeeded();

  updateLedState();
}