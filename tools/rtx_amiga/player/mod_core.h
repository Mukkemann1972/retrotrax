/* mod_core.h - rtx_amiga Phase 2b: der eigentliche 4-Kanal-Pattern-Player.
 *
 * Portable ANSI-C (kein AmigaOS, keine Hardware-Register hier drin) - liest
 * direkt aus einem klassischen 4-Kanal-31-Sample-ProTracker-MOD-Puffer (wie
 * ihn tools/rtx_amiga/export/rtx_to_mod.cpp erzeugt, aber genauso ein
 * beliebiges echtes Amiga-.MOD) und ruft bei jedem Tick ueber die ModHw-
 * Schnittstelle in ein Hardware-Backend. Zwei Backends nutzen genau diesen
 * Kern:
 *   - host_test.c: Software-Mixer fuer die Host-Verifikation (WAV-Datei,
 *     kein 68k noetig) - Muster wie native_test.cpp/rtx_wasm's "erst nativ
 *     pruefen, bevor der eigentliche Zielcode vertraut wird".
 *   - mod_player.c: echte Paula-Register (68k, AmigaOS-CLI-Programm wie
 *     Phase 1s tone_test.c).
 *
 * Format-Referenz: src/ModExport.h (Schreibseite) / src/ModImport.h
 * (JUCE-Leseseite) - dieselbe Bit-Kodierung, hier nur ohne JUCE/C++.
 *
 * Bewusste Einschraenkungen dieser ersten Stufe (siehe README):
 *  - Effekte: nur 0xC (Set-Volume), 0xB (Position-Jump), 0xD (Pattern-Break),
 *    0xF (Speed/Tempo) - Arpeggio/Slides/Vibrato/Sample-Offset fehlen noch.
 *  - Tempo (0xF-Wert >= 32, klassisches "BPM"): die Tick-Dauer wird in ganzen
 *    VBlanks (50 Hz PAL) angenaehert (gerundet, mindestens 1 VBlank/Tick -
 *    siehe hwWaitTick() in mod_player.c), nicht ueber einen CIA-Timer-
 *    Interrupt - bei Standard-Tempo 125 exakt, sonst gerundet und bei sehr
 *    hohem BPM zu langsam (Polling kann nicht schneller als ein Frame
 *    ticken). Ein CIA-Interrupt waere praeziser, braucht aber eigene
 *    Vector-Verwaltung (deutlich mehr System-Code) - bewusst zurueckgestellt.
 *  - Finetune wird ignoriert (die im Cell gespeicherte Periode ist bei
 *    Notenausloesung bereits das Ziel - Finetune wirkt in echtem ProTracker
 *    nur auf Folge-Effekte wie Slides/Arpeggio, die wir noch nicht haben).
 */
#ifndef RTX_AMIGA_MOD_CORE_H
#define RTX_AMIGA_MOD_CORE_H

typedef unsigned char  MC_UBYTE;
typedef signed char    MC_SBYTE;
typedef unsigned short MC_UWORD;
typedef unsigned long  MC_ULONG;

#define MC_CHANNELS        4
#define MC_ROWS            64
#define MC_SAMPLES         31
#define MC_SAMPLE_HDR_SIZE 30
#define MC_SAMPLES_OFF     20
#define MC_SONGLEN_OFF     (MC_SAMPLES_OFF + MC_SAMPLES * MC_SAMPLE_HDR_SIZE) /* 950 */
#define MC_RESTART_OFF     (MC_SONGLEN_OFF + 1)                              /* 951 */
#define MC_ORDER_OFF       (MC_RESTART_OFF + 1)                              /* 952 */
#define MC_ORDER_MAX       128
#define MC_SIG_OFF         (MC_ORDER_OFF + MC_ORDER_MAX)                     /* 1080 */
#define MC_PATTERN_OFF     (MC_SIG_OFF + 4)                                  /* 1084 */
#define MC_BYTES_PER_ROW   (MC_CHANNELS * 4)
#define MC_BYTES_PER_PAT   (MC_ROWS * MC_BYTES_PER_ROW)

typedef struct {
    MC_UWORD lenWords;
    MC_UWORD loopStartWords;
    MC_UWORD loopLenWords;
    MC_UBYTE volume;      /* 0..64 */
} ModSampleHdr;

typedef struct {
    MC_UBYTE sample;   /* 0 = kein Wechsel, sonst 1..31 */
    MC_UWORD period;   /* 0 = kein neuer Ton in dieser Zelle */
    MC_UBYTE effect;
    MC_UBYTE param;
} ModCell;

