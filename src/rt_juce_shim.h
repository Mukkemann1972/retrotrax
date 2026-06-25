#pragma once
//
// rt_juce_shim.h - winziger JUCE-Ersatz fuer den eigenstaendigen Replayer.
//
// Wird NUR verwendet, wenn RETROTRAX_NO_JUCE definiert ist (Build der schlanken
// libretrotrax / des CLI / WASM). Dann ersetzt dieser Header die wenigen
// JUCE-Bausteine, die die Klang-Engine (TrackerEngine.h + SidChip.h) tatsaechlich
// nutzt - ohne das ganze JUCE-Framework. Im normalen Plugin-Build ist
// RETROTRAX_NO_JUCE NICHT gesetzt und das echte JUCE wird eingebunden.
//
// Bewusst minimal: nur das, was die Engine wirklich anfasst (per grep ermittelt):
//   jlimit/jmax/jmin, MathConstants, AudioBuffer<float>, String, CriticalSection
//   + ScopedLock, der uint-Typ. (File/MemoryBlock/SpinLock braucht nur der
//   TFMX-Player - kommt in Phase 1b dazu.)

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace juce
{
    using uint   = unsigned int;
    using uint8  = std::uint8_t;
    using uint16 = std::uint16_t;
    using uint32 = std::uint32_t;
    using uint64 = std::uint64_t;
    using int8   = std::int8_t;
    using int16  = std::int16_t;
    using int32  = std::int32_t;
    using int64  = std::int64_t;

    // --- Zahlen-Helfer (gleiche Semantik wie JUCE) ---------------------------
    template <typename T> inline T jmax (T a, T b) { return a > b ? a : b; }
    template <typename T> inline T jmin (T a, T b) { return a < b ? a : b; }
    template <typename T, typename... Rest> inline T jmax (T a, T b, Rest... r) { return jmax (a, jmax (b, r...)); }
    template <typename T, typename... Rest> inline T jmin (T a, T b, Rest... r) { return jmin (a, jmin (b, r...)); }

    // jlimit(lowerBound, upperBound, value) -> auf [lo,hi] begrenzen
    template <typename T> inline T jlimit (T lo, T hi, T v)
    {
        return v < lo ? lo : (v > hi ? hi : v);
    }

    // --- Mathe-Konstanten ----------------------------------------------------
    template <typename T> struct MathConstants
    {
        static constexpr T pi       = static_cast<T> (3.141592653589793238L);
        static constexpr T twoPi    = static_cast<T> (2.0L * 3.141592653589793238L);
        static constexpr T halfPi   = static_cast<T> (0.5L * 3.141592653589793238L);
        static constexpr T euler    = static_cast<T> (2.718281828459045235L);
        static constexpr T sqrt2    = static_cast<T> (1.414213562373095049L);
    };

    // --- String: duenne Huelle um std::string -------------------------------
    // Reicht fuer Instrument-/Sample-Namen + einfache Verkettung/Zahl->Text.
    class String
    {
    public:
        String() = default;
        String (const char* s) : s_ (s ? s : "") {}
        String (const std::string& s) : s_ (s) {}
        String (int v)    : s_ (std::to_string (v)) {}
        String (double v) : s_ (std::to_string (v)) {}

        String& operator= (const char* s) { s_ = s ? s : ""; return *this; }

        String operator+ (const String& o) const { return String (s_ + o.s_); }
        String& operator+= (const String& o) { s_ += o.s_; return *this; }

        bool operator== (const String& o) const { return s_ == o.s_; }
        bool operator!= (const String& o) const { return s_ != o.s_; }
        bool isEmpty() const { return s_.empty(); }
        bool isNotEmpty() const { return ! s_.empty(); }

        const char* toRawUTF8() const { return s_.c_str(); }
        const std::string& toStdString() const { return s_; }
        operator std::string() const { return s_; }

    private:
        std::string s_;
    };

    inline String operator+ (const char* a, const String& b) { return String (a) + b; }

    // --- Sperren -------------------------------------------------------------
    // CriticalSection == nicht-rekursiver Mutex; ScopedLock == lock_guard.
    class CriticalSection
    {
    public:
        void enter() const { m_.lock(); }
        void exit()  const { m_.unlock(); }
    private:
        mutable std::mutex m_;
        friend class ScopedLock;
    };

    class ScopedLock
    {
    public:
        explicit ScopedLock (const CriticalSection& cs) : cs_ (cs) { cs_.enter(); }
        ~ScopedLock() { cs_.exit(); }
        ScopedLock (const ScopedLock&) = delete;
        ScopedLock& operator= (const ScopedLock&) = delete;
    private:
        const CriticalSection& cs_;
    };

    // --- AudioBuffer<float>: nur die genutzten Methoden ----------------------
    template <typename SampleType>
    class AudioBuffer
    {
    public:
        AudioBuffer() = default;
        AudioBuffer (int numChannels, int numSamples) { setSize (numChannels, numSamples); }

        void setSize (int numChannels, int numSamples)
        {
            channels_ = jmax (0, numChannels);
            samples_  = jmax (0, numSamples);
            data_.assign ((size_t) channels_, std::vector<SampleType> ((size_t) samples_, (SampleType) 0));
            refreshPointers();
        }

        int getNumChannels() const { return channels_; }
        int getNumSamples()  const { return samples_; }

        SampleType*       getWritePointer (int ch)       { return data_[(size_t) ch].data(); }
        const SampleType* getReadPointer  (int ch) const { return data_[(size_t) ch].data(); }

        void clear()
        {
            for (auto& ch : data_)
                std::fill (ch.begin(), ch.end(), (SampleType) 0);
        }

        void addSample (int ch, int index, SampleType value)
        {
            data_[(size_t) ch][(size_t) index] += value;
        }

        void setSample (int ch, int index, SampleType value)
        {
            data_[(size_t) ch][(size_t) index] = value;
        }

        SampleType getSample (int ch, int index) const
        {
            return data_[(size_t) ch][(size_t) index];
        }

    private:
        void refreshPointers() {} // Zeiger kommen direkt aus den Vektoren
        int channels_ = 0, samples_ = 0;
        std::vector<std::vector<SampleType>> data_;
    };
}
