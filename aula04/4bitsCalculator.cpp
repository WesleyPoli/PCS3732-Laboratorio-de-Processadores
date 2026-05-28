// Após conectar na rede Wi-Fi, acessar o front pelo IP 192.168.4.1
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

// ======================
// Configuração Wi-Fi AP
// ======================

const char* ssid = "Calculadora_GrupoC";
const char* password = "12345678";

// Servidor HTTP na porta 80
WebServer server(80);

// ======================
// Modos da calculadora
// ======================

#define BASE_BITS 4
#define EXPANDED_BITS 16

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
//
// Observação: no modo expandido, os LEDs físicos mostram apenas os 4 bits
// menos significativos do resultado, pois o hardware possui 4 LEDs externos.

#define LED_BIT0 7
#define LED_BIT1 6
#define LED_BIT2 5
#define LED_BIT3 4

// ======================
// Funções auxiliares
// ======================

uint32_t maskForBits(int bits) {
  return (1UL << bits) - 1UL;
}

int32_t minSignedValue(int bits) {
  return -(1L << (bits - 1));
}

int32_t maxSignedValue(int bits) {
  return (1L << (bits - 1)) - 1;
}

String zeroBits(int bits) {
  String output = "";

  for (int i = 0; i < bits; i++) {
    output += "0";
  }

  return output;
}

bool isBinaryN(String value, int bits) {
  if (value.length() != bits) {
    return false;
  }

  for (int i = 0; i < bits; i++) {
    if (value[i] != '0' && value[i] != '1') {
      return false;
    }
  }

  return true;
}

bool isBinary4(String value) {
  return isBinaryN(value, BASE_BITS);
}

int32_t toSignedBits(uint32_t value, int bits) {
  uint32_t mask = maskForBits(bits);
  uint32_t signBit = 1UL << (bits - 1);

  value = value & mask;

  if (value & signBit) {
    return (int32_t)value - (int32_t)(1UL << bits);
  }

  return (int32_t)value;
}

int toSigned4(int value) {
  return (int)toSignedBits((uint32_t)value, BASE_BITS);
}

String toBinaryBits(uint32_t value, int bits) {
  uint32_t mask = maskForBits(bits);
  value = value & mask;

  String output = "";

  for (int i = bits - 1; i >= 0; i--) {
    if (value & (1UL << i)) {
      output += "1";
    } else {
      output += "0";
    }
  }

  return output;
}

String toBinary4(int value) {
  return toBinaryBits((uint32_t)value, BASE_BITS);
}

long long fatorialLimitado(int n, int32_t maxRepresentavel, bool* overflowPorLimite) {
  long long resultado = 1;
  long long limite = (long long)maxRepresentavel;

  *overflowPorLimite = false;

  if (n <= 1) {
    return 1;
  }

  for (int i = 2; i <= n; i++) {
    if (resultado > limite / i) {
      *overflowPorLimite = true;
      return limite + 1;
    }

    resultado *= i;
  }

  return resultado;
}

int fatorial(int n) {
  if (n <= 1) {
    return 1;
  }

  int resultado = 1;

  for (int i = 2; i <= n; i++) {
    resultado *= i;
  }

  return resultado;
}

void updateOutputLeds(uint32_t resultado) {
  // Hardware físico de 4 LEDs: mostra apenas os 4 bits menos significativos.
  resultado = resultado & 0x0F;

  digitalWrite(LED_BIT0, resultado & 0x01);
  digitalWrite(LED_BIT1, resultado & 0x02);
  digitalWrite(LED_BIT2, resultado & 0x04);
  digitalWrite(LED_BIT3, resultado & 0x08);
}

void setStatusLed(bool overflow, int32_t resultadoAssinado) {
  if (overflow) {
    // Overflow tem prioridade sobre positivo, negativo ou zero
    onboardLed.setPixelColor(0, onboardLed.Color(80, 80, 0)); // amarelo
  } else if (resultadoAssinado == 0) {
    onboardLed.setPixelColor(0, onboardLed.Color(0, 0, 80)); // azul
  } else if (resultadoAssinado > 0) {
    onboardLed.setPixelColor(0, onboardLed.Color(0, 80, 0)); // verde
  } else {
    onboardLed.setPixelColor(0, onboardLed.Color(80, 0, 0)); // vermelho
  }

  onboardLed.show();
}

