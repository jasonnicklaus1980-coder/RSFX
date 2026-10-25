// Algorithm table. Names and ordering come from the manuals (see docs/SOURCES.md):
//   ASR-10 Musician's Manual, Section 5 "ROM Effects List" pp.3-4 (ROM-01..ROM-50)
//   ASR-X Pro Reference Manual, ch.4 pp.41-42 (40 insert effects, 8 global reverbs)
// The DSP behind each name is an approximation built from generic blocks; see docs/SOURCES.md.
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

namespace rsfx {

enum Param { P_ALGO = 0, P_MIX, P_DECAY, P_DAMP, P_PRE, P_DELAY, P_FB, P_RATE, P_DEPTH, P_DRIVE,
             P_FREQ, P_RESO, P_THRESH, P_PITCH, P_BAL, P_OUT, P_BYPASS,
             // v1.1 additions, appended so v1.0 projects keep their parameter indices
             P_INPUT,                                  // input gain
             P_CHAIN,                                  // read-only: shows the signal chain as text
             P_INL, P_INR, P_OUTL, P_OUTR,             // read-only: peak meters (clip latches the top)
             P_LED_REV, P_LED_DLY, P_LED_MOD, P_LED_CHR,  // read-only: which skin modules the algorithm uses
             NPARAMS };
static const int kFirstReadOnly = P_CHAIN;
inline bool isReadOnly(int p) { return p >= kFirstReadOnly && p < NPARAMS; }

enum Stage : uint8_t { S_NONE = 0, S_REV, S_GATEDREV, S_REVERSE, S_NONLIN, S_DDL, S_MTAP, S_MODDLY,
    S_CHORUS, S_FLANGER, S_PHASER, S_VIBRATO, S_TREM, S_ROTARY, S_SPINNER, S_DIST, S_VCF, S_WAH,
    S_AMP, S_CAB, S_TUNSPKR, S_EQ, S_PEQ, S_COMP, S_GATE, S_EXP, S_INVEXP, S_KEYEXP, S_DEESS, S_DUCK,
    S_PITCH, S_DETUNE, S_FORMANT, S_CHATTER, S_RUMBLE, S_EXCITER, S_COUNT };

enum Model : uint8_t { M_A10 = 0, M_X = 1, M_XG = 2 };

struct ParamMeta { const char* name; const char* unit; float lo, hi; bool log; float def; };
static const ParamMeta kParams[NPARAMS] = {
    {"Algorithm", "",   0, 1, false, 0},
    {"Mix",      "%",   0, 100, false, 35},
    {"Decay",    "s",   0.1f, 30, true, 2},
    {"Damping",  "",    0, 99, false, 35},
    {"PreDelay", "ms",  0, 500, false, 20},
    {"Delay",    "ms",  1, 2000, false, 350},
    {"Feedback", "",  -99, 99, false, 30},
    {"ModRate",  "Hz",  0.02f, 12, true, 0.6f},
    {"ModDepth", "",    0, 99, false, 40},
    {"Drive",    "dB", -24, 48, false, 0},
    {"Freq",     "Hz",  20, 16000, true, 1000},
    {"Reso",     "",    0, 99, false, 30},
    {"Thresh",   "dB", -72, 0, false, -30},
    {"Pitch",    "st", -12, 12, false, 0},
    {"Balance",  "",    0, 99, false, 50},
    {"Output",   "dB", -24, 12, false, 0},
    {"Bypass",   "",    0, 1, false, 0},
    {"Input",    "dB", -24, 12, false, 0},
    {"Chain",    "",    0, 1, false, 0},
    {"In L",     "dB",  0, 1, false, 0},
    {"In R",     "dB",  0, 1, false, 0},
    {"Out L",    "dB",  0, 1, false, 0},
    {"Out R",    "dB",  0, 1, false, 0},
    {"Reverb On", "",   0, 1, false, 0},
    {"Delay On",  "",   0, 1, false, 0},
    {"Mod On",    "",   0, 1, false, 0},
    {"Char On",   "",   0, 1, false, 0},
};
// meter scale: normalized 0..1 = -48..+3 dBFS
static const float kMeterLoDb = -48.f, kMeterHiDb = 3.f;

inline float toReal(int p, float n) {
    const ParamMeta& m = kParams[p];
    n = n < 0 ? 0 : (n > 1 ? 1 : n);
    return m.log ? m.lo * powf(m.hi / m.lo, n) : m.lo + (m.hi - m.lo) * n;
}
inline float toNorm(int p, float v) {
    const ParamMeta& m = kParams[p];
    float n = m.log ? logf(v / m.lo) / logf(m.hi / m.lo) : (v - m.lo) / (m.hi - m.lo);
    return n < 0 ? 0 : (n > 1 ? 1 : n);
}

// reverb voicings (size, diffusion, early-reflection type/level, detune depth/rate, stereo width)
struct RevVoice { const char* id; float size, diff; int er; float erLvl, det, detRate, width; };
static const RevVoice kRev[] = {
    {"hall",     1.20f, 0.90f, 2, 0.25f, 12, 0.8f, 1.0f},   // 0  ASR-10 HALL: transverse (L/R) reflections
    {"room",     0.55f, 0.85f, 1, 0.35f,  8, 1.1f, 0.9f},   // 1
    {"sroom",    0.40f, 0.80f, 1, 0.40f,  6, 1.2f, 0.85f},  // 2
    {"lroom",    0.70f, 0.85f, 1, 0.35f,  8, 1.0f, 0.95f},  // 3
    {"hall2",    1.40f, 0.90f, 2, 0.20f, 14, 0.7f, 1.0f},   // 4
    {"splate",   0.60f, 1.00f, 0, 0.00f, 10, 1.0f, 1.0f},   // 5
    {"lplate",   1.00f, 1.00f, 0, 0.00f, 16, 1.0f, 1.0f},   // 6
    {"dense",    0.70f, 1.00f, 0, 0.00f,  8, 1.0f, 1.0f},   // 7  gated / reverse / non-linear core
    {"smooth",   0.90f, 1.00f, 0, 0.00f, 16, 0.9f, 1.0f},   // 8  X global SmoothPlate
    {"xlhall",   1.50f, 0.90f, 2, 0.20f, 14, 0.7f, 1.0f},   // 9  Large Hall
    {"xshall",   0.90f, 0.90f, 2, 0.25f, 12, 0.8f, 1.0f},   // 10 Small Hall
    {"xbroom",   0.80f, 0.85f, 1, 0.35f,  8, 1.0f, 0.95f},  // 11 Big Room
    {"xrefl",    0.35f, 0.60f, 1, 0.90f,  4, 1.0f, 1.0f},   // 12 Reflections
    {"xbright",  0.80f, 1.00f, 0, 0.00f, 12, 1.0f, 1.0f},   // 13 Bright
    {"xhuge",    1.70f, 0.95f, 2, 0.15f, 16, 0.6f, 1.0f},   // 14 Huge Place
};

struct AlgoDef {
    const char* name;      // shown on the MPC (<= 23 chars)
    uint8_t model;
    uint8_t st[4];
    uint8_t rev;           // kRev index for reverb stages
    uint8_t opt;           // stage option: chorus voices, dist curve, amp model, pitch window, flags
    const char* defs;      // default real values, "key=value ..."
};

// opt meanings: CHORUS voices; DIST curve 0 soft,1 hard,2 tube,3 fuzz,4 fold (in low nibble when combined);
// AMP model 1..4; PITCH window ms; REVERSE 1 = retrigger; NONLIN 1 = narrow; WAH 1 = auto-wah voicing
#define A10(n, s0, s1, s2, rv, op, d) {n, M_A10, {s0, s1, s2, S_NONE}, rv, op, d}
#define AX(n, s0, s1, s2, rv, op, d)  {n, M_X,   {s0, s1, s2, S_NONE}, rv, op, d}
#define AXG(n, rv, d)                 {n, M_XG,  {S_REV, S_NONE, S_NONE, S_NONE}, rv, 0, d}

static const AlgoDef kAlgos[] = {
    // ---- ASR-10 ROM effects (Musician's Manual Section 5 pp.3-4), ROM-01..ROM-50
    A10("10:HALL REVERB",  S_REV, 0, 0, 0, 0, "mix=35 decay=2.4 damp=35 pre=20"),
    A10("10:44KHZ REVERB", S_REV, 0, 0, 0, 0, "mix=35 decay=2.4 damp=30 pre=20"),
    A10("10:ROOM REVERB",  S_REV, 0, 0, 1, 0, "mix=30 decay=1.1 damp=40 pre=8"),
    A10("10:DUAL DELAYS",  S_DDL, 0, 0, 0, 0, "mix=35 delay=375 fb=35 damp=30 bal=28"),
    A10("10:44KHZ DELAYS", S_DDL, 0, 0, 0, 0, "mix=35 delay=375 fb=35 damp=20 bal=28"),
    A10("10:CHORUS+REVRB", S_CHORUS, S_REV, 0, 0, 4, "mix=45 rate=0.6 depth=40 reso=20 decay=2 damp=40 pre=15 bal=50"),
    A10("10:PHASER+REVRB", S_PHASER, S_REV, 0, 0, 0, "mix=50 rate=0.3 depth=60 reso=40 freq=700 decay=1.8 damp=40 bal=45"),
    A10("10:FLANGER+REV",  S_FLANGER, S_REV, 0, 0, 0, "mix=50 rate=0.25 depth=60 reso=55 freq=600 decay=1.8 damp=40 bal=40"),
    A10("10:ROT. SPKR+REV",S_ROTARY, S_REV, 0, 1, 0, "mix=100 rate=6.5 depth=70 drive=6 decay=1.5 damp=45 bal=25"),
    A10("10:CHOR+REV+DDL", S_CHORUS, S_REV, S_DDL, 0, 4, "mix=45 rate=0.6 depth=40 reso=15 decay=1.8 damp=40 delay=320 fb=25 bal=45"),
    A10("10:CMP+DIST+REV", S_COMP, S_DIST, S_REV, 1, 2, "mix=100 thresh=-24 reso=60 drive=24 freq=4500 decay=1.4 damp=45 bal=25"),
    A10("10:DIST+CHO+REV", S_DIST, S_CHORUS, S_REV, 1, 0x40, "mix=100 drive=20 freq=5000 rate=0.8 depth=35 decay=1.6 damp=40 bal=30"),
    A10("10:WAH+DIST+REV", S_WAH, S_DIST, S_REV, 1, 0, "mix=100 freq=500 depth=70 reso=50 drive=18 decay=1.3 damp=40 bal=25"),
    A10("10:SMALL ROOM",   S_REV, 0, 0, 2, 0, "mix=30 decay=0.6 damp=45 pre=4"),
    A10("10:LARGE ROOM",   S_REV, 0, 0, 3, 0, "mix=30 decay=1.4 damp=40 pre=10"),
    A10("10:HALL REVERB2", S_REV, 0, 0, 4, 0, "mix=32 decay=3.2 damp=35 pre=25"),
    A10("10:SMALL PLATE",  S_REV, 0, 0, 5, 0, "mix=30 decay=1.2 damp=25 pre=0"),
    A10("10:LARGE PLATE",  S_REV, 0, 0, 6, 0, "mix=30 decay=2.6 damp=20 pre=10"),
    A10("10:REVRSE REVRB", S_REVERSE, 0, 0, 7, 0, "mix=50 decay=0.6 thresh=-30 damp=30"),
    A10("10:REVRSE REVB2", S_REVERSE, 0, 0, 7, 1, "mix=50 decay=0.6 thresh=-30 damp=30"),
    A10("10:GATED REVERB", S_GATEDREV, 0, 0, 7, 0, "mix=45 decay=0.35 thresh=-36 damp=20 pre=0"),
    A10("10:NLIN REVRB 1", S_NONLIN, 0, 0, 7, 0, "mix=40 decay=0.25 damp=30 pre=0"),
    A10("10:NLIN REVRB 2", S_NONLIN, 0, 0, 7, 0, "mix=40 decay=0.6 damp=30 pre=0"),
    A10("10:NLIN REVRB 3", S_NONLIN, 0, 0, 7, 1, "mix=40 decay=0.25 damp=30 pre=0"),
    A10("10:MULTITAP DDL", S_MTAP, 0, 0, 0, 0, "mix=40 delay=600 fb=25 damp=30"),
    A10("10:EQ+DELAY LFO", S_EQ, S_MODDLY, 0, 0, 0, "mix=45 freq=3000 drive=0 delay=250 fb=40 rate=0.4 depth=30 bal=70"),
    A10("10:VCF+DISTORT",  S_VCF, S_DIST, 0, 0, 1, "mix=100 freq=900 reso=60 depth=60 drive=18"),
    A10("10:GUITAR AMP1",  S_AMP, 0, 0, 0, 1, "mix=100 drive=30 freq=800 thresh=-60"),
    A10("10:GUITAR AMP2",  S_AMP, 0, 0, 0, 2, "mix=100 drive=22 freq=600 thresh=-60"),
    A10("10:GUITAR AMP3",  S_AMP, 0, 0, 0, 3, "mix=100 drive=36 freq=1500 thresh=-60"),
    A10("10:SPKR CABINET", S_CAB, 0, 0, 0, 0, "mix=100"),
    A10("10:TUNABLE SPKR", S_TUNSPKR, 0, 0, 0, 0, "mix=100 freq=1500 reso=30 drive=6"),
    A10("10:EQ+CHOR+DDL",  S_EQ, S_CHORUS, S_DDL, 0, 4, "mix=50 freq=4000 drive=0 rate=0.7 depth=40 reso=10 delay=300 fb=25 bal=35"),
    A10("10:EQ+VIBR+DDL",  S_EQ, S_VIBRATO, S_DDL, 0, 0, "mix=100 freq=4000 drive=0 rate=5 depth=30 delay=300 fb=20 bal=30"),
    A10("10:EQ+FLNGR+DDL", S_EQ, S_FLANGER, S_DDL, 0, 0, "mix=50 freq=4000 drive=0 rate=0.3 depth=60 reso=50 delay=300 fb=20 bal=30"),
    A10("10:EQ+TREM+DDL",  S_EQ, S_TREM, S_DDL, 0, 0, "mix=100 freq=4000 drive=0 rate=4 depth=60 delay=300 fb=20 bal=30"),
    A10("10:PHASER+DDL",   S_PHASER, S_DDL, 0, 0, 0, "mix=50 rate=0.3 depth=60 reso=40 freq=700 delay=350 fb=25 damp=30 bal=35"),
    A10("10:8-VOICE CHOR", S_CHORUS, 0, 0, 0, 8, "mix=50 rate=0.5 depth=45 reso=10"),
    A10("10:PITCH SHIFT",  S_PITCH, 0, 0, 0, 60, "mix=50 pitch=7 depth=10"),
    A10("10:PITCH+DDL",    S_PITCH, S_DDL, 0, 0, 60, "mix=45 pitch=12 depth=0 delay=250 fb=30 damp=30 bal=50"),
    A10("10:FAST PITCHSH", S_PITCH, 0, 0, 0, 18, "mix=50 pitch=0 depth=15"),
    A10("10:EQ+CMPRESSOR", S_EQ, S_COMP, 0, 0, 0, "mix=100 freq=2500 drive=0 thresh=-20 reso=40"),
    A10("10:EXPANDER",     S_EXP, 0, 0, 0, 0, "mix=100 thresh=-40 reso=40"),
    A10("10:KEYED EXPNDR", S_KEYEXP, 0, 0, 0, 0, "mix=100 thresh=-35 reso=60 freq=150"),
    A10("10:INVRS EXPNDR", S_INVEXP, 0, 0, 0, 0, "mix=100 thresh=-30 reso=40"),
    A10("10:DE-ESSER",     S_DEESS, 0, 0, 0, 0, "mix=100 freq=5500 thresh=-28"),
    A10("10:DUCKER",       S_DUCK, 0, 0, 0, 0, "mix=100 thresh=-18 reso=50"),
    A10("10:RUMBLE FILTR", S_RUMBLE, 0, 0, 0, 0, "mix=100 freq=60"),
    A10("10:PARAM EQ",     S_PEQ, 0, 0, 0, 0, "mix=100 freq=1000 reso=30 drive=4"),
    A10("10:VAN DER POL",  S_EXCITER, 0, 0, 0, 0, "mix=100 freq=3000 drive=6"),

    // ---- ASR-X / ASR-X Pro insert effects (Reference Manual ch.4 p.41), 01..40
    AX("X:Parametric EQ",  S_PEQ, 0, 0, 0, 0, "mix=100 freq=1000 reso=30 drive=4"),
    AX("X:Hall Reverb",    S_REV, 0, 0, 0, 0, "mix=35 decay=2.4 damp=35 pre=20"),
    AX("X:Large Room",     S_REV, 0, 0, 3, 0, "mix=30 decay=1.4 damp=40 pre=10"),
    AX("X:Small Room",     S_REV, 0, 0, 2, 0, "mix=30 decay=0.6 damp=45 pre=4"),
    AX("X:Large Plate",    S_REV, 0, 0, 6, 0, "mix=30 decay=2.6 damp=20 pre=10"),
    AX("X:Small Plate",    S_REV, 0, 0, 5, 0, "mix=30 decay=1.2 damp=25 pre=0"),
    AX("X:NonLinReverb1",  S_NONLIN, 0, 0, 7, 0, "mix=40 decay=0.25 damp=30 pre=0"),
    AX("X:NonLinReverb2",  S_NONLIN, 0, 0, 7, 0, "mix=40 decay=0.6 damp=30 pre=0"),
    AX("X:Gated Reverb",   S_GATEDREV, 0, 0, 7, 0, "mix=45 decay=0.35 thresh=-36 damp=20 pre=0"),
    AX("X:Stereo Chorus",  S_CHORUS, 0, 0, 0, 2, "mix=50 rate=0.7 depth=40 reso=0"),
    AX("X:8-VoiceChorus",  S_CHORUS, 0, 0, 0, 8, "mix=50 rate=0.5 depth=45 reso=10"),
    AX("X:Rev>Chorus",     S_REV, S_CHORUS, 0, 6, 4, "mix=45 decay=2 damp=35 pre=10 rate=0.6 depth=40 reso=10 bal=60"),
    AX("X:Rev>Flanger",    S_REV, S_FLANGER, 0, 6, 0, "mix=45 decay=2 damp=35 pre=10 rate=0.2 depth=60 reso=50 freq=600 bal=60"),
    AX("X:Rev>Phaser",     S_REV, S_PHASER, 0, 6, 0, "mix=45 decay=2 damp=35 pre=10 rate=0.25 depth=60 reso=40 freq=700 bal=60"),
    AX("X:Chorus>Rev",     S_CHORUS, S_REV, 0, 0, 4, "mix=45 rate=0.6 depth=40 reso=15 decay=2 damp=40 pre=15 bal=50"),
    AX("X:Flanger>Rev",    S_FLANGER, S_REV, 0, 0, 0, "mix=50 rate=0.25 depth=60 reso=55 freq=600 decay=1.8 damp=40 bal=40"),
    AX("X:Phaser>Rev",     S_PHASER, S_REV, 0, 0, 0, "mix=50 rate=0.3 depth=60 reso=40 freq=700 decay=1.8 damp=40 bal=45"),
    AX("X:EQ>Reverb",      S_EQ, S_REV, 0, 0, 0, "mix=35 freq=8000 drive=-6 decay=2.2 damp=30 pre=15 bal=99"),
    AX("X:Spinner>Rev",    S_SPINNER, S_REV, 0, 1, 0, "mix=100 rate=1.5 depth=80 decay=1.4 damp=40 bal=30"),
    AX("X:DDL>Chorus",     S_DDL, S_CHORUS, 0, 0, 4, "mix=50 delay=330 fb=30 damp=30 rate=0.6 depth=40 reso=10 bal=60"),
    AX("X:DDL>Flanger",    S_DDL, S_FLANGER, 0, 0, 0, "mix=50 delay=330 fb=30 damp=30 rate=0.25 depth=60 reso=50 freq=600 bal=60"),
    AX("X:DDL>Phaser",     S_DDL, S_PHASER, 0, 0, 0, "mix=50 delay=330 fb=30 damp=30 rate=0.3 depth=60 reso=40 freq=700 bal=60"),
    AX("X:DDL>EQ",         S_DDL, S_EQ, 0, 0, 0, "mix=45 delay=375 fb=35 damp=20 freq=1200 drive=6 bal=60"),
    AX("X:Multi-Tap DDL",  S_MTAP, 0, 0, 0, 0, "mix=40 delay=600 fb=25 damp=30"),
    AX("X:Dist>Chorus",    S_DIST, S_CHORUS, 0, 0, 0x40, "mix=100 drive=20 freq=5000 rate=0.8 depth=35 reso=10"),
    AX("X:Dist>Flanger",   S_DIST, S_FLANGER, 0, 0, 0, "mix=100 drive=20 freq=5000 rate=0.25 depth=60 reso=50"),
    AX("X:Dist>Phaser",    S_DIST, S_PHASER, 0, 0, 0, "mix=100 drive=20 freq=5000 rate=0.3 depth=60 reso=40"),
    AX("X:Dist>Auto Wah",  S_DIST, S_WAH, 0, 0, 1, "mix=100 drive=18 freq=450 depth=75 reso=55"),
    AX("X:ResVCF>DDL",     S_VCF, S_DDL, 0, 0, 0, "mix=100 freq=700 reso=70 depth=60 delay=300 fb=30 damp=30 bal=40"),
    AX("X:Dist>VCF>DDL",   S_DIST, S_VCF, S_DDL, 0, 1, "mix=100 drive=18 freq=900 reso=60 depth=55 delay=300 fb=25 damp=30 bal=35"),
    AX("X:Pitch Detuner",  S_DETUNE, 0, 0, 0, 30, "mix=50 depth=20"),
    AX("X:Chatter Box",    S_CHATTER, 0, 0, 0, 0, "mix=100 rate=6 depth=80"),
    AX("X:Formant Morph",  S_FORMANT, 0, 0, 0, 0, "mix=100 rate=0.4 depth=70 freq=1000"),
    AX("X:RotarySpeaker",  S_ROTARY, 0, 0, 0, 0, "mix=100 rate=6.5 depth=70 drive=3"),
    AX("X:Tunable Spkr",   S_TUNSPKR, 0, 0, 0, 0, "mix=100 freq=1500 reso=30 drive=6"),
    AX("X:Guitar Amp",     S_AMP, 0, 0, 0, 4, "mix=100 drive=28 freq=900 thresh=-60"),
    AX("X:Dist>DDL>Trem",  S_DIST, S_DDL, S_TREM, 0, 0, "mix=100 drive=18 freq=5000 delay=300 fb=25 damp=30 rate=5 depth=60 bal=35"),
    AX("X:Comp>Dist>DDL",  S_COMP, S_DIST, S_DDL, 0, 2, "mix=100 thresh=-24 reso=60 drive=20 freq=4500 delay=300 fb=25 damp=30 bal=30"),
    AX("X:EQ>Comp>Gate",   S_EQ, S_COMP, S_GATE, 0, 0, "mix=100 freq=3000 drive=0 thresh=-20 reso=40"),
    AX("X:EQ>Chorus>DDL",  S_EQ, S_CHORUS, S_DDL, 0, 4, "mix=50 freq=4000 drive=0 rate=0.7 depth=40 reso=10 delay=300 fb=25 damp=30 bal=35"),

    // ---- ASR-X / ASR-X Pro global reverbs (Reference Manual ch.4 p.42), 01..08
    AXG("XG:SmoothPlate",  8,  "mix=30 decay=2.2 damp=30 pre=10"),
    AXG("XG:Large Hall",   9,  "mix=32 decay=3.4 damp=35 pre=25"),
    AXG("XG:Small Hall",   10, "mix=30 decay=1.8 damp=35 pre=15"),
    AXG("XG:Big Room",     11, "mix=30 decay=1.3 damp=40 pre=10"),
    AXG("XG:Small Room",   2,  "mix=28 decay=0.6 damp=45 pre=4"),
    AXG("XG:Reflections",  12, "mix=30 decay=0.4 damp=40 pre=0"),
    AXG("XG:Bright",       13, "mix=28 decay=1.8 damp=5 pre=10"),
    AXG("XG:Huge Place",   14, "mix=35 decay=8 damp=30 pre=40"),
};
static const int NALGOS = (int)(sizeof(kAlgos) / sizeof(kAlgos[0]));

inline bool isAdditive(uint8_t s) {
    return s == S_REV || s == S_GATEDREV || s == S_REVERSE || s == S_NONLIN || s == S_DDL || s == S_MTAP || s == S_MODDLY;
}
inline int chainLen(const AlgoDef& a) { int n = 0; while (n < 4 && a.st[n] != S_NONE) n++; return n; }

// Which shared knobs a stage wants: primary claims are assigned first across the whole chain,
// then secondary claims fill whatever is left. A knob belongs to exactly one stage.
struct Claims { uint32_t prim, sec; };
#define B(p) (1u << (p))
inline Claims stageClaims(uint8_t s) {
    switch (s) {
    case S_REV:      return {B(P_DECAY) | B(P_DAMP) | B(P_PRE), 0};
    case S_GATEDREV: return {B(P_DECAY) | B(P_THRESH) | B(P_DAMP) | B(P_PRE), 0};
    case S_REVERSE:  return {B(P_DECAY) | B(P_THRESH) | B(P_DAMP), 0};
    case S_NONLIN:   return {B(P_DECAY) | B(P_DAMP) | B(P_PRE), 0};
    case S_DDL:      return {B(P_DELAY) | B(P_FB) | B(P_DAMP), 0};
    case S_MTAP:     return {B(P_DELAY) | B(P_FB) | B(P_DAMP), 0};
    case S_MODDLY:   return {B(P_DELAY) | B(P_FB) | B(P_RATE) | B(P_DEPTH), 0};
    case S_CHORUS:   return {B(P_RATE) | B(P_DEPTH) | B(P_RESO), 0};
    case S_FLANGER:  return {B(P_RATE) | B(P_DEPTH) | B(P_RESO), B(P_FREQ)};
    case S_PHASER:   return {B(P_RATE) | B(P_DEPTH) | B(P_RESO), B(P_FREQ)};
    case S_VIBRATO:  return {B(P_RATE) | B(P_DEPTH), 0};
    case S_TREM:     return {B(P_RATE) | B(P_DEPTH), 0};
    case S_ROTARY:   return {B(P_RATE) | B(P_DEPTH), B(P_DRIVE)};
    case S_SPINNER:  return {B(P_RATE) | B(P_DEPTH), 0};
    case S_DIST:     return {B(P_DRIVE), B(P_FREQ)};
    case S_VCF:      return {B(P_FREQ) | B(P_RESO) | B(P_DEPTH), 0};
    case S_WAH:      return {B(P_FREQ) | B(P_DEPTH) | B(P_RESO), 0};
    case S_AMP:      return {B(P_DRIVE) | B(P_FREQ) | B(P_THRESH), 0};
    case S_CAB:      return {0, 0};
    case S_TUNSPKR:  return {B(P_FREQ) | B(P_RESO) | B(P_DRIVE), 0};
    case S_EQ:       return {B(P_FREQ) | B(P_DRIVE), 0};
    case S_PEQ:      return {B(P_FREQ) | B(P_RESO) | B(P_DRIVE), 0};
    case S_COMP:     return {B(P_THRESH) | B(P_RESO), 0};
    case S_GATE:     return {B(P_THRESH), 0};
    case S_EXP:      return {B(P_THRESH) | B(P_RESO), 0};
    case S_INVEXP:   return {B(P_THRESH) | B(P_RESO), 0};
    case S_KEYEXP:   return {B(P_THRESH) | B(P_RESO), B(P_FREQ)};
    case S_DEESS:    return {B(P_FREQ) | B(P_THRESH), 0};
    case S_DUCK:     return {B(P_THRESH) | B(P_RESO), 0};
    case S_PITCH:    return {B(P_PITCH) | B(P_DEPTH), 0};
    case S_DETUNE:   return {B(P_DEPTH), 0};
    case S_FORMANT:  return {B(P_RATE) | B(P_DEPTH) | B(P_FREQ), 0};
    case S_CHATTER:  return {B(P_RATE) | B(P_DEPTH), 0};
    case S_RUMBLE:   return {B(P_FREQ), 0};
    case S_EXCITER:  return {B(P_FREQ) | B(P_DRIVE), 0};
    default:         return {0, 0};
    }
}

struct AlgoInfo {
    uint32_t own[4];       // knob bitmask owned by each stage
    int owner[NPARAMS];    // stage type owning each knob (-1 none); P_BAL uses S_COUNT for "chain balance"
    bool balChain;         // Balance = send level into additive stages
    bool balRatio;         // Balance = right/left delay time ratio (single-stage delays)
    float def[NPARAMS];    // default normalized values
    bool mod[4];           // skin modules in use: reverb, delay, modulation, character
    char chain[32];        // e.g. "CHORUS>REVERB"
};

inline int moduleOf(uint8_t s) {
    switch (s) {
    case S_REV: case S_GATEDREV: case S_REVERSE: case S_NONLIN: return 0;
    case S_DDL: case S_MTAP: case S_MODDLY: return 1;
    case S_CHORUS: case S_FLANGER: case S_PHASER: case S_VIBRATO: case S_TREM: case S_ROTARY: case S_SPINNER:
    case S_FORMANT: case S_CHATTER: case S_DETUNE: return 2;
    default: return 3;
    }
}
inline const char* stageShort(uint8_t s) {
    static const char* n[S_COUNT] = {"", "REVERB", "GATED REV", "REVERSE REV", "NONLIN REV", "DELAY", "MULTITAP",
        "LFO DELAY", "CHORUS", "FLANGER", "PHASER", "VIBRATO", "TREMOLO", "ROTARY", "SPINNER", "DIST", "VCF", "WAH",
        "AMP", "CABINET", "TUNE SPKR", "EQ", "PARAM EQ", "COMP", "GATE", "EXPANDER", "INV EXPAND", "KEY EXPAND",
        "DE-ESSER", "DUCKER", "PITCH", "DETUNE", "FORMANT", "CHATTER", "RUMBLE", "EXCITER"};
    return s < S_COUNT ? n[s] : "";
}

inline void buildInfo(int a, AlgoInfo& in) {
    const AlgoDef& d = kAlgos[a];
    int n = chainLen(d);
    memset(&in, 0, sizeof(in));
    for (int p = 0; p < NPARAMS; p++) in.owner[p] = -1;
    uint32_t taken = 0;
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < n; i++) {
            Claims c = stageClaims(d.st[i]);
            uint32_t want = (pass ? c.sec : c.prim) & ~taken;
            in.own[i] |= want; taken |= want;
            for (int p = 0; p < NPARAMS; p++) if (want & B(p)) in.owner[p] = d.st[i];
        }
    bool anyAdd = false;
    for (int i = 0; i < n; i++) anyAdd |= isAdditive(d.st[i]);
    in.balChain = n > 1 && anyAdd;
    in.balRatio = n == 1 && d.st[0] == S_DDL;
    if (in.balChain || in.balRatio) in.owner[P_BAL] = in.balRatio ? S_DDL : S_COUNT;
    in.owner[P_ALGO] = in.owner[P_MIX] = in.owner[P_OUT] = in.owner[P_BYPASS] = S_COUNT;
    for (int p = P_INPUT; p < NPARAMS; p++) in.owner[p] = S_COUNT;
    in.chain[0] = 0;
    for (int i = 0; i < n; i++) {
        in.mod[moduleOf(d.st[i])] = true;
        if (i) strncat(in.chain, ">", sizeof(in.chain) - strlen(in.chain) - 1);
        strncat(in.chain, stageShort(d.st[i]), sizeof(in.chain) - strlen(in.chain) - 1);
    }
    // defaults
    float real[NPARAMS];
    for (int p = 0; p < NPARAMS; p++) real[p] = kParams[p].def;
    const char* s = d.defs;
    while (s && *s) {
        char key[16]; float v; int used = 0;
        if (sscanf(s, " %15[a-z]=%f%n", key, &v, &used) != 2 || used == 0) break;
        static const char* keys[P_INPUT + 1] = {"algo", "mix", "decay", "damp", "pre", "delay", "fb", "rate", "depth",
                                            "drive", "freq", "reso", "thresh", "pitch", "bal", "out", "bypass", "input"};
        for (int p = 0; p <= P_INPUT; p++) if (!strcmp(key, keys[p])) real[p] = v;
        s += used;
    }
    for (int p = 0; p < NPARAMS; p++) in.def[p] = toNorm(p, real[p]);
    // knobs this algorithm doesn't use rest at zero, so the screen shows them empty (they are never read)
    for (int p = P_DECAY; p <= P_BAL; p++) if (in.owner[p] < 0) in.def[p] = 0.f;
    in.def[P_ALGO] = NALGOS > 1 ? (float)a / (float)(NALGOS - 1) : 0.f;
    in.def[P_BYPASS] = 0;
    in.def[P_CHAIN] = in.def[P_ALGO];
    for (int p = P_INL; p <= P_OUTR; p++) in.def[p] = 0;
    for (int m = 0; m < 4; m++) in.def[P_LED_REV + m] = in.mod[m] ? 1.f : 0.f;
}
#undef B

inline int algoFromNorm(float n) {
    int a = (int)floorf(n * (float)(NALGOS - 1) + 0.5f);
    return a < 0 ? 0 : (a >= NALGOS ? NALGOS - 1 : a);
}

} // namespace rsfx