typedef struct {
    MC_UWORD period;
    MC_UBYTE sample;   /* zuletzt zugewiesenes Sample, 0 = keins */
    MC_UBYTE volume;
} ModChanState;

typedef struct ModPlayer ModPlayer;

/* Hardware-/Backend-Schnittstelle. `data` zeigt auf die 8-bit-signed-PCM-
 * Rohdaten IM MOD-PUFFER selbst - das Backend kopiert nicht, es nutzt den
 * Zeiger direkt (auf dem Amiga muss der MOD-Puffer darum in Chip-RAM liegen,
 * s. paula_backend.c). */
typedef struct {
    void (*trigger)   (int ch, const MC_SBYTE *data, MC_ULONG lenBytes,
                        MC_ULONG loopStartBytes, MC_ULONG loopLenBytes,
                        MC_UWORD period, MC_UBYTE volume);
    void (*setVolume) (int ch, MC_UBYTE volume);
    void (*waitTick)  (const ModPlayer *pl);   /* wartet die Dauer EINES Ticks */
} ModHw;

struct ModPlayer {
    const MC_UBYTE *mod;
    int numPatterns;
    int songLen;
    int orderPos;
    int row;
    int tick;
    int ticksPerRow;   /* "Speed", Standard 6 */
    int bpm;            /* "Tempo", Standard 125 (nur informativ/fuers Backend) */
    int loopCount;      /* zaehlt jedes Mal, wenn die Order-Liste von vorn beginnt */
    int pendingPosJump;  /* von mcProcessRow gesetzt (0xB), -1 = keiner; wird erst beim
                            naechsten Zeilenwechsel (letzter Tick der Zeile) in mcStep
                            angewendet - MUSS im Player-Zustand stehen (nicht lokal in
                            mcStep), weil der letzte Tick einer Zeile ein SPAETERER
                            mcStep()-Aufruf ist als der, der die Zeile gelesen hat. */
    int pendingPatBreak; /* von mcProcessRow gesetzt (0xD), -1 = keiner, sonst Zielzeile */
    ModChanState ch[MC_CHANNELS];
};

static MC_UWORD mcBe16 (const MC_UBYTE *p) { return (MC_UWORD) ((p[0] << 8) | p[1]); }

static void mcReadSampleHdr (const MC_UBYTE *mod, int idx1based, ModSampleHdr *out)
{
    const MC_UBYTE *h = mod + MC_SAMPLES_OFF + (idx1based - 1) * MC_SAMPLE_HDR_SIZE;
    out->lenWords       = mcBe16 (h + 22);
    out->volume          = h[25];
    out->loopStartWords = mcBe16 (h + 26);
    out->loopLenWords   = mcBe16 (h + 28);
}

/* Zeiger auf die PCM-Daten von Sample idx1based: alle Sample-Laengen davor
 * aufsummieren (Sample-Daten liegen direkt hinter den Pattern-Daten, in
 * Header-Reihenfolge 1..31 - klassisches MOD-Layout). */
static const MC_UBYTE* mcSampleDataPtr (const MC_UBYTE *mod, int numPatterns, int idx1based)
{
    const MC_UBYTE *p = mod + MC_PATTERN_OFF + (MC_ULONG) numPatterns * MC_BYTES_PER_PAT;
    int i;
    for (i = 1; i < idx1based; i++) {
        ModSampleHdr h;
        mcReadSampleHdr (mod, i, &h);
        p += (MC_ULONG) h.lenWords * 2;
    }
    return p;
}

static void mcReadCell (const MC_UBYTE *mod, int pattern, int row, int ch, ModCell *c)
{
    const MC_UBYTE *b = mod + MC_PATTERN_OFF + (MC_ULONG) pattern * MC_BYTES_PER_PAT
                       + row * MC_BYTES_PER_ROW + ch * 4;
    c->sample = (MC_UBYTE) ((b[0] & 0xF0) | (b[2] >> 4));
    c->period = (MC_UWORD) (((b[0] & 0x0F) << 8) | b[1]);
    c->effect = (MC_UBYTE) (b[2] & 0x0F);
    c->param  = b[3];
}

