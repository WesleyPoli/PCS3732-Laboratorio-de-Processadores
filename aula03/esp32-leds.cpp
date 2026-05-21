// Código para testar o controle do ESP32 sobre LEDs externos montados na protoboard
#include <Adafruit_NeoPixel.h>

// NeoPixel onboard da sua placa
#define NEOPIXEL_PIN 8
#define NEOPIXEL_COUNT 1

Adafruit_NeoPixel onboardLed(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// LEDs externos na protoboard
#define LED1 4
#define LED2 5
#define LED3 6
#define LED4 7

void setup() {
  // Inicializa e apaga o NeoPixel onboard
  onboardLed.begin();
  onboardLed.setBrightness(30);
  onboardLed.setPixelColor(0, onboardLed.Color(0, 0, 0));
  onboardLed.show();

  // Configura os LEDs externos como saída
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  pinMode(LED4, OUTPUT);

  // Começa com todos apagados
  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  digitalWrite(LED3, LOW);
  digitalWrite(LED4, LOW);
}

void loop() {
  // Liga LED1
  digitalWrite(LED1, HIGH);
  digitalWrite(LED2, LOW);
  digitalWrite(LED3, LOW);
  digitalWrite(LED4, LOW);
  delay(500);

  // Liga LED2
  digitalWrite(LED1, LOW);
  digitalWrite(LED2, HIGH);
  digitalWrite(LED3, LOW);
  digitalWrite(LED4, LOW);
  delay(500);

  // Liga LED3
  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  digitalWrite(LED3, HIGH);
  digitalWrite(LED4, LOW);
  delay(500);

  // Liga LED4
  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  digitalWrite(LED3, LOW);
  digitalWrite(LED4, HIGH);
  delay(500);
}