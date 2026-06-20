#pragma once

#include <cstdint>
#include <functional>

// =============================================================================
//  MOS 6510/6502-Prozessor - das Herz des kleinen C64-Kerns fuer .sid-Wiedergabe.
//  Arbeitet direkt auf 64 KB Speicher (schnell). Schreibzugriffe laufen ueber
//  writeHook, damit der SID-Bereich D400..D7FF an reSIDfp weitergereicht wird.
//  Offizieller Befehlssatz + gaengige illegale Opcodes; Unbekanntes = NOP.
// =============================================================================
class Mos6502
{
public:
    uint8_t* mem = nullptr;
    std::function<void (uint16_t, uint8_t)> writeHook;

    uint8_t  A = 0, X = 0, Y = 0, S = 0xFF;
    uint16_t PC = 0;
    bool C = false, Z = false, I = false, D = false, V = false, N = false;

    void reset (uint16_t pc) { A = X = Y = 0; S = 0xFF; PC = pc; C = Z = I = D = V = N = false; }

    // isIrq=false: Routine wird wie ein Unterprogramm aufgerufen und endet mit RTS
    // (PC = 0x0000+1). isIrq=true: IRQ-Handler, endet mit RTI (zieht Status + PC) ->
    // wir legen die passende Ruecksprung-Adresse 0x0001 ab. Beide brechen bei PC==1 ab.
    void runRoutine (uint16_t addr, uint8_t a, uint8_t x, uint8_t y,
                     bool isIrq = false, int cycleLimit = 2000000)
    {
        A = a; X = x; Y = y;
        if (isIrq) { push16 (0x0001); push ((uint8_t) (getP() & ~0x10)); }
        else       { push16 (0x0000); }
        PC = addr;
        int cycles = 0;
        while (cycles < cycleLimit)
        {
            if (PC == 0x0001) break;
            cycles += step();
        }
    }

