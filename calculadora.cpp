// =====================================================================
//  Calculadora bit a bit (nivel de hardware / ULA)
// ---------------------------------------------------------------------
//  As operacoes (soma, subtracao, multiplicacao, divisao e fatorial)
//  sao implementadas manipulando bits individuais — somador completo,
//  complemento de dois, shift-and-add e divisao por restauracao — em
//  vez de usar os operadores nativos do C++. A largura (numero de bits)
//  e parametrizavel em tempo de execucao para permitir o benchmark
//  "para mais bits".
//
//  Compilar:  g++ -O2 -std=c++17 calculadora.cpp -o calculadora
//  Executar:  ./calculadora
// =====================================================================

#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <cstdint>
#include <functional>

// ---------------------------------------------------------------------
//  Numero inteiro SEM SINAL de 'width' bits. bits[0] = LSB.
// ---------------------------------------------------------------------
class BitNumber {
public:
    int width;
    std::vector<uint8_t> bits; // cada elemento vale 0 ou 1

    explicit BitNumber(int w = 4, unsigned long long value = 0)
        : width(w), bits(w, 0) {
        for (int i = 0; i < width; ++i)
            bits[i] = (i < 64) ? static_cast<uint8_t>((value >> i) & 1ULL) : 0;
    }

    bool bit(int i) const { return (i >= 0 && i < width) ? bits[i] != 0 : false; }
    void setBit(int i, bool v) { if (i >= 0 && i < width) bits[i] = v ? 1 : 0; }

    // Valor nativo (apenas para exibir / validar; nao usado nas operacoes).
    unsigned long long toULL() const {
        unsigned long long v = 0;
        for (int i = 0; i < width && i < 64; ++i)
            if (bits[i]) v |= (1ULL << i);
        return v;
    }

    // Representacao binaria MSB..LSB.
    std::string toBinary() const {
        std::string s;
        for (int i = width - 1; i >= 0; --i) s += (bits[i] ? '1' : '0');
        return s;
    }

    bool isZero() const {
        for (int i = 0; i < width; ++i) if (bits[i]) return false;
        return true;
    }
};

// ---------------------------------------------------------------------
//  Somador completo de 1 bit.
//    soma  = a XOR b XOR cin
//    carry = maioria(a, b, cin)
// ---------------------------------------------------------------------
static inline void fullAdder(bool a, bool b, bool cin, bool& sum, bool& cout) {
    sum  = a ^ b ^ cin;
    cout = (a && b) || (cin && (a ^ b));
}

// ---------------------------------------------------------------------
//  SOMA — ripple carry (propaga o carry do LSB ao MSB).
//  'carryOut' sinaliza estouro de 'width' bits.
// ---------------------------------------------------------------------
BitNumber add(const BitNumber& a, const BitNumber& b, bool& carryOut) {
    int n = a.width;
    BitNumber r(n);
    bool carry = false;
    for (int i = 0; i < n; ++i) {
        bool s, c;
        fullAdder(a.bit(i), b.bit(i), carry, s, c);
        r.setBit(i, s);
        carry = c;
    }
    carryOut = carry;
    return r;
}

// ---------------------------------------------------------------------
//  Complemento de dois: inverte os bits e soma 1 (util para negar).
// ---------------------------------------------------------------------
BitNumber twosComplement(const BitNumber& a) {
    int n = a.width;
    BitNumber inv(n);
    for (int i = 0; i < n; ++i) inv.setBit(i, !a.bit(i));
    bool c;
    return add(inv, BitNumber(n, 1), c);
}

// ---------------------------------------------------------------------
//  SUBTRACAO — a - b = a + (~b) + 1, feita numa unica passada com
//  carry-in = 1. 'borrow' = 1 quando a < b (resultado negativo em C2).
// ---------------------------------------------------------------------
BitNumber sub(const BitNumber& a, const BitNumber& b, bool& borrow) {
    int n = a.width;
    BitNumber r(n);
    bool carry = true; // o "+1" do complemento de dois entra como carry-in
    for (int i = 0; i < n; ++i) {
        bool s, c;
        fullAdder(a.bit(i), !b.bit(i), carry, s, c);
        r.setBit(i, s);
        carry = c;
    }
    borrow = !carry; // sem carry-out final => houve emprestimo (borrow)
    return r;
}

