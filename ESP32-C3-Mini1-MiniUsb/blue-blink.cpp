#include <Adafruit_NeoPixel.h>

#define LED_PIN 8
#define LED_COUNT 1

Adafruit_NeoPixel led(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  led.begin();
  led.setBrightness(30);
  led.show();
}

void loop() {
  led.setPixelColor(0, led.Color(0, 0, 255)); // vermelho
  led.show();
  delay(500);

  led.setPixelColor(0, led.Color(0, 0, 0)); // apagado
  led.show();
  delay(500);
}