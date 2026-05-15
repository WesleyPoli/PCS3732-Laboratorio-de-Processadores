void setup() {
pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
neopixelWrite(LED_BUILTIN, 0, 50, 0); //verde
delay(3000);
neopixelWrite(LED_BUILTIN, 50, 0, 0); //vermelho
delay(4000);
neopixelWrite(LED_BUILTIN, 50, 50, 0); //amarelo
delay(1000);

}