// Calculadora de 4 bits em complemento de 2
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>

// ======================
// Configuração Wi-Fi AP
// ======================

const char* ssid = "Calculadora_ESP32";
const char* password = "12345678";

// Servidor HTTP na porta 80
WebServer server(80);

// ======================
// NeoPixel onboard
// ======================

#define NEOPIXEL_PIN 8
#define NEOPIXEL_COUNT 1

Adafruit_NeoPixel onboardLed(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// ======================
// LEDs externos
// ======================
//
// Mapeamento:
// GPIO4 = bit 3, mais significativo
// GPIO5 = bit 2
// GPIO6 = bit 1
// GPIO7 = bit 0, menos significativo

#define LED_BIT0 7
#define LED_BIT1 6
#define LED_BIT2 5
#define LED_BIT3 4

// ======================
// Funções auxiliares
// ======================

bool isBinary4(String value) {
  if (value.length() != 4) {
    return false;
  }

  for (int i = 0; i < 4; i++) {
    if (value[i] != '0' && value[i] != '1') {
      return false;
    }
  }

  return true;
}

int binaryStringToUnsigned4(String value) {
  int result = 0;

  for (int i = 0; i < 4; i++) {
    result = result << 1;

    if (value[i] == '1') {
      result = result | 1;
    }
  }

  return result & 0x0F;
}

int toSigned4(int value) {
  value = value & 0x0F;

  if (value & 0x08) {
    return value - 16;
  }

  return value;
}

void updateOutputLeds(int result4bits) {
  result4bits = result4bits & 0x0F;

  digitalWrite(LED_BIT0, result4bits & 0x01);
  digitalWrite(LED_BIT1, result4bits & 0x02);
  digitalWrite(LED_BIT2, result4bits & 0x04);
  digitalWrite(LED_BIT3, result4bits & 0x08);
}

void setStatusLed(bool overflow) {
  if (overflow) {
    onboardLed.setPixelColor(0, onboardLed.Color(255, 0, 0)); // vermelho
  } else {
    onboardLed.setPixelColor(0, onboardLed.Color(0, 80, 0)); // verde
  }

  onboardLed.show();
}

String toBinary4(int value) {
  value = value & 0x0F;

  String output = "";

  for (int i = 3; i >= 0; i--) {
    if (value & (1 << i)) {
      output += "1";
    } else {
      output += "0";
    }
  }

  return output;
}

bool detectOverflowAdd(int aSigned, int bSigned, int resultSigned) {
  bool aPositive = aSigned >= 0;
  bool bPositive = bSigned >= 0;
  bool resultPositive = resultSigned >= 0;

  return (aPositive == bPositive) && (resultPositive != aPositive);
}

bool detectOverflowSub(int aSigned, int bSigned, int resultSigned) {
  bool aPositive = aSigned >= 0;
  bool bPositive = bSigned >= 0;
  bool resultPositive = resultSigned >= 0;

  return (aPositive != bPositive) && (resultPositive != aPositive);
}

// ======================
// Página HTML
// ======================

String makePage(String a = "0011", String b = "0010", String op = "add",
                String resultBin = "----", String resultDec = "--",
                String status = "Aguardando cálculo", bool overflow = false) {

  String checkedAdd = "";
  String checkedSub = "";

  if (op == "sub") {
    checkedSub = "checked";
  } else {
    checkedAdd = "checked";
  }

  String statusClass = overflow ? "overflow" : "ok";

  String html = "";

  html += "<!DOCTYPE html>";
  html += "<html lang='pt-BR'>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Calculadora ESP32</title>";

  html += "<style>";
  html += "body{font-family:Arial;background:#f4f6f8;margin:0;padding:20px;color:#222;}";
  html += ".box{max-width:520px;margin:auto;background:white;padding:20px;border-radius:12px;box-shadow:0 0 12px #ccc;}";
  html += "h1{text-align:center;font-size:26px;}";
  html += "label{display:block;margin-top:14px;font-weight:bold;}";
  html += "input[type=text]{width:100%;font-size:22px;padding:10px;margin-top:6px;box-sizing:border-box;text-align:center;letter-spacing:4px;}";
  html += ".ops{margin-top:14px;font-size:18px;}";
  html += "button{width:100%;font-size:20px;margin-top:20px;padding:12px;border:0;border-radius:8px;background:#1e88e5;color:white;}";
  html += ".result{margin-top:22px;padding:14px;border-radius:8px;background:#eef3f8;font-size:18px;}";
  html += ".ok{color:#0a8f08;font-weight:bold;}";
  html += ".overflow{color:#d00000;font-weight:bold;}";
  html += ".small{font-size:14px;color:#666;margin-top:20px;}";
  html += "</style>";

  html += "</head>";
  html += "<body>";
  html += "<div class='box'>";

  html += "<h1>Calculadora ESP32-C3</h1>";

  html += "<form action='/calc' method='GET'>";
  html += "<label>Operando A - 4 bits</label>";
  html += "<input type='text' name='a' maxlength='4' value='" + a + "' pattern='[01]{4}' required>";

  html += "<label>Operando B - 4 bits</label>";
  html += "<input type='text' name='b' maxlength='4' value='" + b + "' pattern='[01]{4}' required>";

  html += "<div class='ops'>";
  html += "<label><input type='radio' name='op' value='add' " + checkedAdd + "> Soma</label>";
  html += "<label><input type='radio' name='op' value='sub' " + checkedSub + "> Subtração</label>";
  html += "</div>";

  html += "<button type='submit'>Calcular</button>";
  html += "</form>";

  html += "<div class='result'>";
  html += "<p><strong>Resultado binário:</strong> " + resultBin + "</p>";
  html += "<p><strong>Resultado decimal:</strong> " + resultDec + "</p>";
  html += "<p><strong>Status:</strong> <span class='" + statusClass + "'>" + status + "</span></p>";
  html += "</div>";

  html += "<div class='small'>";
  html += "<p>Faixa em complemento de dois com 4 bits: -8 até +7.</p>";
  html += "<p>GPIO4 = bit0, GPIO5 = bit1, GPIO6 = bit2, GPIO7 = bit3.</p>";
  html += "</div>";

  html += "</div>";
  html += "</body>";
  html += "</html>";

  return html;
}

// ======================
// Rotas HTTP
// ======================

void handleRoot() {
  server.send(200, "text/html", makePage());
}

void handleCalc() {
  String aStr = server.arg("a");
  String bStr = server.arg("b");
  String op = server.arg("op");

  if (!isBinary4(aStr) || !isBinary4(bStr)) {
    server.send(400, "text/html", makePage(aStr, bStr, op, "----", "--", "Erro: use exatamente 4 bits em A e B", true));
    return;
  }

  if (op != "add" && op != "sub") {
    op = "add";
  }

  int aRaw = binaryStringToUnsigned4(aStr);
  int bRaw = binaryStringToUnsigned4(bStr);

  int aSigned = toSigned4(aRaw);
  int bSigned = toSigned4(bRaw);

  int fullResult;

  if (op == "add") {
    fullResult = aSigned + bSigned;
  } else {
    fullResult = aSigned - bSigned;
  }

  int result4bits = fullResult & 0x0F;
  int resultSigned = toSigned4(result4bits);

  bool overflow;

  if (op == "add") {
    overflow = detectOverflowAdd(aSigned, bSigned, resultSigned);
  } else {
    overflow = detectOverflowSub(aSigned, bSigned, resultSigned);
  }

  updateOutputLeds(result4bits);
  setStatusLed(overflow);

  String resultBin = toBinary4(result4bits);
  String resultDec = String(resultSigned);

  String status;

  if (overflow) {
    status = "OVERFLOW";
  } else {
    status = "OK";
  }

  server.send(200, "text/html", makePage(aStr, bStr, op, resultBin, resultDec, status, overflow));
}

// ======================
// Setup e loop
// ======================

void setup() {
  Serial.begin(115200);

  pinMode(LED_BIT0, OUTPUT);
  pinMode(LED_BIT1, OUTPUT);
  pinMode(LED_BIT2, OUTPUT);
  pinMode(LED_BIT3, OUTPUT);

  updateOutputLeds(0);

  onboardLed.begin();
  onboardLed.setBrightness(30);
  onboardLed.setPixelColor(0, onboardLed.Color(0, 0, 30)); // azul: iniciado
  onboardLed.show();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  Serial.println();
  Serial.println("WiFi AP iniciado");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("Senha: ");
  Serial.println(password);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/calc", handleCalc);

  server.begin();

  Serial.println("Servidor HTTP iniciado");
}

void loop() {
  server.handleClient();
}