    int step()
    {
        const uint8_t op = rd (PC++);
        switch (op)
        {
            case 0xA9: A = setNZ (imm());            return 2;
            case 0xA5: A = setNZ (rd (zp()));        return 3;
            case 0xB5: A = setNZ (rd (zpX()));       return 4;
            case 0xAD: A = setNZ (rd (ab()));        return 4;
            case 0xBD: A = setNZ (rd (abX()));       return 4;
            case 0xB9: A = setNZ (rd (abY()));       return 4;
            case 0xA1: A = setNZ (rd (inX()));       return 6;
            case 0xB1: A = setNZ (rd (inY()));       return 5;
            case 0xA2: X = setNZ (imm());            return 2;
            case 0xA6: X = setNZ (rd (zp()));        return 3;
            case 0xB6: X = setNZ (rd (zpY()));       return 4;
            case 0xAE: X = setNZ (rd (ab()));        return 4;
            case 0xBE: X = setNZ (rd (abY()));       return 4;
            case 0xA0: Y = setNZ (imm());            return 2;
            case 0xA4: Y = setNZ (rd (zp()));        return 3;
            case 0xB4: Y = setNZ (rd (zpX()));       return 4;
            case 0xAC: Y = setNZ (rd (ab()));        return 4;
            case 0xBC: Y = setNZ (rd (abX()));       return 4;
            case 0x85: wr (zp(),  A);                return 3;
            case 0x95: wr (zpX(), A);                return 4;
            case 0x8D: wr (ab(),  A);                return 4;
            case 0x9D: wr (abX(), A);                return 5;
            case 0x99: wr (abY(), A);                return 5;
            case 0x81: wr (inX(), A);                return 6;
            case 0x91: wr (inY(), A);                return 6;
            case 0x86: wr (zp(),  X);                return 3;
            case 0x96: wr (zpY(), X);                return 4;
            case 0x8E: wr (ab(),  X);                return 4;
            case 0x84: wr (zp(),  Y);                return 3;
            case 0x94: wr (zpX(), Y);                return 4;
            case 0x8C: wr (ab(),  Y);                return 4;
            case 0xAA: X = setNZ (A);                return 2;
            case 0xA8: Y = setNZ (A);                return 2;
            case 0xBA: X = setNZ (S);                return 2;
            case 0x8A: A = setNZ (X);                return 2;
            case 0x9A: S = X;                        return 2;
            case 0x98: A = setNZ (Y);                return 2;
            case 0x48: push (A);                     return 3;
            case 0x08: push ((uint8_t) (getP() | 0x10)); return 3;
            case 0x68: A = setNZ (pull());           return 4;
            case 0x28: setP (pull());                return 4;
            case 0x29: A = setNZ (A & imm());        return 2;
            case 0x25: A = setNZ (A & rd (zp()));    return 3;
            case 0x35: A = setNZ (A & rd (zpX()));   return 4;
            case 0x2D: A = setNZ (A & rd (ab()));    return 4;
            case 0x3D: A = setNZ (A & rd (abX()));   return 4;
            case 0x39: A = setNZ (A & rd (abY()));   return 4;
            case 0x21: A = setNZ (A & rd (inX()));   return 6;
            case 0x31: A = setNZ (A & rd (inY()));   return 5;
            case 0x09: A = setNZ (A | imm());        return 2;
            case 0x05: A = setNZ (A | rd (zp()));    return 3;
            case 0x15: A = setNZ (A | rd (zpX()));   return 4;
            case 0x0D: A = setNZ (A | rd (ab()));    return 4;
            case 0x1D: A = setNZ (A | rd (abX()));   return 4;
            case 0x19: A = setNZ (A | rd (abY()));   return 4;
            case 0x01: A = setNZ (A | rd (inX()));   return 6;
            case 0x11: A = setNZ (A | rd (inY()));   return 5;
            case 0x49: A = setNZ (A ^ imm());        return 2;
            case 0x45: A = setNZ (A ^ rd (zp()));    return 3;
            case 0x55: A = setNZ (A ^ rd (zpX()));   return 4;
            case 0x4D: A = setNZ (A ^ rd (ab()));    return 4;
            case 0x5D: A = setNZ (A ^ rd (abX()));   return 4;
            case 0x59: A = setNZ (A ^ rd (abY()));   return 4;
            case 0x41: A = setNZ (A ^ rd (inX()));   return 6;
            case 0x51: A = setNZ (A ^ rd (inY()));   return 5;
            case 0x24: bit (rd (zp()));              return 3;
            case 0x2C: bit (rd (ab()));              return 4;
            case 0x69: adc (imm());                  return 2;
            case 0x65: adc (rd (zp()));              return 3;
            case 0x75: adc (rd (zpX()));             return 4;
            case 0x6D: adc (rd (ab()));              return 4;
            case 0x7D: adc (rd (abX()));             return 4;
            case 0x79: adc (rd (abY()));             return 4;
            case 0x61: adc (rd (inX()));             return 6;
            case 0x71: adc (rd (inY()));             return 5;
            case 0xE9: case 0xEB: sbc (imm());       return 2;
            case 0xE5: sbc (rd (zp()));              return 3;
            case 0xF5: sbc (rd (zpX()));             return 4;
            case 0xED: sbc (rd (ab()));              return 4;
            case 0xFD: sbc (rd (abX()));             return 4;
            case 0xF9: sbc (rd (abY()));             return 4;
            case 0xE1: sbc (rd (inX()));             return 6;
            case 0xF1: sbc (rd (inY()));             return 5;
            case 0xC9: cmp (A, imm());               return 2;
            case 0xC5: cmp (A, rd (zp()));           return 3;
            case 0xD5: cmp (A, rd (zpX()));          return 4;
            case 0xCD: cmp (A, rd (ab()));           return 4;
            case 0xDD: cmp (A, rd (abX()));          return 4;
            case 0xD9: cmp (A, rd (abY()));          return 4;
            case 0xC1: cmp (A, rd (inX()));          return 6;
            case 0xD1: cmp (A, rd (inY()));          return 5;
            case 0xE0: cmp (X, imm());               return 2;
            case 0xE4: cmp (X, rd (zp()));           return 3;
            case 0xEC: cmp (X, rd (ab()));           return 4;
            case 0xC0: cmp (Y, imm());               return 2;
            case 0xC4: cmp (Y, rd (zp()));           return 3;
            case 0xCC: cmp (Y, rd (ab()));           return 4;
            case 0xE6: { auto a = zp();  wr (a, setNZ (rd (a) + 1)); } return 5;
            case 0xF6: { auto a = zpX(); wr (a, setNZ (rd (a) + 1)); } return 6;
            case 0xEE: { auto a = ab();  wr (a, setNZ (rd (a) + 1)); } return 6;
            case 0xFE: { auto a = abX(); wr (a, setNZ (rd (a) + 1)); } return 7;
            case 0xC6: { auto a = zp();  wr (a, setNZ (rd (a) - 1)); } return 5;
            case 0xD6: { auto a = zpX(); wr (a, setNZ (rd (a) - 1)); } return 6;
            case 0xCE: { auto a = ab();  wr (a, setNZ (rd (a) - 1)); } return 6;
            case 0xDE: { auto a = abX(); wr (a, setNZ (rd (a) - 1)); } return 7;
            case 0xE8: X = setNZ (X + 1);            return 2;
            case 0xC8: Y = setNZ (Y + 1);            return 2;
            case 0xCA: X = setNZ (X - 1);            return 2;
            case 0x88: Y = setNZ (Y - 1);            return 2;
            case 0x0A: A = asl (A);                  return 2;
            case 0x06: { auto a = zp();  wr (a, asl (rd (a))); } return 5;
            case 0x16: { auto a = zpX(); wr (a, asl (rd (a))); } return 6;
            case 0x0E: { auto a = ab();  wr (a, asl (rd (a))); } return 6;
            case 0x1E: { auto a = abX(); wr (a, asl (rd (a))); } return 7;
            case 0x4A: A = lsr (A);                  return 2;
            case 0x46: { auto a = zp();  wr (a, lsr (rd (a))); } return 5;
            case 0x56: { auto a = zpX(); wr (a, lsr (rd (a))); } return 6;
            case 0x4E: { auto a = ab();  wr (a, lsr (rd (a))); } return 6;
            case 0x5E: { auto a = abX(); wr (a, lsr (rd (a))); } return 7;
            case 0x2A: A = rol (A);                  return 2;
            case 0x26: { auto a = zp();  wr (a, rol (rd (a))); } return 5;
            case 0x36: { auto a = zpX(); wr (a, rol (rd (a))); } return 6;
            case 0x2E: { auto a = ab();  wr (a, rol (rd (a))); } return 6;
            case 0x3E: { auto a = abX(); wr (a, rol (rd (a))); } return 7;
            case 0x6A: A = ror (A);                  return 2;
            case 0x66: { auto a = zp();  wr (a, ror (rd (a))); } return 5;
            case 0x76: { auto a = zpX(); wr (a, ror (rd (a))); } return 6;
            case 0x6E: { auto a = ab();  wr (a, ror (rd (a))); } return 6;
            case 0x7E: { auto a = abX(); wr (a, ror (rd (a))); } return 7;
            case 0x4C: PC = ab();                    return 3;
            case 0x6C: { uint16_t p = ab(); uint16_t lo = rd (p);
                         uint16_t hi = rd ((uint16_t) ((p & 0xFF00) | ((p + 1) & 0x00FF)));
                         PC = (uint16_t) (lo | (hi << 8)); } return 5;
            case 0x20: { uint16_t a = ab(); push16 ((uint16_t) (PC - 1)); PC = a; } return 6;
            case 0x60: PC = (uint16_t) (pull16() + 1); return 6;
            case 0x40: { setP (pull()); PC = pull16(); } return 6;
            case 0x10: return branch (! N);
            case 0x30: return branch (  N);
            case 0x50: return branch (! V);
            case 0x70: return branch (  V);
            case 0x90: return branch (! C);
            case 0xB0: return branch (  C);
            case 0xD0: return branch (! Z);
            case 0xF0: return branch (  Z);
            case 0x18: C = false; return 2;
            case 0x38: C = true;  return 2;
            case 0x58: I = false; return 2;
            case 0x78: I = true;  return 2;
            case 0xB8: V = false; return 2;
            case 0xD8: D = false; return 2;
            case 0xF8: D = true;  return 2;
            case 0xEA: return 2;
            case 0x00: PC++;     return 7;
            case 0xA7: { uint8_t v = rd (zp());  A = X = setNZ (v); } return 3;
            case 0xB7: { uint8_t v = rd (zpY()); A = X = setNZ (v); } return 4;
            case 0xAF: { uint8_t v = rd (ab());  A = X = setNZ (v); } return 4;
            case 0xBF: { uint8_t v = rd (abY()); A = X = setNZ (v); } return 4;
            case 0xA3: { uint8_t v = rd (inX()); A = X = setNZ (v); } return 6;
            case 0xB3: { uint8_t v = rd (inY()); A = X = setNZ (v); } return 5;
            case 0x87: wr (zp(),  (uint8_t) (A & X)); return 3;
            case 0x97: wr (zpY(), (uint8_t) (A & X)); return 4;
            case 0x8F: wr (ab(),  (uint8_t) (A & X)); return 4;
            case 0x83: wr (inX(), (uint8_t) (A & X)); return 6;
            case 0xC7: { auto a = zp();  dcp (a); } return 5;
            case 0xD7: { auto a = zpX(); dcp (a); } return 6;
            case 0xCF: { auto a = ab();  dcp (a); } return 6;
            case 0xDF: { auto a = abX(); dcp (a); } return 7;
            case 0xDB: { auto a = abY(); dcp (a); } return 7;
            case 0xC3: { auto a = inX(); dcp (a); } return 8;
            case 0xD3: { auto a = inY(); dcp (a); } return 8;
            case 0xE7: { auto a = zp();  isc (a); } return 5;
            case 0xF7: { auto a = zpX(); isc (a); } return 6;
            case 0xEF: { auto a = ab();  isc (a); } return 6;
            case 0xFF: { auto a = abX(); isc (a); } return 7;
            case 0xFB: { auto a = abY(); isc (a); } return 7;
            case 0xE3: { auto a = inX(); isc (a); } return 8;
            case 0xF3: { auto a = inY(); isc (a); } return 8;
            case 0x07: { auto a = zp();  slo (a); } return 5;
            case 0x17: { auto a = zpX(); slo (a); } return 6;
            case 0x0F: { auto a = ab();  slo (a); } return 6;
            case 0x1F: { auto a = abX(); slo (a); } return 7;
            case 0x1B: { auto a = abY(); slo (a); } return 7;
            case 0x03: { auto a = inX(); slo (a); } return 8;
            case 0x13: { auto a = inY(); slo (a); } return 8;
            case 0x27: { auto a = zp();  rla (a); } return 5;
            case 0x37: { auto a = zpX(); rla (a); } return 6;
            case 0x2F: { auto a = ab();  rla (a); } return 6;
            case 0x3F: { auto a = abX(); rla (a); } return 7;
            case 0x3B: { auto a = abY(); rla (a); } return 7;
            case 0x23: { auto a = inX(); rla (a); } return 8;
            case 0x33: { auto a = inY(); rla (a); } return 8;
            case 0x47: { auto a = zp();  sre (a); } return 5;
            case 0x57: { auto a = zpX(); sre (a); } return 6;
            case 0x4F: { auto a = ab();  sre (a); } return 6;
            case 0x5F: { auto a = abX(); sre (a); } return 7;
            case 0x5B: { auto a = abY(); sre (a); } return 7;
            case 0x43: { auto a = inX(); sre (a); } return 8;
            case 0x53: { auto a = inY(); sre (a); } return 8;
            case 0x67: { auto a = zp();  rra (a); } return 5;
            case 0x77: { auto a = zpX(); rra (a); } return 6;
            case 0x6F: { auto a = ab();  rra (a); } return 6;
            case 0x7F: { auto a = abX(); rra (a); } return 7;
            case 0x7B: { auto a = abY(); rra (a); } return 7;
            case 0x63: { auto a = inX(); rra (a); } return 8;
            case 0x73: { auto a = inY(); rra (a); } return 8;
            case 0x0B: case 0x2B: { A = setNZ (A & imm()); C = (A & 0x80) != 0; } return 2;
            case 0x4B: { A &= imm(); A = lsr (A); }   return 2;
            case 0x6B: { A &= imm(); A = ror (A); C = (A & 0x40) != 0; V = ((A >> 6) ^ (A >> 5)) & 1; } return 2;
            case 0xCB: { uint8_t v = imm(); uint8_t ax = (uint8_t) (A & X); C = ax >= v; X = setNZ ((uint8_t) (ax - v)); } return 2;
            case 0x80: case 0x82: case 0x89: case 0xC2: case 0xE2: imm(); return 2;
            case 0x04: case 0x44: case 0x64: zp();  return 3;
            case 0x14: case 0x34: case 0x54: case 0x74: case 0xD4: case 0xF4: zpX(); return 4;
            case 0x0C: ab();  return 4;
            case 0x1C: case 0x3C: case 0x5C: case 0x7C: case 0xDC: case 0xFC: abX(); return 4;
            default: return 2;
        }
    }

private:
    inline uint8_t rd (uint16_t a) const { return mem[a]; }
    inline void    wr (uint16_t a, uint8_t v)
    {
        mem[a] = v;
        if (writeHook && a >= 0xD400 && a <= 0xD7FF) writeHook (a, v);
    }
    inline uint8_t  imm()  { return rd (PC++); }
    inline uint16_t zp()   { return rd (PC++); }
    inline uint16_t zpX()  { return (uint8_t) (rd (PC++) + X); }
    inline uint16_t zpY()  { return (uint8_t) (rd (PC++) + Y); }
    inline uint16_t ab()   { uint16_t lo = rd (PC++); uint16_t hi = rd (PC++); return (uint16_t) (lo | (hi << 8)); }
    inline uint16_t abX()  { return (uint16_t) (ab() + X); }
    inline uint16_t abY()  { return (uint16_t) (ab() + Y); }
    inline uint16_t inX()  { uint8_t p = (uint8_t) (rd (PC++) + X); uint16_t lo = mem[p]; uint16_t hi = mem[(uint8_t) (p + 1)]; return (uint16_t) (lo | (hi << 8)); }
    inline uint16_t inY()  { uint8_t p = rd (PC++); uint16_t lo = mem[p]; uint16_t hi = mem[(uint8_t) (p + 1)]; return (uint16_t) ((lo | (hi << 8)) + Y); }
    inline void     push (uint8_t v)    { mem[0x0100 + S] = v; S--; }
    inline uint8_t  pull()              { S++; return mem[0x0100 + S]; }
    inline void     push16 (uint16_t v) { push ((uint8_t) (v >> 8)); push ((uint8_t) (v & 0xFF)); }
    inline uint16_t pull16()            { uint16_t lo = pull(); uint16_t hi = pull(); return (uint16_t) (lo | (hi << 8)); }
    inline uint8_t setNZ (int v) { uint8_t r = (uint8_t) v; Z = (r == 0); N = (r & 0x80) != 0; return r; }
    uint8_t getP() const
    {
        return (uint8_t) ((C ? 0x01 : 0) | (Z ? 0x02 : 0) | (I ? 0x04 : 0) | (D ? 0x08 : 0)
                        | 0x20 | (V ? 0x40 : 0) | (N ? 0x80 : 0));
    }
    void setP (uint8_t p) { C = p & 0x01; Z = p & 0x02; I = p & 0x04; D = p & 0x08; V = p & 0x40; N = p & 0x80; }
    inline void bit (uint8_t v) { Z = (A & v) == 0; N = (v & 0x80) != 0; V = (v & 0x40) != 0; }
    inline void cmp (uint8_t reg, uint8_t v) { C = reg >= v; setNZ ((uint8_t) (reg - v)); }
    void adc (uint8_t v)
    {
        if (D)
        {
            int lo = (A & 0x0F) + (v & 0x0F) + (C ? 1 : 0);
            int hi = (A >> 4) + (v >> 4);
            if (lo > 9) { lo += 6; hi++; }
            V = (~(A ^ v) & (A ^ (hi << 4)) & 0x80) != 0;
            if (hi > 9) hi += 6;
            C = hi > 15;
            A = setNZ ((uint8_t) ((hi << 4) | (lo & 0x0F)));
        }
        else
        {
            int t = A + v + (C ? 1 : 0);
            V = (~(A ^ v) & (A ^ t) & 0x80) != 0;
            C = t > 0xFF;
            A = setNZ ((uint8_t) t);
        }
    }
    void sbc (uint8_t v)
    {
        int t = A - v - (C ? 0 : 1);
        V = ((A ^ v) & (A ^ t) & 0x80) != 0;
        if (D)
        {
            int lo = (A & 0x0F) - (v & 0x0F) - (C ? 0 : 1);
            int hi = (A >> 4) - (v >> 4);
            if (lo < 0) { lo += 10; hi--; }
            if (hi < 0) hi += 10;
            C = (t & 0xFF00) == 0;
            A = setNZ ((uint8_t) ((hi << 4) | (lo & 0x0F)));
        }
        else { C = t >= 0; A = setNZ ((uint8_t) t); }
    }
    inline uint8_t asl (uint8_t v) { C = (v & 0x80) != 0; return setNZ ((uint8_t) (v << 1)); }
    inline uint8_t lsr (uint8_t v) { C = (v & 0x01) != 0; return setNZ ((uint8_t) (v >> 1)); }
    inline uint8_t rol (uint8_t v) { uint8_t c = C ? 1 : 0;    C = (v & 0x80) != 0; return setNZ ((uint8_t) ((v << 1) | c)); }
    inline uint8_t ror (uint8_t v) { uint8_t c = C ? 0x80 : 0; C = (v & 0x01) != 0; return setNZ ((uint8_t) ((v >> 1) | c)); }
    int branch (bool take)
    {
        int8_t off = (int8_t) rd (PC++);
        if (! take) return 2;
        PC = (uint16_t) (PC + off);
        return 3;
    }
    inline void dcp (uint16_t a) { uint8_t v = (uint8_t) (rd (a) - 1); wr (a, v); cmp (A, v); }
    inline void isc (uint16_t a) { uint8_t v = (uint8_t) (rd (a) + 1); wr (a, v); sbc (v); }
    inline void slo (uint16_t a) { uint8_t v = asl (rd (a)); wr (a, v); A = setNZ (A | v); }
    inline void rla (uint16_t a) { uint8_t v = rol (rd (a)); wr (a, v); A = setNZ (A & v); }
    inline void sre (uint16_t a) { uint8_t v = lsr (rd (a)); wr (a, v); A = setNZ (A ^ v); }
    inline void rra (uint16_t a) { uint8_t v = ror (rd (a)); wr (a, v); adc (v); }
};
