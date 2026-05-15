void setup() {
pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
neopixelWrite(LED_BUILTIN, 255, 255, 0);
delay(1000);
neopixelWrite(LED_BUILTIN, 0, 0, 0);
delay(1000);
}