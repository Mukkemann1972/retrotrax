/* rtx_amiga Phase 2b - der eigentliche 4-Kanal-68k-Pattern-Player.
 *
 * Normales AmigaOS-CLI-Programm (wie Phase 1s tone_test.c: kein roher
 * Bootblock - der haengte sich beim AROS-Boot in FS-UAE auf, s.
 * tools/rtx_amiga/README.md), das direkt Paula-Register anspricht und dabei
 * ganz auf mod_core.h aufsetzt (derselbe Kern, den host_test.c schon nativ
 * gegen echte Songs geprueft hat - hier nur mit einem echten Hardware-
 * Backend statt einem Software-Mixer).
 *
 * Der Song steckt eingebettet in song_data.h (per mod2c.py aus einer echten
 * .mod-Datei erzeugt) - kein AmigaDOS-Dateizugriff noetig.
 *
 * WICHTIGE HARDWARE-DETAILS (warum es nicht nur "Register beschreiben" ist):
 *  - Ein Paula-Kanal, dessen DMA schon LAEUFT, uebernimmt ein neues
 *    AUDxLC/AUDxLEN NICHT sofort (Paula ist intern doppelt gepuffert und
 *    laedt neue Werte erst beim naechsten natuerlichen Puffer-Ende). Der
 *    Standardtrick zum sofortigen Neuanschlagen einer Note: DMA fuer diesen
 *    Kanal kurz AUS, neue Werte schreiben, DMA wieder AN - das erzwingt den
 *    Neustart mit den neuen Werten (hwTrigger unten).
 *  - Sample-Loop: Paula wiederholt beim Erreichen des Puffer-Endes
 *    automatisch DENSELBEN Puffer (AUDxLC/AUDxLEN unveraendert), bis wer
 *    anders sie umschreibt - kein Interrupt/Nachladen noetig, SOLANGE die
 *    Loop-Region der GANZE Sample ist. Das ist bei allen aus RetroTrax
 *    eingefrorenen Synth-Instrumenten der Fall (rt_freeze.h liefert immer
 *    loopStart=0, loopLen=ganzer Sample). Ein echtes MOD mit Attack+kurzem
 *    Sustain-Loop (Loop-Region kuerzer als der ganze Sample) wuerde damit
 *    NICHT korrekt loopen (es wiederholt den ganzen Sample inkl. Attack statt
 *    nur die Loop-Region) - fehlt noch (braucht den AUDxIP-Interrupt fuers
 *    Nachladen), s. README.
 *  - Tempo (0xF-Wert >= 32, "BPM"): die Tick-Dauer wird in ganzen VBlanks
 *    (50 Hz PAL) angenaehert, nicht per CIA-Timer-Interrupt - bei genau
 *    Standard-Tempo 125 trifft das exakt (125 BPM = 20 ms/Tick = 1 VBlank),
 *    bei anderen Tempi wird auf- oder abgerundet (siehe hwWaitTick unten und
 *    README-Einschraenkungen).
 *  - VBlank-Erkennung per Raster-Zeilen-Polling (VHPOSR), NICHT per
 *    INTREQR/INTREQ - das Flag dort teilt sich der laufende Multitasking-
 *    Amiga-OS mit seinem eigenen VBlank-Interrupt-Handler (Maus-Polling,
 *    Timer), es leerzulesen waere eine Race gegen das OS. Reines Lesen von
 *    VHPOSR stoert niemanden.
 */

typedef unsigned char  UBYTE;
typedef signed char    SBYTE;
typedef unsigned short UWORD;
typedef unsigned long  ULONG;
typedef volatile UWORD *REG16;
typedef volatile ULONG *REG32;

#define DMACON  (*(REG16)0xdff096)
#define VHPOSR  (*(REG16)0xdff006)
#define AUD_BASE 0xdff0a0UL   /* AUD0LC; Kanal ch beginnt bei AUD_BASE + ch*0x10 */

#include "mod_core.h"
#include "song_data.h"

/* Ein Frame (eine volle PAL-Bildwiederholung, ~20 ms) abwarten - ohne
 * Interrupts, reines Polling der Raster-Zeile (s. Erklaerung oben). */
static void waitFrame (void)
{
    while ((VHPOSR >> 8) != 0) { }
    while ((VHPOSR >> 8) == 0) { }
}

static void hwTrigger (int ch, const MC_SBYTE *data, MC_ULONG lenBytes,
                       MC_ULONG loopStartBytes, MC_ULONG loopLenBytes,
                       MC_UWORD period, MC_UBYTE volume)
{
    const ULONG base = AUD_BASE + (ULONG) ch * 0x10UL;
    REG32 lc  = (REG32) (base + 0x0);
    REG16 len = (REG16) (base + 0x4);
    REG16 per = (REG16) (base + 0x6);
    REG16 vol = (REG16) (base + 0x8);
    const UWORD chBit = (UWORD) (1U << ch);
    /* loopStartBytes/loopLenBytes bewusst ungenutzt - s. Kommentar oben:
       Sustain-Loop-Nachladen (AUDxIP-Interrupt) fehlt noch. */

    DMACON = chBit;                 /* CLR: Kanal-DMA kurz aus, damit ... */
    *lc  = (ULONG) (void *) data;
    *len = (UWORD) (lenBytes / 2);  /* Laenge in WORDS */
    *per = period;
    *vol = volume;
    DMACON = (UWORD) (0x8000U | chBit);   /* SET: ... die neuen Werte sofort uebernommen werden */
}

static void hwSetVolume (int ch, MC_UBYTE volume)
{
    const ULONG base = AUD_BASE + (ULONG) ch * 0x10UL;
    REG16 vol = (REG16) (base + 0x8);
    *vol = volume;                  /* Lautstaerke darf man jederzeit live aendern, kein DMA-Toggle noetig */
}

/* Tick-Dauer in ganzen VBlanks (50 Hz PAL) annaehern: Standard-Tempo 125 BPM
 * ergibt exakt 20 ms/Tick = 1 VBlank. round(125/bpm), mindestens 1 (unser
 * Polling kann nicht schneller als ein Frame ticken - bei sehr hohem BPM
 * laeuft der Song dadurch langsamer als am PC/Web-Player, s. README). */
static void hwWaitTick (const ModPlayer *pl)
{
    int n = (125 + pl->bpm / 2) / pl->bpm;
    if (n < 1) n = 1;
    while (n-- > 0) waitFrame();
}

int main (void)
{
    ModPlayer pl;
    ModHw hw;

    hw.trigger   = hwTrigger;
    hw.setVolume = hwSetVolume;
    hw.waitTick  = hwWaitTick;

    mcInit (&pl, songData);

    DMACON = (UWORD) 0x820FU;   /* SET + DMAEN(9) + AUD0..3EN(0-3): Master-DMA + alle 4 Kanaele an */

    for ( ; ; )
        mcStep (&pl, &hw);

    /* unerreichbar (Endlos-Player wie ein echtes Amiga-Hintergrundmusikstueck) */
}
