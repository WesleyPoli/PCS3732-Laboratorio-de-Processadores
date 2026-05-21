// Calculadora de 4 bits em complemento de 1 com entrada serial
#include <Adafruit_NeoPixel.h>

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

int toSignedOnesComplement4(int value) {
  value = value & 0x0F;

  // Números positivos: 0000 até 0111
  if ((value & 0x08) == 0) {
    return value;
  }

  // Números negativos em complemento de um:
  // 1110 = -1
  // 1101 = -2
  // 1100 = -3
  // ...
  // 1000 = -7
  //
  // 1111 representa -0
  return -((~value) & 0x0F);
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

String toDecimalTextOnesComplement4(int value) {
  value = value & 0x0F;

  if (value == 0x0F) {
    return "-0";
  }

  return String(toSignedOnesComplement4(value));
}

bool isOperation(String op) {
  op.toLowerCase();

  return op == "add" ||
         op == "soma" ||
         op == "+" ||
         op == "sub" ||
         op == "subtracao" ||
         op == "subtração" ||
         op == "-";
}

String normalizeOperation(String op) {
  op.toLowerCase();

  if (op == "sub" || op == "subtracao" || op == "subtração" || op == "-") {
    return "sub";
  }

  return "add";
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

void setStatusLed(bool overflow) {
  if (overflow) {
    onboardLed.setPixelColor(0, onboardLed.Color(255, 0, 0)); // vermelho
  } else {
    onboardLed.setPixelColor(0, onboardLed.Color(0, 80, 0)); // verde
  }

  onboardLed.show();
}

void printHelp() {
  Serial.println();
  Serial.println("=== Calculadora ESP32-C3 - Complemento de Um ===");
  Serial.println("Digite no formato:");
  Serial.println("  0011 0010 add");
  Serial.println("  0011 0010 sub");
  Serial.println();
  Serial.println("Tambem aceita:");
  Serial.println("  add 0011 0010");
  Serial.println("  sub 0011 0010");
  Serial.println();
  Serial.println("Faixa em complemento de um com 4 bits:");
  Serial.println("  0111 = +7");
  Serial.println("  0000 = +0");
  Serial.println("  1111 = -0");
  Serial.println("  1000 = -7");
  Serial.println();
  Serial.println("LEDs:");
  Serial.println("  GPIO4 = bit3");
  Serial.println("  GPIO5 = bit2");
  Serial.println("  GPIO6 = bit1");
  Serial.println("  GPIO7 = bit0");
  Serial.println();
}

bool parseInput(String entrada, String &paramA, String &paramB, String &op) {
  entrada.trim();
  entrada.replace(',', ' ');
  entrada.replace(';', ' ');

  String tokens[3];
  int tokenCount = 0;

  while (entrada.length() > 0 && tokenCount < 3) {
    entrada.trim();

    int spaceIndex = entrada.indexOf(' ');

    if (spaceIndex == -1) {
      tokens[tokenCount] = entrada;
      tokenCount++;
      entrada = "";
    } else {
      tokens[tokenCount] = entrada.substring(0, spaceIndex);
      tokenCount++;
      entrada = entrada.substring(spaceIndex + 1);
    }
  }

  if (tokenCount != 3) {
    return false;
  }

  tokens[0].trim();
  tokens[1].trim();
  tokens[2].trim();

  if (isOperation(tokens[0])) {
    op = normalizeOperation(tokens[0]);
    paramA = tokens[1];
    paramB = tokens[2];
  } else {
    paramA = tokens[0];
    paramB = tokens[1];
    op = normalizeOperation(tokens[2]);
  }

  return true;
}

int onesComplementAdd4(int valA_raw, int valB_raw, bool &endAroundCarry) {
  // 2. Operacao aritmetica em C nativo
  int soma = (valA_raw & 0x0F) + (valB_raw & 0x0F);

  // Carry out do bit mais significativo
  endAroundCarry = soma > 0x0F;

  // 3. Mascaramento, garantindo 4 bits
  int resultado = soma & 0x0F;

  // End-around carry:
  // se houve carry out, soma 1 de volta no bit menos significativo
  if (endAroundCarry) {
    resultado = resultado + 1;
  }

  // Garante novamente 4 bits
  resultado = resultado & 0x0F;

  return resultado;
}

void processCommand(String entrada) {
  String paramA;
  String paramB;
  String op;

  if (!parseInput(entrada, paramA, paramB, op)) {
    Serial.println("Erro: formato invalido.");
    Serial.println("Use, por exemplo: 0011 0010 add");
    return;
  }

  if (!isBinary4(paramA) || !isBinary4(paramB)) {
    Serial.println("Erro: A e B devem ter exatamente 4 bits.");
    Serial.println("Exemplo valido: 0011 0010 add");
    return;
  }

  // 1. Parsing: String para Inteiro
  int valA_raw = strtol(paramA.c_str(), NULL, 2);
  int valB_raw = strtol(paramB.c_str(), NULL, 2);

  int valA = toSignedOnesComplement4(valA_raw);
  int valB = toSignedOnesComplement4(valB_raw);

  int operandoB_raw;

  if (op == "add") {
    operandoB_raw = valB_raw;
  } else {
    // Em complemento de um:
    // A - B = A + complemento_de_um(B)
    operandoB_raw = (~valB_raw) & 0x0F;
  }

  bool endAroundCarry = false;

  // 2. Operacao aritmetica em C nativo
  int resultado = onesComplementAdd4(valA_raw, operandoB_raw, endAroundCarry);

  // Resultado assinado em complemento de um
  int resultadoAssinado = toSignedOnesComplement4(resultado);

  // Resultado matematico ideal, usado para detectar overflow
  int resultadoMatematico;

  if (op == "add") {
    resultadoMatematico = valA + valB;
  } else {
    resultadoMatematico = valA - valB;
  }

  bool overflow = resultadoMatematico > 7 || resultadoMatematico < -7;

  // 4. Output para GPIO
  updateOutputLeds(resultado);

  // NeoPixel onboard indica status
  setStatusLed(overflow);

  Serial.println();
  Serial.println("=== Resultado ===");

  Serial.print("A: ");
  Serial.print(paramA);
  Serial.print(" = ");
  Serial.println(toDecimalTextOnesComplement4(valA_raw));

  Serial.print("B: ");
  Serial.print(paramB);
  Serial.print(" = ");
  Serial.println(toDecimalTextOnesComplement4(valB_raw));

  Serial.print("Operacao: ");
  if (op == "add") {
    Serial.println("SOMA");
  } else {
    Serial.println("SUBTRACAO");
  }

  if (op == "sub") {
    Serial.print("Complemento de um de B usado na soma: ");
    Serial.println(toBinary4(operandoB_raw));
  }

  Serial.print("End-around carry: ");
  if (endAroundCarry) {
    Serial.println("SIM");
  } else {
    Serial.println("NAO");
  }

  Serial.print("Resultado binario: ");
  Serial.println(toBinary4(resultado));

  Serial.print("Resultado decimal interpretado: ");
  Serial.println(toDecimalTextOnesComplement4(resultado));

  Serial.print("Status: ");
  if (overflow) {
    Serial.println("OVERFLOW");
  } else {
    Serial.println("OK");
  }

  Serial.println();
  Serial.println("Digite outro comando:");
}

// ======================
// Setup e loop
// ======================

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(200);

  pinMode(LED_BIT0, OUTPUT);
  pinMode(LED_BIT1, OUTPUT);
  pinMode(LED_BIT2, OUTPUT);
  pinMode(LED_BIT3, OUTPUT);

  updateOutputLeds(0);

  onboardLed.begin();
  onboardLed.setBrightness(30);
  onboardLed.setPixelColor(0, onboardLed.Color(0, 0, 30)); // azul: iniciado
  onboardLed.show();

  delay(1000);

  printHelp();

  Serial.println("Digite um comando:");
}

void loop() {
  if (Serial.available() > 0) {
    // Entrada via teclado no computador conectado ao ESP32 por USB
    String entrada = Serial.readString();

    entrada.trim();

    if (entrada.length() > 0) {
      processCommand(entrada);
    }
  }
}