bool detectOverflowAdd(int valA, int valB, int resultadoAssinado) {
  bool aPositive = valA >= 0;
  bool bPositive = valB >= 0;
  bool resultPositive = resultadoAssinado >= 0;

  return (aPositive == bPositive) && (resultPositive != aPositive);
}

bool detectOverflowSub(int valA, int valB, int resultadoAssinado) {
  bool aPositive = valA >= 0;
  bool bPositive = valB >= 0;
  bool resultPositive = resultadoAssinado >= 0;

  return (aPositive != bPositive) && (resultPositive != aPositive);
}


// ======================
// Testes de regressao
// ======================

bool runRegressionCase(const char* nome,
                       const char* op,
                       int aRaw,
                       int bRaw,
                       int resultadoCompletoEsperado,
                       int resultado4bitsEsperado,
                       bool overflowEsperado) {
  int valA = toSigned4(aRaw);
  int valB = toSigned4(bRaw);

  bool isAdd = strcmp(op, "add") == 0;

  int resultadoCompleto;

  if (isAdd) {
    resultadoCompleto = valA + valB;
  } else {
    resultadoCompleto = valA - valB;
  }

  int resultadoMascarado = resultadoCompleto & 0x0F;
  int resultado4bits = toSigned4(resultadoMascarado);

  bool overflow;

  if (isAdd) {
    overflow = detectOverflowAdd(valA, valB, resultado4bits);
  } else {
    overflow = detectOverflowSub(valA, valB, resultado4bits);
  }

  bool passou = (resultadoCompleto == resultadoCompletoEsperado) &&
                (resultado4bits == resultado4bitsEsperado) &&
                (overflow == overflowEsperado);

  Serial.print("> Regressao: ");
  Serial.print(nome);
  Serial.print("... ");
  Serial.println(passou ? "PASS" : "FAIL");

  if (!passou) {
    Serial.print("  A = ");
    Serial.print(toBinary4(aRaw));
    Serial.print(" (");
    Serial.print(valA);
    Serial.print(") | B = ");
    Serial.print(toBinary4(bRaw));
    Serial.print(" (");
    Serial.print(valB);
    Serial.println(")");

    Serial.print("  Resultado completo obtido: ");
    Serial.print(resultadoCompleto);
    Serial.print(" | esperado: ");
    Serial.println(resultadoCompletoEsperado);

    Serial.print("  Resultado 4 bits obtido: ");
    Serial.print(toBinary4(resultadoMascarado));
    Serial.print(" (");
    Serial.print(resultado4bits);
    Serial.print(") | esperado: ");
    Serial.println(resultado4bitsEsperado);

    Serial.print("  Overflow obtido: ");
    Serial.print(overflow ? "true" : "false");
    Serial.print(" | esperado: ");
    Serial.println(overflowEsperado ? "true" : "false");
  }

  return passou;
}

