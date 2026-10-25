// RSFX DSP building blocks. Real-time safe: all memory is allocated in alloc(), never while processing.
#pragma once
#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace rsfx {

static const float kPi = 3.14159265358979f;
static const float kTwoPi = 6.28318530717959f;

inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float dbToLin(float db) { return powf(10.f, db * 0.05f); }
inline float linToDb(float x) { return 20.f * log10f(x > 1e-9f ? x : 1e-9f); }
inline float fastTanh(float x) {               // rational approximation, |err| < 0.003, exact clamp at +-1
    if (x > 4.97f) return 1.f;
    if (x < -4.97f) return -1.f;
    float x2 = x * x;
    float y = x * (27.f + x2) / (27.f + 9.f * x2);
    return clampf(y, -1.f, 1.f);
}
inline float fastSin(float ph) {                // ph in turns [0,1): parabolic sine, max err ~0.001
    float x = ph - floorf(ph);
    x = x * 2.f - 1.f;                          // -1..1 maps to -pi..pi, then negate to get sin
    float y = 4.f * x * (1.f - fabsf(x));
    y = 0.225f * (y * fabsf(y) - y) + y;
    return -y;
}
inline float wrap1(float p) { return p - floorf(p); }
inline float onePoleCoef(float ms, float sr) { return ms <= 0.f ? 1.f : 1.f - expf(-1.f / (ms * 0.001f * sr)); }

// ---------------------------------------------------------------- delay line (read before write)
struct Delay {
    float* buf = nullptr; int size = 0, mask = 0, w = 0;
    void alloc(int maxSamples) {
        free();
        size = 1; while (size < maxSamples + 4) size <<= 1;
        mask = size - 1; buf = (float*)calloc((size_t)size, sizeof(float)); w = 0;
    }
    void free() { if (buf) ::free(buf); buf = nullptr; size = 0; }
    void clear() { if (buf) memset(buf, 0, sizeof(float) * (size_t)size); }
    inline void put(float x) { buf[w] = x; w = (w + 1) & mask; }
    inline float geti(int d) const { return buf[(w - d) & mask]; }          // d >= 1
    inline float get(float d) const {                                        // fractional, d >= 1
        if (d < 1.f) d = 1.f;
        if (d > (float)(size - 3)) d = (float)(size - 3);
        int i = (int)d; float f = d - (float)i;
        float a = buf[(w - i) & mask], b = buf[(w - i - 1) & mask];
        return a + (b - a) * f;
    }
    ~Delay() { free(); }
};

