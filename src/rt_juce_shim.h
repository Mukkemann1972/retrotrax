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
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>
#include <sys/stat.h>
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

        bool startsWithIgnoreCase (const String& o) const
        {
            if (o.s_.size() > s_.size()) return false;
            for (size_t i = 0; i < o.s_.size(); ++i)
                if (std::tolower ((unsigned char) s_[i]) != std::tolower ((unsigned char) o.s_[i]))
                    return false;
            return true;
        }
        bool equalsIgnoreCase (const String& o) const
        {
            if (s_.size() != o.s_.size()) return false;
            for (size_t i = 0; i < s_.size(); ++i)
                if (std::tolower ((unsigned char) s_[i]) != std::tolower ((unsigned char) o.s_[i]))
                    return false;
            return true;
        }
        String substring (int start) const
        {
            if (start < 0) start = 0;
            return String (start < (int) s_.size() ? s_.substr ((size_t) start) : std::string());
        }
        // substring(start, end) - end ist exklusiv, wie bei JUCE; beide Grenzen
        // werden auf die Laenge geklemmt, ein leeres Stueck ist erlaubt.
        String substring (int start, int end) const
        {
            const int len = (int) s_.size();
            if (start < 0) start = 0;
            if (end > len) end = len;
            if (start >= len || end <= start) return String();
            return String (s_.substr ((size_t) start, (size_t) (end - start)));
        }
        // true, wenn JEDES Zeichen in 'chars' vorkommt (leerer Text -> true).
        bool containsOnly (const String& chars) const
        {
            for (char c : s_)
                if (chars.s_.find (c) == std::string::npos)
                    return false;
            return true;
        }
        // Fuehrende Ganzzahl lesen (wie JUCE: kein Treffer -> 0).
        int getIntValue() const
        {
            try { return std::stoi (s_); } catch (...) { return 0; }
        }
        String trim() const
        {
            const char* ws = " \t\r\n\f\v";
            const auto a = s_.find_first_not_of (ws);
            if (a == std::string::npos) return String();
            const auto b = s_.find_last_not_of (ws);
            return String (s_.substr (a, b - a + 1));
        }
        // Aus einem NICHT nullterminierten Puffer fester Laenge; ein enthaltenes
        // \0 beendet den Text (MOD-Namen sind so aufgefuellt).
        static String fromUTF8 (const char* p, int len)
        {
            if (p == nullptr || len <= 0) return String();
            int n = 0;
            while (n < len && p[n] != '\0') ++n;
            return String (std::string (p, (size_t) n));
        }

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

    // --- SpinLock: nicht-blockierende Sperre (fuer den TFMX-Player) ----------
    // Der Audio-Thread nimmt sie nur per TryLock (ScopedTryLockType) -> bei
    // Konflikt Stille statt Warten. Gleiche Semantik wie juce::SpinLock.
    class SpinLock
    {
    public:
        void enter() const { while (flag_.test_and_set (std::memory_order_acquire)) {} }
        bool tryEnter() const { return ! flag_.test_and_set (std::memory_order_acquire); }
        void exit() const { flag_.clear (std::memory_order_release); }

        class ScopedLockType
        {
        public:
            explicit ScopedLockType (const SpinLock& l) : l_ (l) { l_.enter(); }
            ~ScopedLockType() { l_.exit(); }
            ScopedLockType (const ScopedLockType&) = delete;
            ScopedLockType& operator= (const ScopedLockType&) = delete;
        private:
            const SpinLock& l_;
        };

        class ScopedTryLockType
        {
        public:
            explicit ScopedTryLockType (const SpinLock& l) : l_ (l), locked_ (l.tryEnter()) {}
            ~ScopedTryLockType() { if (locked_) l_.exit(); }
            bool isLocked() const { return locked_; }
            ScopedTryLockType (const ScopedTryLockType&) = delete;
            ScopedTryLockType& operator= (const ScopedTryLockType&) = delete;
        private:
            const SpinLock& l_;
            bool locked_;
        };

    private:
        mutable std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
    };

    // --- MemoryBlock: roher Byte-Puffer -------------------------------------
    class MemoryBlock
    {
    public:
        void*       getData()       { return bytes_.empty() ? nullptr : bytes_.data(); }
        const void* getData() const { return bytes_.empty() ? nullptr : bytes_.data(); }
        size_t      getSize() const { return bytes_.size(); }
        std::vector<uint8_t> bytes_; // direkt beschreibbar (nur im schlanken Build)
    };

    // --- File: nur was der TFMX-Player anfasst (Lesen + Namens-Teile) --------
    class File
    {
    public:
        File() = default;
        File (const char* path)        : path_ (path ? path : "") {}
        File (const std::string& path) : path_ (path) {}
        File (const String& path)      : path_ (path.toStdString()) {}

        bool existsAsFile() const
        {
            struct stat st;
            return ! path_.empty() && ::stat (path_.c_str(), &st) == 0 && S_ISREG (st.st_mode);
        }

        bool loadFileAsData (MemoryBlock& mb) const
        {
            std::ifstream f (path_, std::ios::binary | std::ios::ate);
            if (! f.good()) return false;
            const std::streamsize n = f.tellg();
            if (n < 0) return false;
            f.seekg (0);
            mb.bytes_.resize ((size_t) n);
            if (n > 0) f.read ((char*) mb.bytes_.data(), n);
            return (bool) f;
        }

        String getFullPathName() const { return String (path_); }

        String getFileName() const
        {
            const auto slash = path_.find_last_of ("/\\");
            return String (slash == std::string::npos ? path_ : path_.substr (slash + 1));
        }
        String getFileExtension() const // inkl. Punkt, wie JUCE (letzter Punkt)
        {
            const std::string fn = getFileName().toStdString();
            const auto dot = fn.find_last_of ('.');
            if (dot == std::string::npos || dot == 0) return String();
            return String (fn.substr (dot));
        }
        String getFileNameWithoutExtension() const
        {
            const std::string fn = getFileName().toStdString();
            const auto dot = fn.find_last_of ('.');
            if (dot == std::string::npos || dot == 0) return String (fn);
            return String (fn.substr (0, dot));
        }

    private:
        std::string path_;
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

        SampleType*       getWritePointer (int ch, int start)       { return data_[(size_t) ch].data() + start; }
        const SampleType* getReadPointer  (int ch, int start) const { return data_[(size_t) ch].data() + start; }

        void clear()
        {
            for (auto& ch : data_)
                std::fill (ch.begin(), ch.end(), (SampleType) 0);
        }

        void clear (int ch, int start, int num)
        {
            auto* p = data_[(size_t) ch].data();
            std::fill (p + start, p + start + num, (SampleType) 0);
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
