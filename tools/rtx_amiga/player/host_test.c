/* host_test.c - Host-natives Verifikationswerkzeug fuer mod_core.h (rtx_amiga
 * Phase 2b), nach dem etablierten Repo-Muster ("erst nativ mit g++ pruefen,
 * bevor der eigentliche Zielcode - hier: 68k - vertraut wird", s.
 * tools/rtx_wasm/native_test.cpp).
 *
 * Baut EINEN Software-Paula-Kanalmixer (nicht-interpoliert, wie die echte
 * Paula-DAC) um denselben mod_core.h-Playroutine-Kern, den auch der 68k-
 * Player (paula_backend.c) nutzt - spielt ein echtes .mod EINMAL komplett
 * durch (bis die Order-Liste einmal umlaeuft, mcInit/loopCount) und schreibt
 * das Ergebnis als WAV. Kein AmigaOS/68k noetig, reines ANSI C + gcc.
 *
 * Bauen:
 *   gcc -std=c89 -O2 -o build/rtx_amiga_host_test tools/rtx_amiga/player/host_test.c -lm
 * Nutzung:
 *   build/rtx_amiga_host_test song.mod out.wav
 */
#include "mod_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define OUT_RATE  44100
#define PAL_CLOCK 3546895.0  /* Amiga-Paula-Audiotakt PAL - Ausgaberate = Takt/Periode */

typedef struct {
    const MC_SBYTE *data;
    MC_ULONG lenBytes, loopStartBytes, loopLenBytes;
    double posBytes;
    double incPerSample;   /* Bytes pro Ausgabe-Sample bei aktueller Periode */
    double volumeGain;     /* 0..1, aus 0..64 */
    int active;
} HostChan;

static HostChan g_chan[MC_CHANNELS];
static short   *g_buf = 0;
static long     g_bufLen = 0, g_bufCap = 0;

static void hostEnsureCap (long extra)
{
    if (g_bufLen + extra <= g_bufCap) return;
    g_bufCap = (g_bufLen + extra) * 2 + 4096;
    g_buf = (short *) realloc (g_buf, (size_t) g_bufCap * sizeof (short));
}

static void hostTrigger (int ch, const MC_SBYTE *data, MC_ULONG lenBytes,
                         MC_ULONG loopStartBytes, MC_ULONG loopLenBytes,
                         MC_UWORD period, MC_UBYTE volume)
{
    HostChan *c = &g_chan[ch];
    c->data           = data;
    c->lenBytes       = lenBytes;
    c->loopStartBytes = loopStartBytes;
    c->loopLenBytes   = loopLenBytes;
    c->posBytes       = 0.0;
    c->incPerSample   = (period > 0) ? (PAL_CLOCK / (double) period) / (double) OUT_RATE : 0.0;
    c->volumeGain     = (double) volume / 64.0;
    c->active         = (lenBytes > 0 && period > 0) ? 1 : 0;
}

static void hostSetVolume (int ch, MC_UBYTE volume)
{
    g_chan[ch].volumeGain = (double) volume / 64.0;
}

/* Mischt `frames` Ausgabe-Samples (mono, dann als Stereo dupliziert) der 4
 * Kanaele zusammen und haengt sie an den Ausgabepuffer an. */
static void hostRenderFrames (long frames)
{
    long i;
    int ch;
    hostEnsureCap (frames * 2);
    for (i = 0; i < frames; i++) {
        double mix = 0.0;
        for (ch = 0; ch < MC_CHANNELS; ch++) {
            HostChan *c = &g_chan[ch];
            if (!c->active) continue;
            {
                MC_ULONG idx = (MC_ULONG) c->posBytes;
                if (idx >= c->lenBytes) {
                    if (c->loopLenBytes > 2) {
                        MC_ULONG span = c->lenBytes - c->loopStartBytes;
                        MC_ULONG rel  = (idx - c->loopStartBytes);
                        if (span == 0) span = c->loopLenBytes;
                        rel = c->loopStartBytes + (rel % span);
                        idx = rel;
                        c->posBytes = (double) idx;
                    } else {
                        c->active = 0;
                        continue;
                    }
                }
                mix += ((double) c->data[idx] / 128.0) * c->volumeGain;
                c->posBytes += c->incPerSample;
            }
        }
        if (mix > 1.0) mix = 1.0;
        if (mix < -1.0) mix = -1.0;
        {
            short s = (short) (mix * 32000.0);
            g_buf[g_bufLen++] = s;
            g_buf[g_bufLen++] = s;
        }
    }
}