// ---------------------------------------------------------------------
//  MULTIPLICACAO — shift-and-add. Acumula em 2*width bits para poder
//  detectar overflow; retorna o resultado truncado em 'width' bits.
// ---------------------------------------------------------------------
BitNumber mul(const BitNumber& a, const BitNumber& b, bool& overflow) {
    int n = a.width;
    BitNumber product(2 * n, 0); // acumulador de largura dupla
    for (int i = 0; i < n; ++i) {
        if (!b.bit(i)) continue;
        // soma 'a' deslocado i posicoes ao produto
        bool carry = false;
        for (int j = 0; j < n; ++j) {
            bool s, c;
            fullAdder(product.bit(i + j), a.bit(j), carry, s, c);
            product.setBit(i + j, s);
            carry = c;
        }
        // propaga o carry restante nos bits altos
        int k = i + n;
        while (carry && k < product.width) {
            bool s, c;
            fullAdder(product.bit(k), false, carry, s, c);
            product.setBit(k, s);
            carry = c;
            ++k;
        }
    }
    BitNumber result(n);
    for (int i = 0; i < n; ++i) result.setBit(i, product.bit(i));
    overflow = false;
    for (int i = n; i < 2 * n; ++i) if (product.bit(i)) { overflow = true; break; }
    return result;
}

// ---------------------------------------------------------------------
//  DIVISAO inteira sem sinal pelo metodo de restauracao (bit a bit).
//  Retorna false e ativa 'divByZero' quando o divisor e 0.
// ---------------------------------------------------------------------
bool divide(const BitNumber& dividend, const BitNumber& divisor,
            BitNumber& quotient, BitNumber& remainder, bool& divByZero) {
    int n = dividend.width;

    if (divisor.isZero()) {               // ---- tratamento de divisao por zero ----
        divByZero = true;
        quotient  = BitNumber(n, 0);
        remainder = BitNumber(n, 0);
        return false;
    }
    divByZero = false;

    quotient = BitNumber(n, 0);
    BitNumber rem(n + 1, 0);              // resto precisa de 1 bit extra no shift
    BitNumber div(n + 1, 0);
    for (int i = 0; i < n; ++i) div.setBit(i, divisor.bit(i));

    for (int i = n - 1; i >= 0; --i) {
        // desloca o resto para a esquerda e traz o bit i do dividendo
        for (int j = rem.width - 1; j > 0; --j) rem.setBit(j, rem.bit(j - 1));
        rem.setBit(0, dividend.bit(i));

        // tenta subtrair o divisor (comparacao via subtracao bit a bit)
        BitNumber tmp(rem.width);
        bool carry = true;
        for (int j = 0; j < rem.width; ++j) {
            bool s, c;
            fullAdder(rem.bit(j), !div.bit(j), carry, s, c);
            tmp.setBit(j, s);
            carry = c;
        }
        bool borrow = !carry;
        if (!borrow) {                    // resto >= divisor: mantem a subtracao
            rem = tmp;
            quotient.setBit(i, true);
        } else {                          // resto <  divisor: restaura (descarta tmp)
            quotient.setBit(i, false);
        }
    }
    remainder = BitNumber(n, 0);
    for (int i = 0; i < n; ++i) remainder.setBit(i, rem.bit(i));
    return true;
}

// ---------------------------------------------------------------------
//  FATORIAL — produto sucessivo (2..n) usando a multiplicacao bit a bit.
//  Ativa 'overflow' se o resultado nao couber em 'width' bits.
//  (o contador do laco e nativo; a aritmetica do produto e bit a bit)
// ---------------------------------------------------------------------
BitNumber factorial(const BitNumber& n, bool& overflow) {
    int w = n.width;
    overflow = false;
    BitNumber result(w, 1);              // 0! = 1! = 1
    unsigned long long limit = n.toULL();
    for (unsigned long long i = 2; i <= limit; ++i) {
        bool ovf;
        result = mul(result, BitNumber(w, i), ovf);
        if (ovf) { overflow = true; break; }
    }
    return result;
}