void runRegressionTests() {
  Serial.println();
  Serial.println("============================================");
  Serial.println("Testes de regressao: adicao e subtracao C2");
  Serial.println("============================================");

  int total = 0;
  int aprovados = 0;

  total++;
  if (runRegressionCase("Soma positiva sem overflow (2+4=6)", "add", 0b0010, 0b0100, 6, 6, false)) {
    aprovados++;
  }

  total++;
  if (runRegressionCase("Soma negativa sem overflow (-3+1=-2)", "add", 0b1101, 0b0001, -2, -2, false)) {
    aprovados++;
  }

  total++;
  if (runRegressionCase("Soma com resultado zero (-3+3=0)", "add", 0b1101, 0b0011, 0, 0, false)) {
    aprovados++;
  }

  total++;
  if (runRegressionCase("Soma com overflow positivo (7+1=8)", "add", 0b0111, 0b0001, 8, -8, true)) {
    aprovados++;
  }

  total++;
  if (runRegressionCase("Soma com overflow negativo (-8+(-1)=-9)", "add", 0b1000, 0b1111, -9, 7, true)) {
    aprovados++;
  }

  total++;
  if (runRegressionCase("Subtracao positiva sem overflow (5-2=3)", "sub", 0b0101, 0b0010, 3, 3, false)) {
    aprovados++;
  }

  total++;
  if (runRegressionCase("Subtracao negativa sem overflow (2-5=-3)", "sub", 0b0010, 0b0101, -3, -3, false)) {
    aprovados++;
  }

  total++;
  if (runRegressionCase("Subtracao com resultado zero (3-3=0)", "sub", 0b0011, 0b0011, 0, 0, false)) {
    aprovados++;
  }

  total++;
  if (runRegressionCase("Subtracao com overflow positivo (7-(-1)=8)", "sub", 0b0111, 0b1111, 8, -8, true)) {
    aprovados++;
  }

  total++;
  if (runRegressionCase("Subtracao com overflow negativo (-8-1=-9)", "sub", 0b1000, 0b0001, -9, 7, true)) {
    aprovados++;
  }

  Serial.print("> Resultado geral: ");
  Serial.print(aprovados);
  Serial.print("/");
  Serial.print(total);
  Serial.print(" testes... ");
  Serial.println((aprovados == total) ? "PASS" : "FAIL");
  Serial.println("============================================");
  Serial.println();
}

// ======================
// Página HTML
// ======================

