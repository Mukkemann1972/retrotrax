// Generierte Fassung von siddefs-fp.h.in fuer RetroTrax (sonst per autoconf erzeugt).
// reSIDfp-Motor von Dag Lem / Leandro Nini, GPL-2.0+ (passt zu RetroTrax GPL-3.0).

#ifndef SIDDEFS_FP_H
#define SIDDEFS_FP_H

// Compilation configuration.
#define RESID_BRANCH_HINTS 1

// Compiler specifics: __builtin_expect gibt es nur bei GCC/Clang, nicht bei MSVC.
#if defined(__GNUC__) || defined(__clang__)
#  define HAVE_BUILTIN_EXPECT 1
#else
#  define HAVE_BUILTIN_EXPECT 0
#endif

// Branch prediction macros, lifted off the Linux kernel.
#if RESID_BRANCH_HINTS && HAVE_BUILTIN_EXPECT
#  define likely(x)      __builtin_expect(!!(x), 1)
#  define unlikely(x)    __builtin_expect(!!(x), 0)
#else
#  define likely(x)      (x)
#  define unlikely(x)    (x)
#endif

namespace reSIDfp {

typedef enum { MOS6581=1, MOS8580 } ChipModel;

typedef enum { AVERAGE=1, WEAK, STRONG } CombinedWaveforms;

typedef enum { DECIMATE=1, RESAMPLE } SamplingMethod;
}

extern "C"
{
#ifndef __VERSION_CC__
extern const char* residfp_version_string;
#else
const char* residfp_version_string = "2.14.0-retrotrax";
#endif
}

// Inlining on/off.
#define RESID_INLINING 1
#define RESID_INLINE inline

#endif // SIDDEFS_FP_H
