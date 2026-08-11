/* rtx_amiga Phase 1 - normales AmigaOS-CLI-Programm, das Paula-Kanal 0 direkt
 * ansteuert und ein eingebettetes 8-bit-Testsample spielt. Laeuft (anders als
 * der reine Bootblock-Versuch) ueber ein normales AmigaDOS-Startup-Sequence-
 * Boot, weil AROS' interne ROM-Ersatzvariante in FS-UAE beim rohen Custom-
 * Bootblock haengenblieb (dosboot.resource kam nie zum eigentlichen Disk-Read).
 */

typedef unsigned char UBYTE;
typedef unsigned short UWORD;
typedef unsigned long ULONG;
typedef volatile UWORD *REG16;
typedef volatile ULONG *REG32;

#define DMACON  (*(REG16)0xdff096)
#define AUD0LC  (*(REG32)0xdff0a0)
#define AUD0LEN (*(REG16)0xdff0a4)
#define AUD0PER (*(REG16)0xdff0a6)
#define AUD0VOL (*(REG16)0xdff0a8)

static const signed char wave[32] = {
    100,100,100,100,100,100,100,100,
    100,100,100,100,100,100,100,100,
   -100,-100,-100,-100,-100,-100,-100,-100,
   -100,-100,-100,-100,-100,-100,-100,-100
};

int main(void) {
    ULONG i;
    AUD0LC  = (ULONG)(void *)wave;
    AUD0LEN = sizeof(wave) / 2;   /* Laenge in WORDS */
    AUD0PER = 400;
    AUD0VOL = 64;
    DMACON  = 0x8201;             /* SET + DMAEN + AUD0EN */

    /* ein paar Sekunden hoerbar halten, dann DMA abschalten und normal
       zurueckkehren (CLI kann danach weiterlaufen) */
    for (i = 0; i < 40000000UL; i++) { }
    DMACON = 0x0001;              /* AUD0EN loeschen (SET-Bit=0 -> CLR) */
    return 0;
}