String makePage(String a = "0011", String b = "0010", String op = "add", String mode = "4",
                String resultBin = "----", String resultDec = "--",
                String status = "Aguardando cálculo", bool overflow = false) {

  if (mode != "16") {
    mode = "4";
  }

  int bits = (mode == "16") ? EXPANDED_BITS : BASE_BITS;

  String checked4 = (mode == "4") ? "checked" : "";
  String checked16 = (mode == "16") ? "checked" : "";

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
  html += ".box{max-width:620px;margin:auto;background:white;padding:20px;border-radius:12px;box-shadow:0 0 12px #ccc;}";
  html += "h1{text-align:center;font-size:26px;}";
  html += "label{display:block;margin-top:14px;font-weight:bold;}";
  html += "input[type=text]{width:100%;font-size:20px;padding:10px;margin-top:6px;box-sizing:border-box;text-align:center;letter-spacing:3px;}";
  html += ".ops,.modes{margin-top:14px;font-size:18px;}";
  html += ".ops label,.modes label{font-weight:normal;margin-top:8px;}";
  html += "button{width:100%;font-size:20px;margin-top:20px;padding:12px;border:0;border-radius:8px;background:#1e88e5;color:white;}";
  html += ".result{margin-top:22px;padding:14px;border-radius:8px;background:#eef3f8;font-size:18px;word-break:break-word;}";
  html += ".ok{color:#0a8f08;font-weight:bold;}";
  html += ".overflow{color:#d00000;font-weight:bold;}";
  html += ".small{font-size:14px;color:#666;margin-top:20px;line-height:1.4;}";
  html += ".card{border:1px solid #d7e2ee;border-radius:10px;padding:12px;margin-top:12px;background:#fafcff;}";
  html += "</style>";

  html += "</head>";
  html += "<body>";
  html += "<div class='box'>";

  html += "<h1>Calculadora ESP32-C3</h1>";

  html += "<form action='/calc' method='GET'>";

  html += "<div class='card'>";
  html += "<strong>Modo de operandos</strong>";
  html += "<div class='modes'>";
  html += "<label><input type='radio' name='mode' value='4' " + checked4 + " onchange='toggleMode()'> Original - 4 bits</label>";
  html += "<label><input type='radio' name='mode' value='16' " + checked16 + " onchange='toggleMode()'> Expandido - 16 bits</label>";
  html += "</div>";
  html += "</div>";

  html += "<label id='labelA'>Operando A - " + String(bits) + " bits</label>";
  html += "<input id='inputA' type='text' name='a' maxlength='" + String(bits) + "' value='" + a + "' pattern='[01]{" + String(bits) + "}' required>";

  html += "<div id='campoB'>";
  html += "<label id='labelB'>Operando B - " + String(bits) + " bits</label>";
  html += "<input id='inputB' type='text' name='b' maxlength='" + String(bits) + "' value='" + b + "' pattern='[01]{" + String(bits) + "}' required>";
  html += "</div>";

  html += "<div class='ops'>";
  html += "<strong>Operação</strong>";
  html += "<label><input type='radio' name='op' value='add' " + checkedAdd + " onchange='toggleMode()'> Soma</label>";
  html += "<label><input type='radio' name='op' value='sub' " + checkedSub + " onchange='toggleMode()'> Subtração</label>";
  html += "<label><input type='radio' name='op' value='mul' " + checkedMul + " onchange='toggleMode()'> Multiplicação</label>";
  html += "<label><input type='radio' name='op' value='fat' " + checkedFat + " onchange='toggleMode()'> Fatorial de A</label>";
  html += "</div>";

  html += "<button type='submit'>Calcular</button>";
  html += "</form>";

  html += "<div class='result'>";
  html += "<p><strong>Resultado binário:</strong> " + resultBin + "</p>";
  html += "<p><strong>Resultado decimal:</strong> " + resultDec + "</p>";
  html += "<p><strong>Status:</strong> <span class='" + statusClass + "'>" + status + "</span></p>";
  html += "</div>";

  html += "<div class='small' id='infoModo'>";
  if (mode == "16") {
    html += "<p>Modo expandido: 16 bits em complemento de dois. Faixa assinada: -32768 até +32767. Valor bruto máximo: 65535.</p>";
  } else {
    html += "<p>Modo original: 4 bits em complemento de dois. Faixa assinada: -8 até +7. Valor bruto máximo: 15.</p>";
  }
  html += "<p>No modo fatorial, apenas o operando A é usado.</p>";
  html += "<p>GPIO4 = bit3, GPIO5 = bit2, GPIO6 = bit1, GPIO7 = bit0. No modo expandido, os LEDs mostram os 4 bits menos significativos.</p>";
  html += "</div>";

  html += "</div>";

  html += "<script>";
  html += "function getBits(){return document.querySelector(\"input[name='mode']:checked\").value === '16' ? 16 : 4;}";
  html += "function onlyBinary(value){return value.replace(/[^01]/g,'');}";
  html += "function normalizeInput(input,bits){";
  html += "  input.value = onlyBinary(input.value);";
  html += "  if(input.value.length > bits){input.value = input.value.slice(input.value.length - bits);}";
  html += "  if(input.value.length < bits){input.value = input.value.padStart(bits,'0');}";
  html += "}";
  html += "function toggleMode(){";
  html += "  var bits = getBits();";
  html += "  var fat = document.querySelector(\"input[name='op'][value='fat']\").checked;";
  html += "  var inputA = document.getElementById('inputA');";
  html += "  var inputB = document.getElementById('inputB');";
  html += "  var campoB = document.getElementById('campoB');";
  html += "  document.getElementById('labelA').innerHTML = 'Operando A - ' + bits + ' bits';";
  html += "  document.getElementById('labelB').innerHTML = 'Operando B - ' + bits + ' bits';";
  html += "  inputA.maxLength = bits; inputB.maxLength = bits;";
  html += "  inputA.pattern = '[01]{' + bits + '}'; inputB.pattern = '[01]{' + bits + '}';";
  html += "  normalizeInput(inputA,bits); normalizeInput(inputB,bits);";
  html += "  if(fat){campoB.style.display='none'; inputB.required=false;}";
  html += "  else{campoB.style.display='block'; inputB.required=true;}";
  html += "  var info = document.getElementById('infoModo');";
  html += "  if(bits === 16){info.innerHTML = '<p>Modo expandido: 16 bits em complemento de dois. Faixa assinada: -32768 até +32767. Valor bruto máximo: 65535.</p><p>No modo fatorial, apenas o operando A é usado.</p><p>GPIO4 = bit3, GPIO5 = bit2, GPIO6 = bit1, GPIO7 = bit0. No modo expandido, os LEDs mostram os 4 bits menos significativos.</p>';}";
  html += "  else{info.innerHTML = '<p>Modo original: 4 bits em complemento de dois. Faixa assinada: -8 até +7. Valor bruto máximo: 15.</p><p>No modo fatorial, apenas o operando A é usado.</p><p>GPIO4 = bit3, GPIO5 = bit2, GPIO6 = bit1, GPIO7 = bit0. No modo expandido, os LEDs mostram os 4 bits menos significativos.</p>';}";
  html += "}";
  html += "window.onload = toggleMode;";
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
  String mode = server.arg("mode");

  if (mode != "16") {
    mode = "4";
  }

  int bits = (mode == "16") ? EXPANDED_BITS : BASE_BITS;

  if (op != "add" && op != "sub" && op != "mul" && op != "fat") {
    op = "add";
  }

  if (op == "fat") {
    if (!isBinaryN(paramA, bits)) {
      String erro = "Erro: use exatamente " + String(bits) + " bits em A";
      server.send(400, "text/html", makePage(paramA, paramB, op, mode, "----", "--", erro, true));
      return;
    }

    // No fatorial, o operando B não é usado.
    paramB = zeroBits(bits);
  } else {
    if (!isBinaryN(paramA, bits) || !isBinaryN(paramB, bits)) {
      String erro = "Erro: use exatamente " + String(bits) + " bits em A e B";
      server.send(400, "text/html", makePage(paramA, paramB, op, mode, "----", "--", erro, true));
      return;
    }
  }

  // 1. Parsing: string binária para inteiro bruto
  uint32_t valA_raw = strtoul(paramA.c_str(), NULL, 2);
  uint32_t valB_raw = strtoul(paramB.c_str(), NULL, 2);

  // 2. Interpretação em complemento de dois conforme o modo escolhido
  int32_t valA = toSignedBits(valA_raw, bits);
  int32_t valB = toSignedBits(valB_raw, bits);

  int32_t limiteMin = minSignedValue(bits);
  int32_t limiteMax = maxSignedValue(bits);

  // 3. Operação aritmética em C++ nativo
  long long resultadoCompleto = 0;
  bool erroFatorialNegativo = false;
  bool overflowFatorialAntecipado = false;

  if (op == "add") {
    resultadoCompleto = (long long)valA + (long long)valB;
  } else if (op == "sub") {
    resultadoCompleto = (long long)valA - (long long)valB;
  } else if (op == "mul") {
    resultadoCompleto = (long long)valA * (long long)valB;
  } else if (op == "fat") {
    if (valA < 0) {
      erroFatorialNegativo = true;
    } else {
      resultadoCompleto = fatorialLimitado((int)valA, limiteMax, &overflowFatorialAntecipado);
    }
  }

  if (erroFatorialNegativo) {
    server.send(400, "text/html", makePage(paramA, paramB, op, mode, "----", "--", "Erro: fatorial não existe para número negativo", true));
    return;
  }

  // 4. Detecção de overflow para qualquer operação e qualquer modo
  bool overflow = overflowFatorialAntecipado ||
                  (resultadoCompleto < (long long)limiteMin || resultadoCompleto > (long long)limiteMax);

  // 5. Mascaramento conforme a quantidade de bits do modo escolhido
  uint32_t resultadoMascarado = ((uint32_t)resultadoCompleto) & maskForBits(bits);

  // 6. Resultado assinado após o mascaramento em complemento de dois
  int32_t resultadoAssinado = toSignedBits(resultadoMascarado, bits);

  // 7. Output para GPIO
  updateOutputLeds(resultadoMascarado);

  // LED onboard indica status
  setStatusLed(overflow, resultadoAssinado);

  String resultBin = toBinaryBits(resultadoMascarado, bits);
  String resultDec = String((long)resultadoAssinado);

  String status;

  if (overflow) {
    status = "OVERFLOW";
  } else {
    status = "OK";
  }

  server.send(200, "text/html", makePage(paramA, paramB, op, mode, resultBin, resultDec, status, overflow));
}

// ======================
// Setup e loop
// ======================

void setup() {
  Serial.begin(115200);

  runRegressionTests();

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
