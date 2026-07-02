// =====================================================================
//  Calculadora.cpp - Calculadora Binaria Standalone
//  Teclado Matricial 4x4  +  LCD 16x2 HD44780 via I2C (PCF8574)
//  Plataforma: Raspberry Pi 3B+  |  Kit Freenove FNK0054
//  PCS3732 - Laboratorio de Processadores
//
//  Usa a biblioteca Keypad do kit Freenove (Keypad.hpp / Keypad.cpp /
//  Key.hpp / Key.cpp), portada da Arduino Keypad Library.
//
//  Compilar:
//    g++ Calculadora.cpp Keypad.cpp Key.cpp -o Calculadora \
//        -lwiringPi -lwiringPiDev
//  Executar:
//    sudo ./Calculadora
//
// -----------------------------------------------------------------------
//  Pinagem - Raspberry Pi 3B+ (numeracao BCM - wiringPiSetupGpio)
// -----------------------------------------------------------------------
//
//  Teclado 4x4 (Freenove FNK0054 - 21_Matrix_Keypad):
//    Linhas  [OUTPUT]        R1=GPIO16  R2=GPIO20  R3=GPIO21  R4=GPIO26
//    Colunas [INPUT pull-up] C1=GPIO19  C2=GPIO13  C3=GPIO6   C4=GPIO5
//
//  LCD 16x2 HD44780 + modulo I2C PCF8574:
//    VCC -> 5V     (pino fisico  2)
//    GND -> GND    (pino fisico  6)
//    SDA -> GPIO2  (pino fisico  3, I2C1 SDA - hardware)
//    SCL -> GPIO3  (pino fisico  5, I2C1 SCL - hardware)
//    Endereco I2C: 0x27  (verificar com: sudo i2cdetect -y 1)
//
// -----------------------------------------------------------------------
//  Teclas fisicas do teclado 4x4  ->  operacao da calculadora
// -----------------------------------------------------------------------
//   [ 1 ][ 2 ][ 3 ][ A ]     A = soma        (+)
//   [ 4 ][ 5 ][ 6 ][ B ]     B = subtracao   (-)
//   [ 7 ][ 8 ][ 9 ][ C ]     C = multiplic.  (*)
//   [ * ][ 0 ][ # ][ D ]     D = divisao     (/)
//                            * = fatorial    (!)
//                            # = calcular    (=)
//
//  Fluxo:  A -> [digitos] -> operador(A/B/C/D) -> B -> [digitos] -> #
//                         -> * (fatorial, so precisa de A)
// =====================================================================

#include "Keypad.hpp"
#include <wiringPiI2C.h>
#include <wiringPi.h>
#include <iostream>
#include <string>
#include <vector>
#include <cstdint>

// =====================================================================
//  BitNumber - inteiro de largura parametrizavel; bits[0] = LSB
// =====================================================================
class BitNumber {
public:
    int                  width;
    std::vector<uint8_t> bits;

    explicit BitNumber(int w = 8, unsigned long long value = 0)
        : width(w), bits(w, 0) {
        for (int i = 0; i < width && i < 64; ++i)
            bits[i] = static_cast<uint8_t>((value >> i) & 1ULL);
    }

    bool bit(int i)       const { return (i >= 0 && i < width) && bits[i]; }
    void setBit(int i, bool v)  { if (i >= 0 && i < width) bits[i] = v ? 1 : 0; }

    unsigned long long toULL() const {
        unsigned long long v = 0;
        for (int i = 0; i < width && i < 64; ++i)
            if (bits[i]) v |= (1ULL << i);
        return v;
    }

    bool isZero() const {
        for (auto b : bits) if (b) return false;
        return true;
    }
};

// =====================================================================
//  Somador completo de 1 bit (porta XOR/AND basica)
// =====================================================================
static inline void fullAdder(bool a, bool b, bool cin, bool& sum, bool& cout) {
    sum  = a ^ b ^ cin;
    cout = (a && b) || (cin && (a ^ b));
}

// =====================================================================
//  SOMA - ripple-carry: propaga carry do LSB ao MSB
// =====================================================================
BitNumber ula_add(const BitNumber& a, const BitNumber& b, bool& carry) {
    BitNumber r(a.width);
    carry = false;
    for (int i = 0; i < a.width; ++i) {
        bool s, c;
        fullAdder(a.bit(i), b.bit(i), carry, s, c);
        r.setBit(i, s);
        carry = c;
    }
    return r;
}