static void mcInit (ModPlayer *pl, const MC_UBYTE *mod)
{
    int i, maxPat = 0, len;
    const MC_UBYTE *order = mod + MC_ORDER_OFF;
    len = mod[MC_SONGLEN_OFF];
    if (len < 1) len = 1;
    if (len > MC_ORDER_MAX) len = MC_ORDER_MAX;
    for (i = 0; i < len; i++)
        if (order[i] > maxPat) maxPat = order[i];

    pl->mod         = mod;
    pl->numPatterns = maxPat + 1;
    pl->songLen     = len;
    pl->orderPos    = 0;
    pl->row         = 0;
    pl->tick        = 0;
    pl->ticksPerRow = 6;
    pl->bpm         = 125;
    pl->loopCount   = 0;
    pl->pendingPosJump  = -1;
    pl->pendingPatBreak = -1;
    for (i = 0; i < MC_CHANNELS; i++) {
        pl->ch[i].period = 0;
        pl->ch[i].sample = 0;
        pl->ch[i].volume = 0;
    }
}

/* Eine Zeile verarbeiten: Noten ausloesen, C/B/D/F-Effekte anwenden. 0xB/0xD
 * schreiben ihr Ziel nach pl->pendingPosJump/pendingPatBreak - angewendet
 * wird das erst beim Zeilenwechsel (letzter Tick der Zeile) in mcStep, das
 * ist ein SPAETERER mcStep()-Aufruf als dieser hier (darum im Player-Zustand,
 * nicht als lokale Variable). */
static void mcProcessRow (ModPlayer *pl, const ModHw *hw)
{
    int pattern = pl->mod[MC_ORDER_OFF + pl->orderPos];
    int c;
    pl->pendingPosJump  = -1;
    pl->pendingPatBreak = -1;

    for (c = 0; c < MC_CHANNELS; c++) {
        ModCell cell;
        mcReadCell (pl->mod, pattern, pl->row, c, &cell);

        if (cell.sample >= 1 && cell.sample <= MC_SAMPLES) {
            ModSampleHdr sh;
            mcReadSampleHdr (pl->mod, cell.sample, &sh);
            pl->ch[c].sample = cell.sample;
            pl->ch[c].volume = sh.volume;
        }
        if (cell.period > 0 && pl->ch[c].sample >= 1) {
            ModSampleHdr sh;
            const MC_UBYTE *base;
            mcReadSampleHdr (pl->mod, pl->ch[c].sample, &sh);
            base = mcSampleDataPtr (pl->mod, pl->numPatterns, pl->ch[c].sample);
            pl->ch[c].period = cell.period;
            hw->trigger (c, (const MC_SBYTE *) base, (MC_ULONG) sh.lenWords * 2,
                         (MC_ULONG) sh.loopStartWords * 2, (MC_ULONG) sh.loopLenWords * 2,
                         pl->ch[c].period, pl->ch[c].volume);
        }
        if (cell.effect == 0xC) {
            MC_UBYTE v = cell.param;
            if (v > 64) v = 64;
            pl->ch[c].volume = v;
            hw->setVolume (c, v);
        } else if (cell.effect == 0xB) {
            pl->pendingPosJump = cell.param;
        } else if (cell.effect == 0xD) {
            pl->pendingPatBreak = (cell.param >> 4) * 10 + (cell.param & 0x0F);
        } else if (cell.effect == 0xF && cell.param > 0) {
            if (cell.param < 32) pl->ticksPerRow = cell.param;
            else                 pl->bpm         = cell.param;
        }
    }
}

/* Einen Tick abarbeiten: bei Tick 0 die aktuelle Zeile spielen, danach beim
 * Backend die Tick-Dauer abwarten, dann Zeilen-/Order-Fortschritt. Ruft man
 * das endlos auf (68k-Player) oder bis loopCount>=N (Host-Test). */
static void mcStep (ModPlayer *pl, const ModHw *hw)
{
    if (pl->tick == 0)
        mcProcessRow (pl, hw);

    hw->waitTick (pl);

    pl->tick++;
    if (pl->tick >= pl->ticksPerRow) {
        int posJump = pl->pendingPosJump, patBreak = pl->pendingPatBreak;
        pl->tick = 0;
        if (posJump >= 0) {
            pl->orderPos = posJump;
            pl->row      = (patBreak >= 0) ? patBreak : 0;
        } else if (patBreak >= 0) {
            pl->orderPos++;
            pl->row = patBreak;
        } else {
            pl->row++;
            if (pl->row >= MC_ROWS) { pl->row = 0; pl->orderPos++; }
        }
        if (pl->orderPos >= pl->songLen) {
            MC_UBYTE restart = pl->mod[MC_RESTART_OFF];
            pl->orderPos = (restart < pl->songLen) ? restart : 0;
            pl->loopCount++;
        }
    }
}

#endif /* RTX_AMIGA_MOD_CORE_H */
