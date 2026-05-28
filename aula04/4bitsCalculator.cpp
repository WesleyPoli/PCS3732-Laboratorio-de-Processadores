// Após conectar na rede Wi-Fi, acessar o front pelo IP 192.168.4.1
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

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
// Dashboard de desempenho
// ======================

#define PERF_SAMPLES 5
#define PERF_REPETITIONS 5000

volatile uint32_t perfSink = 0;

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
// Dashboard de desempenho
// ======================

long long executarOperacaoBenchmark(const char* op, int bits, uint32_t aRaw, uint32_t bRaw) {
  int32_t valA = toSignedBits(aRaw, bits);
  int32_t valB = toSignedBits(bRaw, bits);
  int32_t limiteMax = maxSignedValue(bits);

  if (strcmp(op, "add") == 0) {
    return (long long)valA + (long long)valB;
  }

  if (strcmp(op, "sub") == 0) {
    return (long long)valA - (long long)valB;
  }

  if (strcmp(op, "mul") == 0) {
    return (long long)valA * (long long)valB;
  }

  if (strcmp(op, "div") == 0) {
    if (valB == 0) {
      return 0;
    }

    return (long long)valA / (long long)valB;
  }

  if (strcmp(op, "fat") == 0) {
    if (valA < 0) {
      return 0;
    }

    bool overflowFatorial = false;
    return fatorialLimitado((int)valA, limiteMax, &overflowFatorial);
  }

  return 0;
}

void getBenchmarkOperands(int bits, const char* op, uint32_t* aRaw, uint32_t* bRaw) {
  if (bits == EXPANDED_BITS) {
    // Valores representativos para o modo expandido de 16 bits.
    if (strcmp(op, "add") == 0) {
      *aRaw = 0b0000101110111000; // 3000
      *bRaw = 0b0000010010110000; // 1200
    } else if (strcmp(op, "sub") == 0) {
      *aRaw = 0b0001001110001000; // 5000
      *bRaw = 0b0000010011010010; // 1234
    } else if (strcmp(op, "mul") == 0) {
      *aRaw = 0b0000000001111000; // 120
      *bRaw = 0b0000000000001100; // 12
    } else if (strcmp(op, "div") == 0) {
      *aRaw = 0b0000000001100100; // 100
      *bRaw = 0b0000000000000101; // 5
    } else {
      *aRaw = 0b0000000000001000; // 8, usado em 8!
      *bRaw = 0;
    }

    return;
  }

  // Valores representativos para o modo original de 4 bits com sinal.
  if (strcmp(op, "add") == 0) {
    *aRaw = 0b0011; // 3
    *bRaw = 0b0010; // 2
  } else if (strcmp(op, "sub") == 0) {
    *aRaw = 0b0101; // 5
    *bRaw = 0b0011; // 3
  } else if (strcmp(op, "mul") == 0) {
    *aRaw = 0b0011; // 3
    *bRaw = 0b0010; // 2
  } else if (strcmp(op, "div") == 0) {
    *aRaw = 0b0110; // 6
    *bRaw = 0b0010; // 2
  } else {
    *aRaw = 0b0101; // 5, usado em 5!
    *bRaw = 0;
  }
}

unsigned long medirTempoLoteUs(const char* op, int bits, uint32_t aRaw, uint32_t bRaw) {
  unsigned long inicio = micros();

  for (int i = 0; i < PERF_REPETITIONS; i++) {
    long long resultado = executarOperacaoBenchmark(op, bits, aRaw, bRaw);
    perfSink ^= (uint32_t)resultado;
  }

  unsigned long fim = micros();
  return fim - inicio;
}

String formatFloat(float value, int casas) {
  return String(value, casas);
}

