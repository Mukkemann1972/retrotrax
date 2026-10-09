// Haertetest fuer die Importer: kleine, absichtlich kaputte Dateien duerfen weder Gigabytes
// reservieren noch lange haengen. Bauen mit: cmake -B build -DRETROTRAX_BUILD_TESTS=ON
// Starten: build/RetroTraxImportTests_artefacts/Release/RetroTraxImportTests.exe  (Exit 0 = alles gut)
#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include "../../src/ImportCommon.h"
#include "../../src/ItImport.h"
#include "../../src/S3mImport.h"
#include "../../src/Soundfont2.h"
#include "../../src/IFF8SVXFormat.h"

static int failures = 0;

static void check (bool ok, const juce::String& name)
{
    std::cout << (ok ? "OK    " : "FEHLER") << "  " << name << std::endl;
    if (! ok) ++failures;
}

static void put16 (juce::MemoryBlock& b, size_t off, juce::uint16 v) { b[off] = (char) (v & 255); b[off + 1] = (char) (v >> 8); }
static void put32be (juce::MemoryBlock& b, size_t off, juce::uint32 v)
{
    b[off] = (char) (v >> 24); b[off + 1] = (char) (v >> 16); b[off + 2] = (char) (v >> 8); b[off + 3] = (char) v;
}
static void put32le (juce::MemoryBlock& b, size_t off, juce::uint32 v)
{
    b[off] = (char) v; b[off + 1] = (char) (v >> 8); b[off + 2] = (char) (v >> 16); b[off + 3] = (char) (v >> 24);
}
static void putText (juce::MemoryBlock& b, size_t off, const char* t) { for (int i = 0; t[i] != 0; ++i) b[off + (size_t) i] = t[i]; }

int main()
{
    const auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("rtx_haertetest");
    tmp.createDirectory();

    // 1) IT mit 65535 Patterns (Zeigertabelle ~256 KB, davor haette jeder Eintrag 64x64 Zellen vorab reserviert).
    {
        const size_t size = 192 + 65535 * 4;
        juce::MemoryBlock b (size, true);
        putText (b, 0, "IMPM");
        put16 (b, 38, 65535); // patNum
        const auto f = tmp.getChildFile ("viele_patterns.it");
        f.replaceWithData (b.getData(), b.getSize());
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        const auto song = ItImport::parse (f);
        const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
        check (! song.ok && song.message.containsIgnoreCase ("zu viele"), "IT: 65535 Patterns wird abgelehnt (" + juce::String (song.message) + ")");
        check (ms < 1000.0, "IT: Ablehnung dauert unter 1 s (" + juce::String (ms, 1) + " ms)");
    }

    // 2) S3M mit 65535 Patterns.
    {
        const size_t size = 96 + 65535 * 2;
        juce::MemoryBlock b (size, true);
        putText (b, 44, "SCRM");
        put16 (b, 36, 65535); // patNum
        const auto f = tmp.getChildFile ("viele_patterns.s3m");
        f.replaceWithData (b.getData(), b.getSize());
        const auto song = S3mImport::parse (f);
        check (! song.ok && song.message.containsIgnoreCase ("zu viele"), "S3M: 65535 Patterns wird abgelehnt (" + juce::String (song.message) + ")");
    }

    // 3) SF2 mit riesiger shdr-Groesse und fast keinen Daten.
    {
        juce::MemoryBlock b (4 + 4 + 4 + 4 + 4 + 4 + 4 + 64, true);
        size_t o = 0;
        putText (b, o, "RIFF"); o += 4; put32le (b, o, 0xFFFFFFF0u); o += 4;
        putText (b, o, "sfbk"); o += 4;
        putText (b, o, "LIST"); o += 4; put32le (b, o, 0xFFFFFFE0u); o += 4;
        putText (b, o, "pdta"); o += 4;
        putText (b, o, "shdr"); o += 4; put32le (b, o, 0xFFFFFFC0u);
        const auto f = tmp.getChildFile ("riesig.sf2");
        f.replaceWithData (b.getData(), b.getSize());
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        const auto bank = sf2::readBank (f);
        const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
        check (! bank.valid, "SF2: kaputte Bank ist nicht gueltig");
        check (ms < 1000.0, "SF2: riesige shdr-Groesse haengt nicht (" + juce::String (ms, 1) + " ms)");
    }

    // 4) 8SVX mit BODY-Groesse ~2 GiB und nur 16 Datenbytes.
    {
        juce::MemoryBlock b (12 + 8 + 20 + 8 + 16, true);
        size_t o = 0;
        putText (b, o, "FORM"); o += 4; put32be (b, o, 0x7FFFFFF0u); o += 4;
        putText (b, o, "8SVX"); o += 4;
        putText (b, o, "VHDR"); o += 4; put32be (b, o, 20); o += 4 + 20;
        putText (b, o, "BODY"); o += 4; put32be (b, o, 0x7FFFFFF0u);
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        auto* in = new juce::MemoryInputStream (b.getData(), b.getSize(), true);
        IFF8SVXAudioFormat fmt;
        std::unique_ptr<juce::AudioFormatReader> r (fmt.createReaderFor (in, true));
        const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
        check (ms < 1000.0, "8SVX: riesige BODY-Groesse reserviert nichts Riesiges (" + juce::String (ms, 1) + " ms)");
        check (r == nullptr || r->lengthInSamples <= 16, "8SVX: es werden hoechstens die vorhandenen 16 Bytes uebernommen");
    }

    tmp.deleteRecursively();
    std::cout << (failures == 0 ? "ALLE TESTS BESTANDEN" : "FEHLGESCHLAGEN") << std::endl;
    return failures == 0 ? 0 : 1;
}