// =====================================================================
//  SUBTRACAO - complemento de dois (a + ~b + 1)
//  borrow=true quando a < b (resultado negativo)
// =====================================================================
BitNumber ula_sub(const BitNumber& a, const BitNumber& b, bool& borrow) {
    BitNumber r(a.width);
    bool carry = true; // "+1" do complemento de dois
    for (int i = 0; i < a.width; ++i) {
        bool s, c;
        fullAdder(a.bit(i), !b.bit(i), carry, s, c);
        r.setBit(i, s);
        carry = c;
    }
    borrow = !carry;
    return r;
}

// =====================================================================
//  MULTIPLICACAO - shift-and-add (sem instrucao MUL nativa)
//  Acumula em 2*width bits para detectar overflow
// =====================================================================
BitNumber ula_mul(const BitNumber& a, const BitNumber& b, bool& overflow) {
    int n = a.width;
    BitNumber prod(2 * n);
    for (int i = 0; i < n; ++i) {
        if (!b.bit(i)) continue;
        bool carry = false;
        for (int j = 0; j < n; ++j) {
            bool s, c;
            fullAdder(prod.bit(i + j), a.bit(j), carry, s, c);
            prod.setBit(i + j, s);
            carry = c;
        }
        for (int k = i + n; carry && k < prod.width; ++k) {
            bool s, c;
            fullAdder(prod.bit(k), false, carry, s, c);
            prod.setBit(k, s);
            carry = c;
        }
    }
    BitNumber r(n);
    for (int i = 0; i < n; ++i) r.setBit(i, prod.bit(i));
    overflow = false;
    for (int i = n; i < 2 * n; ++i) if (prod.bit(i)) { overflow = true; break; }
    return r;
}

// =====================================================================
//  DIVISAO - algoritmo de restauracao bit a bit
//  Retorna false e ativa divByZero quando divisor == 0
// =====================================================================
bool ula_div(const BitNumber& dividend, const BitNumber& divisor,
             BitNumber& quotient, BitNumber& remainder, bool& divByZero) {
    int n = dividend.width;
    if (divisor.isZero()) {
        divByZero = true;
        quotient = BitNumber(n); remainder = BitNumber(n);
        return false;
    }
    divByZero = false;
    quotient  = BitNumber(n);
    BitNumber rem(n + 1), div(n + 1);
    for (int i = 0; i < n; ++i) div.setBit(i, divisor.bit(i));

    for (int i = n - 1; i >= 0; --i) {
        for (int j = rem.width - 1; j > 0; --j) rem.setBit(j, rem.bit(j - 1));
        rem.setBit(0, dividend.bit(i));
        BitNumber tmp(rem.width);
        bool carry = true;
        for (int j = 0; j < rem.width; ++j) {
            bool s, c;
            fullAdder(rem.bit(j), !div.bit(j), carry, s, c);
            tmp.setBit(j, s); carry = c;
        }
        if (carry) { rem = tmp; quotient.setBit(i, true); }
    }
    remainder = BitNumber(n);
    for (int i = 0; i < n; ++i) remainder.setBit(i, rem.bit(i));
    return true;
}

// =====================================================================
//  FATORIAL - loop iterativo com monitoramento de overflow
//  Limite: 12! = 479001600 < 2^32; 13! estoura 32 bits
// =====================================================================
BitNumber ula_fat(unsigned long long n, int width, bool& overflow) {
    overflow = false;
    BitNumber result(width, 1);
    for (unsigned long long i = 2; i <= n; ++i) {
        bool ovf;
        result = ula_mul(result, BitNumber(width, i), ovf);
        if (ovf) { overflow = true; return result; }
    }
    return result;
}

// =====================================================================
//  LCD HD44780 via modulo I2C PCF8574
//
//  wiringPiI2CSetup(addr) abre /dev/i2c-1 e retorna fd.
//  wiringPiI2CWrite(fd, byte) escreve byte no PCF8574.
//
//  Mapa de bits PCF8574 -> HD44780:
//    bit7=D7  bit6=D6  bit5=D5  bit4=D4
//    bit3=BL  bit2=EN  bit1=RW  bit0=RS
// =====================================================================
class LCD_I2C {
    static constexpr uint8_t RS = 0x01; // Register Select (0=cmd, 1=dado)
    static constexpr uint8_t EN = 0x04; // Enable (borda de descida = latch)
    static constexpr uint8_t BL = 0x08; // Backlight

    int fd_ = -1;

    void i2cByte(uint8_t data) {
        wiringPiI2CWrite(fd_, static_cast<int>(data | BL));
    }

    void pulseEN(uint8_t nibble) {
        i2cByte(nibble | EN);
        delayMicroseconds(2);
        i2cByte(nibble & ~EN);
        delayMicroseconds(50);
    }