String buildDashboardHtml() {
  const int totalRows = 10;
  const char* opCodes[5] = {"add", "sub", "mul", "div", "fat"};
  const char* opLabels[5] = {"Soma", "Subtracao", "Multiplicacao", "Divisao", "Fatorial"};
  const int modeBits[2] = {BASE_BITS, EXPANDED_BITS};
  const char* modeLabels[2] = {"4 bits C2", "16 bits C2"};

  unsigned long tempos[totalRows][PERF_SAMPLES];
  float medias[totalRows];
  float desvios[totalRows];
  float maiorMedia = 0.0;

  int row = 0;

  for (int m = 0; m < 2; m++) {
    for (int o = 0; o < 5; o++) {
      uint32_t aRaw = 0;
      uint32_t bRaw = 0;
      getBenchmarkOperands(modeBits[m], opCodes[o], &aRaw, &bRaw);

      float soma = 0.0;

      for (int s = 0; s < PERF_SAMPLES; s++) {
        tempos[row][s] = medirTempoLoteUs(opCodes[o], modeBits[m], aRaw, bRaw);
        soma += (float)tempos[row][s];
      }

      medias[row] = soma / PERF_SAMPLES;

      float somaQuadrados = 0.0;

      for (int s = 0; s < PERF_SAMPLES; s++) {
        float diferenca = (float)tempos[row][s] - medias[row];
        somaQuadrados += diferenca * diferenca;
      }

      desvios[row] = sqrt(somaQuadrados / PERF_SAMPLES);

      if (medias[row] > maiorMedia) {
        maiorMedia = medias[row];
      }

      row++;
    }
  }

  String html = "";

  html += "<div class='dashboard'>";
  html += "<h2>Dashboard de desempenho</h2>";
  html += "<p class='dash-note'>Cada amostra executa " + String(PERF_REPETITIONS) + " repeticoes da mesma operacao e mede o tempo total do lote em microssegundos. Isso evita que uma unica operacao apareca como 0 us por ser rapida demais.</p>";
  html += "<div class='table-wrap'>";
  html += "<table>";
  html += "<tr>";
  html += "<th>Modo</th>";
  html += "<th>Operacao</th>";

  for (int s = 0; s < PERF_SAMPLES; s++) {
    html += "<th>t" + String(s + 1) + " (us)</th>";
  }

  html += "<th>Media (us)</th>";
  html += "<th>Desvio padrao</th>";
  html += "<th>Comparacao</th>";
  html += "</tr>";

  row = 0;

  for (int m = 0; m < 2; m++) {
    for (int o = 0; o < 5; o++) {
      int barWidth = 0;

      if (maiorMedia > 0.0) {
        barWidth = (int)((medias[row] / maiorMedia) * 100.0);
      }

      if (barWidth < 3) {
        barWidth = 3;
      }

      html += "<tr>";
      html += "<td>" + String(modeLabels[m]) + "</td>";
      html += "<td>" + String(opLabels[o]) + "</td>";

      for (int s = 0; s < PERF_SAMPLES; s++) {
        html += "<td>" + String(tempos[row][s]) + "</td>";
      }

      html += "<td><strong>" + formatFloat(medias[row], 1) + "</strong></td>";
      html += "<td>" + formatFloat(desvios[row], 1) + "</td>";
      html += "<td><div class='bar-bg'><div class='bar-fill' style='width:" + String(barWidth) + "%'></div></div></td>";
      html += "</tr>";

      row++;
    }
  }

  html += "</table>";
  html += "</div>";
  html += "<p class='dash-note'>A tabela compara os modos de 4 bits e 16 bits e tambem compara soma, subtracao, multiplicacao, divisao e fatorial. Tempos maiores geram barras mais longas.</p>";
  html += "</div>";

  return html;
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
  String checkedDiv = "";
  String checkedFat = "";

  if (op == "sub") {
    checkedSub = "checked";
  } else if (op == "mul") {
    checkedMul = "checked";
  } else if (op == "div") {
    checkedDiv = "checked";
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
  html += ".dashboard{margin-top:22px;padding:14px;border:1px solid #d7e2ee;border-radius:10px;background:#fafcff;}";
  html += ".dashboard h2{text-align:center;margin:0 0 10px 0;font-size:22px;}";
  html += ".dash-note{font-size:13px;color:#555;line-height:1.35;}";
  html += ".table-wrap{overflow-x:auto;margin-top:10px;}";
  html += "table{width:100%;border-collapse:collapse;font-size:13px;background:white;}";
  html += "th,td{border:1px solid #c8d3df;padding:8px;text-align:center;white-space:nowrap;}";
  html += "th{background:#eaf2fb;}";
  html += ".bar-bg{width:100px;height:12px;border-radius:6px;background:#e1e8f0;margin:auto;overflow:hidden;}";
  html += ".bar-fill{height:12px;border-radius:6px;background:#1e88e5;}";
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
  html += "<label><input type='radio' name='op' value='div' " + checkedDiv + " onchange='toggleMode()'> Divisão</label>";
  html += "<label><input type='radio' name='op' value='fat' " + checkedFat + " onchange='toggleMode()'> Fatorial de A</label>";
  html += "</div>";

  html += "<button type='submit'>Calcular</button>";
  html += "</form>";

  html += "<div class='result'>";
  html += "<p><strong>Resultado binário:</strong> " + resultBin + "</p>";
  html += "<p><strong>Resultado decimal:</strong> " + resultDec + "</p>";
  html += "<p><strong>Status:</strong> <span class='" + statusClass + "'>" + status + "</span></p>";
  html += "</div>";

  html += buildDashboardHtml();

  html += "<div class='small' id='infoModo'>";
  if (mode == "16") {
    html += "<p>Modo expandido: 16 bits em complemento de dois. Faixa com sinal: -32768 até +32767. Valor bruto máximo: 65535.</p>";
  } else {
    html += "<p>Modo original: 4 bits em complemento de dois. Valor bruto máximo: 15.</p>";
  }
  html += "<p>No modo fatorial, apenas o operando A é usado. Na divisão, o operando B não pode ser zero.</p>";
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
  html += "  if(bits === 16){info.innerHTML = '<p>Modo expandido: 16 bits em complemento de dois. Faixa com sinal: -32768 até +32767. Valor bruto máximo: 65535.</p><p>No modo fatorial, apenas o operando A é usado. Na divisão, o operando B não pode ser zero.</p><p>GPIO4 = bit3, GPIO5 = bit2, GPIO6 = bit1, GPIO7 = bit0. No modo expandido, os LEDs mostram os 4 bits menos significativos.</p>';}";
  html += "  else{info.innerHTML = '<p>Modo original: 4 bits em complemento de dois.Valor bruto máximo: 15.</p><p>No modo fatorial, apenas o operando A é usado. Na divisão, o operando B não pode ser zero.</p><p>GPIO4 = bit3, GPIO5 = bit2, GPIO6 = bit1, GPIO7 = bit0. No modo expandido, os LEDs mostram os 4 bits menos significativos.</p>';}";
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

  if (op != "add" && op != "sub" && op != "mul" && op != "div" && op != "fat") {
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

  if (op == "div" && valB == 0) {
    updateOutputLeds(0);
    server.send(400, "text/html", makePage(paramA, paramB, op, mode, "----", "--", "Erro: divisão por zero", true));
    return;
  }

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
  } else if (op == "div") {
    resultadoCompleto = (long long)valA / (long long)valB;
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