// ---------------------------------------------------------------- biquad (RBJ), TDF-II
struct Biquad {
    enum Type { LP, HP, BP, PEAK, LSHELF, HSHELF, NOTCH };
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void clear() { z1 = z2 = 0; }
    void set(Type t, float f, float q, float gainDb, float sr) {
        f = clampf(f, 10.f, sr * 0.45f); q = clampf(q, 0.1f, 40.f);
        float A = powf(10.f, gainDb / 40.f), w0 = kTwoPi * f / sr, cw = cosf(w0), sw = sinf(w0);
        float al = sw / (2.f * q), a0 = 1;
        switch (t) {
        case LP: b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = b0; a0 = 1 + al; a1 = -2 * cw; a2 = 1 - al; break;
        case HP: b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = b0; a0 = 1 + al; a1 = -2 * cw; a2 = 1 - al; break;
        case BP: b0 = al; b1 = 0; b2 = -al; a0 = 1 + al; a1 = -2 * cw; a2 = 1 - al; break;
        case NOTCH: b0 = 1; b1 = -2 * cw; b2 = 1; a0 = 1 + al; a1 = -2 * cw; a2 = 1 - al; break;
        case PEAK: b0 = 1 + al * A; b1 = -2 * cw; b2 = 1 - al * A; a0 = 1 + al / A; a1 = -2 * cw; a2 = 1 - al / A; break;
        case LSHELF: { float s = 2 * sqrtf(A) * al;
            b0 = A * ((A + 1) - (A - 1) * cw + s); b1 = 2 * A * ((A - 1) - (A + 1) * cw); b2 = A * ((A + 1) - (A - 1) * cw - s);
            a0 = (A + 1) + (A - 1) * cw + s; a1 = -2 * ((A - 1) + (A + 1) * cw); a2 = (A + 1) + (A - 1) * cw - s; } break;
        case HSHELF: { float s = 2 * sqrtf(A) * al;
            b0 = A * ((A + 1) + (A - 1) * cw + s); b1 = -2 * A * ((A - 1) + (A + 1) * cw); b2 = A * ((A + 1) + (A - 1) * cw - s);
            a0 = (A + 1) - (A - 1) * cw + s; a1 = 2 * ((A - 1) - (A + 1) * cw); a2 = (A + 1) - (A - 1) * cw - s; } break;
        }
        b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
    }
    inline float run(float x) { float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
};

// ---------------------------------------------------------------- TPT state-variable filter (stable under fast modulation)
struct SVF {
    float ic1 = 0, ic2 = 0;
    void clear() { ic1 = ic2 = 0; }
    // returns lp, writes bp/hp
    inline float run(float x, float g, float k, float& bp, float& hp) {
        float a1 = 1.f / (1.f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        bp = v1; hp = x - k * v1 - v2; return v2;
    }
    static inline float gOf(float f, float sr) { return tanf(kPi * clampf(f, 10.f, sr * 0.45f) / sr); }
};

// ---------------------------------------------------------------- envelope follower (peak, log-friendly)
struct Env {
    float y = 0, a = 0.01f, r = 0.001f;
    void set(float atkMs, float relMs, float sr) { a = onePoleCoef(atkMs, sr); r = onePoleCoef(relMs, sr); }
    void clear() { y = 0; }
    inline float run(float x) { x = fabsf(x); y += (x > y ? a : r) * (x - y); return y; }
};

// ---------------------------------------------------------------- Dattorro-topology plate/hall tank
// Topology and base lengths from J. Dattorro, "Effect Design Part 1", JAES 45(9), 1997 (lengths at 29761 Hz).
struct Plate {
    static constexpr float kBaseSR = 29761.f, kMaxSize = 1.7f;
    Delay pre, in1, in2, in3, in4, mapL, dL1, apL2, dL2, mapR, dR1, apR2, dR2;
    float sr = 48000, scale = 1;
    float lIn = 0, dampL = 0, dampR = 0, ph = 0;
    // current settings
    float lenIn[4], lenMapL, lenDL1, lenApL2, lenDL2, lenMapR, lenDR1, lenApR2, lenDR2;
    float decay = 0.5f, bw = 0.9995f, damp = 0.0005f, id1 = 0.75f, id2 = 0.625f, dd1 = 0.7f, dd2 = 0.5f;
    float exc = 16, rate = 1.f, preS = 1, width = 1.f;
    int tL[7], tR[7];
    void alloc(float s) {
        sr = s; scale = sr / kBaseSR;
        int k = (int)(scale * kMaxSize) + 2;
        pre.alloc((int)(sr * 0.6f));
        in1.alloc(142 * k); in2.alloc(107 * k); in3.alloc(379 * k); in4.alloc(277 * k);
        mapL.alloc(672 * k + 64 * k); dL1.alloc(4453 * k); apL2.alloc(1800 * k); dL2.alloc(3720 * k);
        mapR.alloc(908 * k + 64 * k); dR1.alloc(4217 * k); apR2.alloc(2656 * k); dR2.alloc(3163 * k);
        configure(1.f);
    }
    void clear() {
        Delay* all[] = {&pre, &in1, &in2, &in3, &in4, &mapL, &dL1, &apL2, &dL2, &mapR, &dR1, &apR2, &dR2};
        for (Delay* d : all) d->clear();
        lIn = dampL = dampR = 0;
    }
    void configure(float size) {   // size 0.25..kMaxSize
        size = clampf(size, 0.25f, kMaxSize);
        float s = scale * size;
        float base[4] = {142, 107, 379, 277};
        for (int i = 0; i < 4; i++) lenIn[i] = fmaxf(1.f, base[i] * scale * fminf(size, 1.f));
        lenMapL = 672 * s; lenDL1 = 4453 * s; lenApL2 = 1800 * s; lenDL2 = 3720 * s;
        lenMapR = 908 * s; lenDR1 = 4217 * s; lenApR2 = 2656 * s; lenDR2 = 3163 * s;
        int tl[7] = {266, 2974, 1913, 1996, 1990, 187, 1066}, tr[7] = {353, 3627, 1228, 2673, 2111, 335, 121};
        for (int i = 0; i < 7; i++) { tL[i] = (int)(tl[i] * s) + 1; tR[i] = (int)(tr[i] * s) + 1; }
    }
    // decaySec = RT60; damping/bandwidth 0..1; diff 0..1; detune depth in base samples; rate Hz
    void setParams(float rt60, float damping, float bandwidth, float diff, float detuneDepth, float detuneRate,
                   float predelayMs, float stereoWidth) {
        float th = (lenMapL + lenDL1 + lenApL2 + lenDL2 + lenMapR + lenDR1 + lenApR2 + lenDR2) * 0.5f / sr;
        rt60 = fmaxf(rt60, 0.05f);
        decay = clampf(powf(10.f, -3.f * th / (2.f * rt60)), 0.f, 0.9992f);
        damp = clampf(damping, 0.f, 0.95f);
        bw = clampf(bandwidth, 0.05f, 0.9999f);
        id1 = 0.75f * diff; id2 = 0.625f * diff;
        dd1 = 0.7f; dd2 = clampf(decay + 0.15f, 0.25f, 0.5f);
        exc = detuneDepth * scale; rate = detuneRate;
        preS = fmaxf(1.f, predelayMs * 0.001f * sr);
        width = stereoWidth;
    }
    static inline float ap(Delay& d, float len, float x, float g) {
        float z = d.get(len); float v = x - g * z; d.put(v); return z + g * v;
    }
    inline void run(float inL, float inR, float& outL, float& outR) {
        float x = (inL + inR) * 0.5f;
        pre.put(x); x = pre.get(preS);
        lIn += bw * (x - lIn); x = lIn;
        x = ap(in1, lenIn[0], x, id1); x = ap(in2, lenIn[1], x, id1);
        x = ap(in3, lenIn[2], x, id2); x = ap(in4, lenIn[3], x, id2);
        ph += rate / sr; if (ph >= 1.f) ph -= 1.f;
        float mL = exc * (1.f + fastSin(ph)), mR = exc * (1.f + fastSin(ph + 0.25f));
        float fbR = dR2.get(lenDR2), fbL = dL2.get(lenDL2);
        // left half
        float a = ap(mapL, lenMapL + mL, x + decay * fbR, -dd1);
        float b = dL1.get(lenDL1); dL1.put(a);
        dampL += (1.f - damp) * (b - dampL);
        float c = ap(apL2, lenApL2, dampL * decay, dd2);
        dL2.put(c);
        // right half
        float a2 = ap(mapR, lenMapR + mR, x + decay * fbL, -dd1);
        float b2 = dR1.get(lenDR1); dR1.put(a2);
        dampR += (1.f - damp) * (b2 - dampR);
        float c2 = ap(apR2, lenApR2, dampR * decay, dd2);
        dR2.put(c2);
        float yl = dR1.geti(tL[0]) + dR1.geti(tL[1]) - apR2.geti(tL[2]) + dR2.geti(tL[3])
                 - dL1.geti(tL[4]) - apL2.geti(tL[5]) - dL2.geti(tL[6]);
        float yr = dL1.geti(tR[0]) + dL1.geti(tR[1]) - apL2.geti(tR[2]) + dL2.geti(tR[3])
                 - dR1.geti(tR[4]) - apR2.geti(tR[5]) - dR2.geti(tR[6]);
        yl *= 0.6f; yr *= 0.6f;
        float m = (yl + yr) * 0.5f, sd = (yl - yr) * 0.5f * width;
        outL = m + sd; outR = m - sd;
    }
};

// ---------------------------------------------------------------- early reflections (tapped delay)
struct EarlyRefl {
    Delay d; float sr = 48000;
    void alloc(float s) { sr = s; d.alloc((int)(s * 0.3f)); }
    void clear() { d.clear(); }
    // up to 6 taps: time ms, gain, pan (-1 L .. +1 R)
    inline void run(float x, const float* ms, const float* g, const float* pan, int n, float scale, float& l, float& r) {
        d.put(x); l = r = 0;
        for (int i = 0; i < n; i++) {
            float v = d.get(ms[i] * scale * 0.001f * sr) * g[i];
            l += v * (1.f - pan[i]) * 0.5f; r += v * (1.f + pan[i]) * 0.5f;
        }
    }
};

// ---------------------------------------------------------------- two-tap splice pitch shifter ("splicer type")
struct Pitch {
    Delay d; float sr = 48000;
    void alloc(float s) { sr = s; d.alloc((int)(s * 0.25f)); }
    void clear() { d.clear(); }
    inline void push(float x) { d.put(x); }
    // ph = this voice's splice phase (0..1), ratio = playback ratio, winMs = splice window. Call after push().
    inline float read(float& ph, float ratio, float winMs) const {
        float W = winMs * 0.001f * sr;
        ph += (1.f - ratio) / W; ph = wrap1(ph);
        float p2 = wrap1(ph + 0.5f);
        float s1 = fastSin(ph * 0.5f), s2 = fastSin(p2 * 0.5f);     // sin(pi*p)
        float w1 = s1 * s1, w2 = s2 * s2;
        return (d.get(2.f + ph * W) * w1 + d.get(2.f + p2 * W) * w2) / (w1 + w2 + 1e-6f);
    }
};

} // namespace rsfx