    // Envia byte em dois nibbles (modo 4 bits)
    void send8(uint8_t value, uint8_t mode) {
        pulseEN((value & 0xF0)        | mode); // nibble alto
        pulseEN(((value << 4) & 0xF0) | mode); // nibble baixo
    }

public:
    bool init(int i2cAddr = 0x27) {
        fd_ = wiringPiI2CSetup(i2cAddr);
        if (fd_ < 0) return false;

        delay(50); // aguarda estabilizacao apos power-on

        // Inicializacao em modo 4 bits (HD44780 datasheet, p.46)
        pulseEN(0x30); delay(5);
        pulseEN(0x30); delay(5);
        pulseEN(0x30); delay(1);
        pulseEN(0x20);              // entra em modo 4 bits

        send8(0x28, 0);             // 2 linhas, fonte 5x8
        send8(0x08, 0);             // display OFF
        send8(0x01, 0); delay(2);   // clear display
        send8(0x06, 0);             // entry mode: cursor avanca
        send8(0x0C, 0);             // display ON, cursor OFF
        return true;
    }

    void command(uint8_t cmd)   { send8(cmd, 0);  delayMicroseconds(100); }
    void writeChar(uint8_t d)   { send8(d,   RS); delayMicroseconds(50);  }
    void clear()                { command(0x01); delay(2); }

    void setCursor(int col, int row) {
        static const uint8_t row_off[] = {0x00, 0x40};
        command(0x80 | ((col & 0x0F) + row_off[row & 1]));
    }

    // Escreve linha completa de 16 chars sem dar clear (evita flickering)
    void printLine(int row, const std::string& s) {
        setCursor(0, row);
        for (int i = 0; i < 16; ++i)
            writeChar(static_cast<uint8_t>(i < (int)s.size() ? s[i] : ' '));
    }
};

// =====================================================================
//  Maquina de estados da Calculadora
// =====================================================================
enum class CalcState { ENTER_A, ENTER_B, SHOW_RESULT };

class Calculadora {
    LCD_I2C& lcd_;

    CalcState          state_  = CalcState::ENTER_A;
    unsigned long long val_a_  = 0;
    unsigned long long val_b_  = 0;
    char               op_     = '\0';   // operador logico: + - * / !
    unsigned long long result_ = 0;
    bool               neg_    = false;
    bool               error_  = false;
    std::string        errMsg_;

    // W=8 bits: suficiente para entradas 0-15 (max: 15*15=225 < 256)
    static constexpr int W = 8;

    static std::string numStr(unsigned long long v) {
        if (v == 0) return "0";
        std::string s;
        while (v > 0) { s = char('0' + v % 10) + s; v /= 10; }
        return s;
    }

    // Binario de 8 bits com separacao de nibbles: "0000 0101"
    static std::string binStr8(unsigned long long v) {
        std::string s;
        for (int i = 7; i >= 0; --i) {
            s += ((v >> i) & 1) ? '1' : '0';
            if (i == 4) s += ' ';
        }
        return s; // 9 chars: "XXXX XXXX"
    }

    void updateDisplay() {
        std::string line0, line1;

        switch (state_) {
        case CalcState::ENTER_A:
            line0 = "A: " + numStr(val_a_) + "_";
            line1 = "A+ B- C* D/ *=!";
            break;

        case CalcState::ENTER_B:
            line0 = numStr(val_a_) + " " + op_ + " " + numStr(val_b_) + "_";
            line1 = "Aperte # p/ calc";
            break;

        case CalcState::SHOW_RESULT:
            if (op_ == '!') {
                line0 = numStr(val_a_) + "!=";
            } else {
                line0 = numStr(val_a_) + " " + op_ + " " + numStr(val_b_) + "=";
            }

            if (error_) {
                line1 = errMsg_;
            } else if (op_ == '!') {
                line1 = numStr(result_);
            } else {
                // decimal + binario (complemento de dois se negativo)
                unsigned long long bv = neg_
                    ? ((~result_ + 1) & 0xFF)
                    : (result_ & 0xFF);
                std::string dec = (neg_ ? "-" : "") + numStr(result_);
                line1 = dec + " " + binStr8(bv);
            }
            break;
        }

        lcd_.printLine(0, line0);
        lcd_.printLine(1, line1);
    }