/* ModHw::waitTick fuer den Host: rendert die Audiomenge, die diesem Tick bei
 * der aktuellen Tempo (pl->bpm) entspricht - klassische Formel
 * Tick-Dauer[s] = 2.5 / BPM (Standard 125 BPM -> 20 ms/Tick). */
static void hostWaitTick (const ModPlayer *pl)
{
    double seconds = 2.5 / (double) pl->bpm;
    long frames = (long) (seconds * OUT_RATE + 0.5);
    hostRenderFrames (frames);
}

static void writeWav (const char *path)
{
    FILE *f = fopen (path, "wb");
    long dataBytes = g_bufLen * (long) sizeof (short);
    long riffLen = 36 + dataBytes;
    if (!f) { fprintf (stderr, "kann %s nicht schreiben\n", path); exit (1); }

    fwrite ("RIFF", 1, 4, f);
    fwrite (&riffLen, 4, 1, f);
    fwrite ("WAVEfmt ", 1, 8, f);
    { long sz = 16; fwrite (&sz, 4, 1, f); }
    { short fmt = 1, ch = 2; long sr = OUT_RATE, br = OUT_RATE * 2 * 2; short ba = 4, bits = 16;
      fwrite (&fmt, 2, 1, f); fwrite (&ch, 2, 1, f); fwrite (&sr, 4, 1, f);
      fwrite (&br, 4, 1, f); fwrite (&ba, 2, 1, f); fwrite (&bits, 2, 1, f); }
    fwrite ("data", 1, 4, f);
    fwrite (&dataBytes, 4, 1, f);
    fwrite (g_buf, sizeof (short), (size_t) g_bufLen, f);
    fclose (f);
}

int main (int argc, char **argv)
{
    FILE *f;
    long size;
    MC_UBYTE *mod;
    ModPlayer pl;
    ModHw hw;
    long ticks = 0;

    if (argc < 3) {
        fprintf (stderr, "Nutzung: %s song.mod out.wav\n", argv[0]);
        return 1;
    }
    f = fopen (argv[1], "rb");
    if (!f) { fprintf (stderr, "kann %s nicht lesen\n", argv[1]); return 1; }
    fseek (f, 0, SEEK_END); size = ftell (f); fseek (f, 0, SEEK_SET);
    mod = (MC_UBYTE *) malloc ((size_t) size);
    if (fread (mod, 1, (size_t) size, f) != (size_t) size) { fprintf (stderr, "Lesefehler\n"); return 1; }
    fclose (f);

    if (memcmp (mod + MC_SIG_OFF, "M.K.", 4) != 0) {
        fprintf (stderr, "WARNUNG: Kennung bei %d ist nicht 'M.K.' (%.4s) - nur 4-Kanal-MOD unterstuetzt.\n",
                 MC_SIG_OFF, mod + MC_SIG_OFF);
    }

    memset (g_chan, 0, sizeof (g_chan));
    hw.trigger   = hostTrigger;
    hw.setVolume = hostSetVolume;
    hw.waitTick  = hostWaitTick;

    mcInit (&pl, mod);
    printf ("Song: %d Pattern(e), Songlaenge %d, Order[0]=%d\n",
            pl.numPatterns, pl.songLen, mod[MC_ORDER_OFF]);

    while (pl.loopCount < 1 && ticks < 2000000L) {   /* Sicherheitsnetz wie beim WASM-Renderfull */
        mcStep (&pl, &hw);
        ticks++;
    }
    printf ("Fertig nach %ld Ticks, %ld Ausgabe-Frames (%.2f s), finaler Speed=%d Tempo=%d\n",
            ticks, g_bufLen / 2, (double) (g_bufLen / 2) / OUT_RATE, pl.ticksPerRow, pl.bpm);

    writeWav (argv[2]);
    printf ("Geschrieben: %s\n", argv[2]);
    return 0;
}