// =====================================================================
//  DEMONSTRACAO — operacoes em 4 bits (0 a 15), incluindo divisao/0.
// =====================================================================
void demonstracao() {
    const int W = 4;
    std::cout << "=====================================================\n"
              << " DEMONSTRACAO -- Operacoes em " << W << " bits (0 a 15)\n"
              << "=====================================================\n\n";

    BitNumber a(W, 11); // 1011
    BitNumber b(W, 6);  // 0110
    bool carry, borrow, ovf, dbz;

    BitNumber s = add(a, b, carry);
    std::cout << "SOMA:          " << a.toBinary() << " (" << a.toULL() << ") + "
              << b.toBinary() << " (" << b.toULL() << ") = "
              << s.toBinary() << " (" << s.toULL() << ")"
              << (carry ? "   [carry-out=1 -> estourou 4 bits]" : "") << "\n";

    BitNumber d = sub(a, b, borrow);
    std::cout << "SUBTRACAO:     " << a.toBinary() << " (" << a.toULL() << ") - "
              << b.toBinary() << " (" << b.toULL() << ") = "
              << d.toBinary() << " (" << d.toULL() << ")"
              << (borrow ? "   [borrow=1]" : "") << "\n";

    BitNumber d2 = sub(b, a, borrow);
    long long sgn = borrow ? (long long)d2.toULL() - (1LL << W) : (long long)d2.toULL();
    std::cout << "SUBTRACAO:     " << b.toBinary() << " (" << b.toULL() << ") - "
              << a.toBinary() << " (" << a.toULL() << ") = "
              << d2.toBinary() << " (" << d2.toULL() << ")";
    if (borrow) std::cout << "   [borrow=1 -> em C2 = " << sgn << "]";
    std::cout << "\n";

    BitNumber m = mul(a, b, ovf);
    std::cout << "MULTIPLICACAO: " << a.toBinary() << " (" << a.toULL() << ") * "
              << b.toBinary() << " (" << b.toULL() << ") = "
              << m.toBinary() << " (" << m.toULL() << ")"
              << (ovf ? "   [OVERFLOW: 66 nao cabe em 4 bits]" : "") << "\n";

    BitNumber q, r;
    divide(a, b, q, r, dbz);
    std::cout << "DIVISAO:       " << a.toBinary() << " (" << a.toULL() << ") / "
              << b.toBinary() << " (" << b.toULL() << ") = "
              << q.toBinary() << " (" << q.toULL() << ")  resto "
              << r.toBinary() << " (" << r.toULL() << ")\n";

    bool ok = divide(a, BitNumber(W, 0), q, r, dbz);
    std::cout << "DIVISAO/ZERO:  " << a.toBinary() << " (" << a.toULL() << ") / 0000 (0) = ";
    std::cout << ((!ok && dbz) ? "ERRO: divisao por zero detectada e tratada.\n"
                               : "(inesperado)\n");

    for (unsigned long long n : {3ULL, 5ULL}) {
        BitNumber f = factorial(BitNumber(W, n), ovf);
        std::cout << "FATORIAL:      " << n << "! = ";
        if (ovf) std::cout << "OVERFLOW (nao cabe em 4 bits)\n";
        else     std::cout << f.toULL() << " (" << f.toBinary() << ")\n";
    }
    std::cout << "\n";
}

// =====================================================================
//  BENCHMARK — tempo medio por operacao em funcao da largura (bits).
// =====================================================================
static double timeOp(long iters, const std::function<void()>& f) {
    auto t0 = std::chrono::high_resolution_clock::now();
    for (long i = 0; i < iters; ++i) f();
    auto t1 = std::chrono::high_resolution_clock::now();
    double ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    return ns / (double)iters;
}

void benchmark() {
    std::cout << "=====================================================\n"
              << " BENCHMARK -- tempo por operacao x largura (bits)\n"
              << "=====================================================\n"
              << "(tempo medio em NANOSSEGUNDOS por operacao)\n\n";

    std::vector<int> widths = {4, 8, 16, 32, 64};
    const long ITER = 300000;
    volatile unsigned long long sink = 0; // impede que o otimizador remova o laco

    std::cout << std::left << std::setw(8) << "bits"
              << std::right << std::setw(11) << "soma"
              << std::setw(12) << "subtracao"
              << std::setw(13) << "multiplic."
              << std::setw(11) << "divisao"
              << std::setw(11) << "fatorial" << "\n"
              << std::string(66, '-') << "\n";

    for (int W : widths) {
        // operandos representativos que preenchem a largura
        unsigned long long va = (W >= 64) ? 0xC000000000000000ULL : (3ULL << (W - 2));
        unsigned long long vb = (W >= 64) ? 0x5000000000000001ULL : ((1ULL << (W - 2)) | 1ULL);
        BitNumber a(W, va), b(W, vb);
        bool flag;

        double tS = timeOp(ITER,      [&]{ auto r = add(a, b, flag); sink += r.bit(0); });
        double tD = timeOp(ITER,      [&]{ auto r = sub(a, b, flag); sink += r.bit(0); });
        double tM = timeOp(ITER,      [&]{ auto r = mul(a, b, flag); sink += r.bit(0); });
        double tV = timeOp(ITER,      [&]{ BitNumber q, rem; bool z;
                                           divide(a, b, q, rem, z); sink += q.bit(0); });
        // fatorial: entrada fixa; o nº de multiplicacoes cresce ate estourar o registrador
        BitNumber nf(W, (W >= 8 ? 12ULL : 6ULL));
        double tF = timeOp(ITER / 40 + 1,
                                      [&]{ auto r = factorial(nf, flag); sink += r.bit(0); });

        std::cout << std::left << std::setw(8) << W
                  << std::right << std::fixed << std::setprecision(1)
                  << std::setw(11) << tS
                  << std::setw(12) << tD
                  << std::setw(13) << tM
                  << std::setw(11) << tV
                  << std::setw(11) << tF << "\n";
    }
    std::cout << "\nObs.: soma/subtracao crescem ~O(n)   (ripple carry);\n"
              << "      multiplic./divisao ~O(n^2)      (shift-and-add / restauracao);\n"
              << "      fatorial = varias multiplicacoes ate o registrador estourar.\n\n";
    (void)sink;
}

