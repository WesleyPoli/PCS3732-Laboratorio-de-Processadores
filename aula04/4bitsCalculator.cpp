// Após conectar na rede wi-fi, acessar o front pelo IP 192.168.4.1
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>

// ======================
// Configuração Wi-Fi AP
// ======================

const char* ssid = "Calculadora_GrupoC";
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
 
// Calcula fatorial 
  int fatorial(int n){
    if(n <= 1){
      return 1; 
    }

    int resultado = 1;

    for(int i = 2; i <= n; i++){
      resultado *= i;
    }

    return resultado;
  }

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

int toSigned4(int value) {
  value = value & 0x0F;

  if (value & 0x08) {
    return value - 16;
  }

  return value;
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

void updateOutputLeds(int resultado) {
  // 3. Mascaramento, garantindo 4 bits
  resultado = resultado & 0x0F;

  // 4. Output para GPIO
  digitalWrite(LED_BIT0, resultado & 0x01);
  digitalWrite(LED_BIT1, resultado & 0x02);
  digitalWrite(LED_BIT2, resultado & 0x04);
  digitalWrite(LED_BIT3, resultado & 0x08);
}

void setStatusLed(bool overflow, int resultado4bits
) {
  if (overflow) {
    // Overflow tem prioridade sobre positivo, negativo ou zero
    onboardLed.setPixelColor(0, onboardLed.Color(80, 80, 0)); // amarelo
  } else if (resultado4bits
     == 0) {
    onboardLed.setPixelColor(0, onboardLed.Color(0, 0, 80)); // azul
  } else if (resultado4bits
     > 0) {
    onboardLed.setPixelColor(0, onboardLed.Color(0, 80, 0)); // verde
  } else {
    onboardLed.setPixelColor(0, onboardLed.Color(80, 0, 0)); // vermelho
  }

  onboardLed.show();
}

bool detectOverflowAdd(int valA, int valB, int resultado4bits
) {
  bool aPositive = valA >= 0;
  bool bPositive = valB >= 0;
  bool resultPositive = resultado4bits
   >= 0;

  return (aPositive == bPositive) && (resultPositive != aPositive);
}

bool detectOverflowSub(int valA, int valB, int resultado4bits
) {
  bool aPositive = valA >= 0;
  bool bPositive = valB >= 0;
  bool resultPositive = resultado4bits
   >= 0;

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
  String checkedMul = "";
  String checkedFat = "";

  if (op == "sub") {
    checkedSub = "checked";
  } else if (op == "mul") {
    checkedMul = "checked";
  } else if (op == "fat") {
    checkedFat = "checked";
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

  html += "<div id='campoB'>";
  html += "<label>Operando B - 4 bits</label>";
  html += "<input id='inputB' type='text' name='b' maxlength='4' value='" + b + "' pattern='[01]{4}' required>";
  html += "</div>";

  html += "<div class='ops'>";
  html += "<label><input type='radio' name='op' value='add' " + checkedAdd + " onchange='toggleB()'> Soma</label>";
  html += "<label><input type='radio' name='op' value='sub' " + checkedSub + " onchange='toggleB()'> Subtração</label>";
  html += "<label><input type='radio' name='op' value='mul' " + checkedMul + " onchange='toggleB()'> Multiplicação</label>";
  html += "<label><input type='radio' name='op' value='fat' " + checkedFat + " onchange='toggleB()'> Fatorial de A</label>";
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
  html += "<p>GPIO4 = bit3, GPIO5 = bit2, GPIO6 = bit1, GPIO7 = bit0.</p>";
  html += "</div>";

  html += "</div>";

  html += "<script>";
  html += "function toggleB(){";
  html += "  var fat = document.querySelector(\"input[name='op'][value='fat']\").checked;";
  html += "  var campoB = document.getElementById('campoB');";
  html += "  var inputB = document.getElementById('inputB');";
  html += "  if(fat){";
  html += "    campoB.style.display = 'none';";
  html += "    inputB.required = false;";
  html += "  } else {";
  html += "    campoB.style.display = 'block';";
  html += "    inputB.required = true;";
  html += "  }";
  html += "}";
  html += "window.onload = toggleB;";
  html += "</script>";

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
  String paramA = server.arg("a");
  String paramB = server.arg("b");
  String op = server.arg("op");

  if (op != "add" && op != "sub" && op != "mul" && op != "fat") {
    op = "add";
  }

  if (op == "fat") {
    if (!isBinary4(paramA)) {
      server.send(400, "text/html", makePage(paramA, paramB, op, "----", "--", "Erro: use exatamente 4 bits em A", true));
      return;
    }

    // No fatorial, o operando B não é usado.
    paramB = "0000";
  } else {
    if (!isBinary4(paramA) || !isBinary4(paramB)) {
      server.send(400, "text/html", makePage(paramA, paramB, op, "----", "--", "Erro: use exatamente 4 bits em A e B", true));
      return;
    }
  }

  // 1. Parsing: string binária para inteiro
  int valA_raw = strtol(paramA.c_str(), NULL, 2);
  int valB_raw = strtol(paramB.c_str(), NULL, 2);

  // Complemento de dois: interpretação com sinal de 4 bits
  int valA = toSigned4(valA_raw);
  int valB = toSigned4(valB_raw);

  // 2. Operação aritmética em C++ nativo
  int resultadoCompleto = (op == "add") ? (valA + valB)
                        : (op == "sub") ? (valA - valB)
                        : (op == "mul") ? (valA * valB)
                        : (op == "fat") ? fatorial(valA)
                        : 0;

  // 3. Mascaramento, garantindo 4 bits
  int resultado = resultadoCompleto & 0x0F;

  // Complemento de dois: resultado assinado de 4 bits
  int resultado4bits
   = toSigned4(resultado);

  // Detecção de overflow
  bool overflow = false;

  if (op == "add") {
    overflow = detectOverflowAdd(valA, valB, resultado4bits
    );
  } else if (op == "sub") {
    overflow = detectOverflowSub(valA, valB, resultado4bits
    );
  } else if (op == "mul" || op == "fat") {
    // Em 4 bits com sinal, a faixa representável é de -8 até +7.
    overflow = (resultadoCompleto < -8 || resultadoCompleto > 7);
  }

  // 4. Output para GPIO
  updateOutputLeds(resultado);

  // LED onboard indica status
  setStatusLed(overflow, resultado4bits
  );

  String resultBin = toBinary4(resultado);
  String resultDec = String(resultado4bits
  );

  String status;

  if (overflow) {
    status = "OVERFLOW";
  } else {
    status = "OK";
  }

  server.send(200, "text/html", makePage(paramA, paramB, op, resultBin, resultDec, status, overflow));
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