    void compute() {
        bool borrow = false, dbz = false, ovf = false;
        error_ = false; neg_ = false;

        if (op_ == '!') {
            if (val_a_ > 12) {
                error_ = true; errMsg_ = "OVF: N maximo=12";
            } else {
                BitNumber r = ula_fat(val_a_, 32, ovf);
                if (ovf) { error_ = true; errMsg_ = "OVERFLOW!"; }
                else      result_ = r.toULL();
            }
        } else {
            BitNumber a(W, val_a_), b(W, val_b_);

            if (op_ == '+') {
                bool carry;
                result_ = ula_add(a, b, carry).toULL();

            } else if (op_ == '-') {
                BitNumber r = ula_sub(a, b, borrow);
                if (borrow) { neg_ = true; result_ = val_b_ - val_a_; }
                else          result_ = r.toULL();

            } else if (op_ == '*') {
                result_ = ula_mul(a, b, ovf).toULL();

            } else if (op_ == '/') {
                BitNumber q, rem;
                if (!ula_div(a, b, q, rem, dbz)) {
                    error_ = true; errMsg_ = "ERR: DIV/ZERO!";
                } else {
                    result_ = q.toULL();
                }
            }
        }

        state_ = CalcState::SHOW_RESULT;
        updateDisplay();
    }

    // Traduz a tecla FISICA do teclado Freenove para operador LOGICO
    //   A->+  B->-  C->*  D->/  *->!(fatorial)  #->=(calcular)
    static char mapKey(char k) {
        switch (k) {
            case 'A': return '+';
            case 'B': return '-';
            case 'C': return '*';
            case 'D': return '/';
            case '*': return '!';
            case '#': return '=';
            default:  return k; // digitos 0-9 passam direto
        }
    }

public:
    explicit Calculadora(LCD_I2C& lcd) : lcd_(lcd) {}

    void reset() {
        state_ = CalcState::ENTER_A;
        val_a_ = val_b_ = 0;
        op_ = '\0'; neg_ = false; error_ = false;
        updateDisplay();
    }

    void processKey(char physicalKey) {
        if (physicalKey == 0) return;
        char k = mapKey(physicalKey);

        // Qualquer tecla no estado de resultado reinicia a calculadora
        if (state_ == CalcState::SHOW_RESULT) { reset(); return; }

        // Digito 0-9
        if (k >= '0' && k <= '9') {
            unsigned long long d = static_cast<unsigned long long>(k - '0');
            if (state_ == CalcState::ENTER_A) {
                val_a_ = val_a_ * 10 + d;
                if (val_a_ > 15) val_a_ = 15;   // clamp 4 bits (0-15)
            } else {
                val_b_ = val_b_ * 10 + d;
                if (val_b_ > 15) val_b_ = 15;
            }
            updateDisplay();
            return;
        }

        // Operador binario (A/B/C/D) - so apos digitar A
        if ((k == '+' || k == '-' || k == '*' || k == '/') &&
             state_ == CalcState::ENTER_A) {
            op_ = k; val_b_ = 0;
            state_ = CalcState::ENTER_B;
            updateDisplay();
            return;
        }

        // Fatorial (tecla *) - operador unario
        if (k == '!' && state_ == CalcState::ENTER_A) {
            op_ = '!'; compute(); return;
        }

        // Calcular (tecla #)
        if (k == '=' && state_ == CalcState::ENTER_B) {
            compute(); return;
        }
    }
};

// =====================================================================
//  Teclado - biblioteca Freenove (Keypad.hpp)
//  Pinagem BCM conforme 21_Matrix_Keypad do kit FNK0054
// =====================================================================
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'},
};

byte rowPins[ROWS] = {16, 20, 21, 26}; // R1 R2 R3 R4
byte colPins[COLS] = {19, 13,  6,  5}; // C1 C2 C3 C4

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// =====================================================================
//  main
// =====================================================================
int main() {
    wiringPiSetupGpio(); // numeracao BCM (necessario para o Keypad)

    LCD_I2C lcd;
    if (!lcd.init(0x27)) {
        std::cerr << "ERRO: LCD I2C nao encontrado em 0x27.\n"
                  << "  Execute: sudo i2cdetect -y 1\n"
                  << "  Se aparecer 0x3F, use lcd.init(0x3F)\n";
        return 1;
    }

    keypad.setDebounceTime(50);

    // Tela de abertura
    lcd.printLine(0, " Calculadora Bit");
    lcd.printLine(1, " RPi3B+ PCS3732 ");
    delay(2000);

    Calculadora calc(lcd);
    calc.reset();

    // Loop principal: le teclado e atualiza o LCD
    while (true) {
        char key = keypad.getKey();
        if (key) calc.processKey(key);
        delay(5); // polling a cada 5 ms
    }

    return 0;
}