// =====================================================================
//  CALCULADORA INTERATIVA
// =====================================================================
void menuInterativo() {
    int width = 4;
    std::cout << "=====================================================\n"
              << " CALCULADORA INTERATIVA (bit a bit)\n"
              << "=====================================================\n";
    while (true) {
        std::cout << "\n[largura = " << width << " bits, faixa 0.."
                  << ((width < 64) ? ((1ULL << width) - 1) : 0xFFFFFFFFFFFFFFFFULL) << "]\n"
                  << "  1) Soma        2) Subtracao   3) Multiplicacao\n"
                  << "  4) Divisao     5) Fatorial    6) Mudar largura\n"
                  << "  0) Sair\nEscolha: ";
        int op;
        if (!(std::cin >> op)) { std::cout << "\nEntrada encerrada.\n"; break; }
        if (op == 0) { std::cout << "Encerrando.\n"; break; }

        if (op == 6) {
            std::cout << "Nova largura (1..64): ";
            int w; if (std::cin >> w && w >= 1 && w <= 64) width = w;
            else std::cout << "Valor invalido.\n";
            continue;
        }
        if (op == 5) { // fatorial: 1 operando
            std::cout << "n = ";
            unsigned long long x; if (!(std::cin >> x)) break;
            bool ovf; BitNumber f = factorial(BitNumber(width, x), ovf);
            if (ovf) std::cout << "-> " << x << "! nao cabe em " << width << " bits (OVERFLOW)\n";
            else     std::cout << "-> " << x << "! = " << f.toULL() << " (" << f.toBinary() << ")\n";
            continue;
        }
        if (op >= 1 && op <= 4) { // dois operandos
            std::cout << "a = "; unsigned long long x; if (!(std::cin >> x)) break;
            std::cout << "b = "; unsigned long long y; if (!(std::cin >> y)) break;
            BitNumber a(width, x), b(width, y);
            bool flag;
            if (op == 1) { BitNumber r = add(a, b, flag);
                std::cout << "-> " << r.toULL() << " (" << r.toBinary() << ")"
                          << (flag ? "  [carry-out]" : "") << "\n"; }
            else if (op == 2) { BitNumber r = sub(a, b, flag);
                long long sg = flag ? (long long)r.toULL() - (1LL << width) : (long long)r.toULL();
                std::cout << "-> " << r.toULL() << " (" << r.toBinary() << ")"
                          << (flag ? "  [borrow; em C2 = " + std::to_string(sg) + "]" : "") << "\n"; }
            else if (op == 3) { BitNumber r = mul(a, b, flag);
                std::cout << "-> " << r.toULL() << " (" << r.toBinary() << ")"
                          << (flag ? "  [OVERFLOW]" : "") << "\n"; }
            else { BitNumber q, rem; bool z;
                if (!divide(a, b, q, rem, z)) std::cout << "-> ERRO: divisao por zero.\n";
                else std::cout << "-> quociente " << q.toULL() << " (" << q.toBinary()
                               << "), resto " << rem.toULL() << " (" << rem.toBinary() << ")\n"; }
            continue;
        }
        std::cout << "Opcao invalida.\n";
    }
}

int main() {
    demonstracao();   // mostra soma, subtracao, multiplicacao, divisao, divisao/0 e fatorial em 4 bits
    benchmark();      // tabela de tempos por largura (4..64 bits)
    menuInterativo(); // calculadora interativa (encerra com 0 ou EOF)
    return 0;
}
