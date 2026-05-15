 .section .text
    .align 2

    # ESP32-C3: arquitetura RISC-V
    .equ LED_PIN, 8       
    .equ OUTPUT, 1
    .equ LOW, 0
    .equ HIGH, 1

    .global _Z5setupv
    .global _Z4loopv

    .extern _Z7pinModehh
    .extern _Z12digitalWritehh
    .extern _Z5delaym


# equivalente a:
# void setup() {
#   pinMode(LED_BUILTIN, OUTPUT);
# }

_Z5setupv:
    addi sp, sp, -16
    sw ra, 12(sp)

    li a0, LED_PIN        # primeiro argumento: pino
    li a1, OUTPUT         # segundo argumento: modo OUTPUT
    call _Z7pinModehh     # pinMode(uint8_t, uint8_t)

    lw ra, 12(sp)
    addi sp, sp, 16
    ret


# equivalente a:
# void loop() {
#   digitalWrite(LED_BUILTIN, HIGH);
#   delay(1000);
#   digitalWrite(LED_BUILTIN, LOW);
#   delay(1000);
# }

_Z4loopv:
    addi sp, sp, -16
    sw ra, 12(sp)

    li a0, LED_PIN
    li a1, HIGH
    call _Z12digitalWritehh

    li a0, 1000
    call _Z5delaym

    li a0, LED_PIN
    li a1, LOW
    call _Z12digitalWritehh

    li a0, 1000
    call _Z5delaym

    lw ra, 12(sp)
    addi sp, sp, 16
    ret
