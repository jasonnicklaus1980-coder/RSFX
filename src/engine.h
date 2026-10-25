// RSFX engine: runs one algorithm (a chain of up to 4 stages) on a stereo signal.
#pragma once
#include "dsp.h"
#include "algos.h"

namespace rsfx {

static const int kChunk = 32;

struct Engine {
    float sr = 48000;
    bool ready = false;
    AlgoInfo info[256];

    // ---- current state
    int algo = 0, pending = 0;
    float fade = 1.f; int fadeDir = 0;          // algorithm switch crossfade on the wet signal
    float val[NPARAMS];                          // smoothed real values
    float bypassMix = 0.f;
    uint32_t own = 0;                            // knob mask of the stage being processed
    uint32_t noise = 22222;

    // ---- modules (one of each type; no algorithm uses a type twice)
    Plate plate; EarlyRefl er;
    Delay ddlL, ddlR, mtap, mdL, mdR, chL, chR, flL, flR, vibL, vibR, rotHL, rotHR, rotDL, rotDR;
    Pitch pitchL, pitchR;
    float pv[4] = {0, 0, 0, 0};
    float ddlLpL = 0, ddlLpR = 0, mtapLp = 0, mdLp = 0, mdLpR = 0;
    // delay-time glides in double: in float, steps near 16k samples round to zero and the time stalls short
    double ddlCurL = -1, ddlCurR = -1, mdCur = -1;
    float ph[8];                                 // LFO phases
    float apStL[12], apStR[12], phFbL = 0, phFbR = 0;
    float flFbL = 0, flFbR = 0;
    float rotHorn = 0, rotDrum = 0, rotSpdH = 1, rotSpdD = 1;
    Biquad rotXL, rotXR, rotXHL, rotXHR;
    Biquad bq[12];                               // general filters (indices documented at use)
    Biquad lofi[4];
    SVF svfL, svfR, fmt[6];
    Env envIn, envDet, envTrig, envKey;
    float gainSm = 1.f, gateGain = 0.f, trigEnv = 0.f, trigT = 1e9f; bool trigArmed = true, trigActive = false;
    float formPos = 0, formTarget = 0;
    float wetL[kChunk], wetR[kChunk], tmpL[kChunk], tmpR[kChunk];
    // meters: peak with ~300 ms fall, clip latch held 1.5 s. Written here, read by the host via getParameter.
    float meter[4] = {0, 0, 0, 0}, clipHold[4] = {0, 0, 0, 0};
    float meterNorm[4] = {0, 0, 0, 0};
    float inGain = 1.f;
    bool snap = true;      // jump straight to the knob values (first block, and right after an algorithm loads)

    Engine() {
        for (int a = 0; a < NALGOS && a < 256; a++) buildInfo(a, info[a]);
        for (int p = 0; p < NPARAMS; p++) val[p] = toReal(p, info[0].def[p]);
        memset(ph, 0, sizeof(ph));
    }

    void prepare(float sampleRate) {
        sr = sampleRate > 8000 ? sampleRate : 48000;
        plate.alloc(sr); er.alloc(sr);
        int d22 = (int)(sr * 2.25f);
        ddlL.alloc(d22); ddlR.alloc(d22); mtap.alloc(d22);
        mdL.alloc((int)(sr * 2.1f)); mdR.alloc((int)(sr * 2.1f));
        chL.alloc((int)(sr * 0.06f)); chR.alloc((int)(sr * 0.06f));
        flL.alloc((int)(sr * 0.05f)); flR.alloc((int)(sr * 0.05f));
        vibL.alloc((int)(sr * 0.03f)); vibR.alloc((int)(sr * 0.03f));
        rotHL.alloc((int)(sr * 0.02f)); rotHR.alloc((int)(sr * 0.02f));
        rotDL.alloc((int)(sr * 0.02f)); rotDR.alloc((int)(sr * 0.02f));
        pitchL.alloc(sr); pitchR.alloc(sr);
        for (int i = 0; i < 4; i++) lofi[i].set(Biquad::LP, 13500.f, i % 2 ? 1.3066f : 0.5412f, 0, sr);
        rotXL.set(Biquad::LP, 800, 0.707f, 0, sr); rotXR = rotXL;
        rotXHL.set(Biquad::HP, 800, 0.707f, 0, sr); rotXHR = rotXHL;
        ready = true;
        resetAlgo(algo);
    }

    void resetAlgo(int a) {
        algo = a;
        const AlgoDef& d = kAlgos[a];
        int rv = d.rev < sizeof(kRev) / sizeof(kRev[0]) ? d.rev : 0;
        plate.configure(kRev[rv].size);
        plate.clear(); er.clear();
        Delay* ds[] = {&ddlL, &ddlR, &mtap, &mdL, &mdR, &chL, &chR, &flL, &flR, &vibL, &vibR, &rotHL, &rotHR, &rotDL, &rotDR};
        for (Delay* x : ds) x->clear();
        pitchL.clear(); pitchR.clear();
        for (int i = 0; i < 4; i++) pv[i] = (float)i * 0.25f;
        ddlLpL = ddlLpR = mtapLp = mdLp = mdLpR = 0; ddlCurL = ddlCurR = mdCur = -1;
        memset(apStL, 0, sizeof(apStL)); memset(apStR, 0, sizeof(apStR));
        phFbL = phFbR = flFbL = flFbR = 0;
        for (Biquad& b : bq) b.clear();
        for (Biquad& b : lofi) b.clear();
        rotXL.clear(); rotXR.clear(); rotXHL.clear(); rotXHR.clear();
        svfL.clear(); svfR.clear(); for (SVF& f : fmt) f.clear();
        envIn.set(5, 150, sr); envDet.set(3, 120, sr); envTrig.set(1, 40, sr); envKey.set(1, 60, sr);
        envIn.clear(); envDet.clear(); envTrig.clear(); envKey.clear();
        gainSm = 1; gateGain = 0; trigEnv = 0; trigT = 1e9f; trigArmed = true; trigActive = false;
        formPos = formTarget = 0;
        rotSpdH = rotSpdD = 1;
    }

    inline float pick(int p, float fallback) const { return (own >> p) & 1u ? val[p] : fallback; }
    inline float rnd() { noise = noise * 1664525u + 1013904223u; return (float)(noise >> 8) / 16777216.f; }
    inline float lfoInc(float hz) const { return hz / sr; }

    // ============================================================ stages
    // Each stage processes L/R in place over n <= kChunk samples.

    void revParams(float rt60, float preMs) {
        const RevVoice& v = kRev[kAlgos[algo].rev];
        float damp = pick(P_DAMP, 35.f) / 99.f;
        bool a10 = kAlgos[algo].model == M_A10 && !strstr(kAlgos[algo].name, "44KHZ");
        float bwHz = a10 ? 9000.f : 12000.f;
        float bw = 1.f - expf(-kTwoPi * bwHz / sr);
        plate.setParams(rt60, damp * 0.85f, bw, v.diff, v.det, v.detRate, preMs, v.width);
    }
    inline void revRun(float inL, float inR, float& oL, float& oR) {
        const RevVoice& v = kRev[kAlgos[algo].rev];
        plate.run(inL, inR, oL, oR);
        if (v.er) {
            static const float roomMs[6] = {7, 11, 17, 23, 29, 37}, hallMs[6] = {13, 19, 27, 37, 47, 59};
            static const float g[6] = {0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f};
            static const float roomPan[6] = {-0.6f, 0.5f, -0.3f, 0.7f, -0.8f, 0.2f}, hallPan[6] = {-0.9f, 0.9f, -0.9f, 0.9f, -0.9f, 0.9f};
            float l, r;
            er.run((inL + inR) * 0.5f, v.er == 2 ? hallMs : roomMs, g, v.er == 2 ? hallPan : roomPan, 6, v.size, l, r);
            oL += l * v.erLvl; oR += r * v.erLvl;
        }
    }

    void stRev(float* L, float* R, int n) {
        revParams(pick(P_DECAY, 2.f), pick(P_PRE, 10.f));
        for (int i = 0; i < n; i++) revRun(L[i], R[i], L[i], R[i]);
    }

    // trigger detector shared by gated / reverse / non-linear reverbs; returns true on a new onset
    inline bool trigger(float x, float thrDb) {
        float e = envTrig.run(x);
        float thr = dbToLin(thrDb);
        bool onset = false;
        if (e > thr && trigArmed) { onset = true; trigArmed = false; }
        if (e < thr * 0.5f) trigArmed = true;
        return onset;
    }

    void stGatedRev(float* L, float* R, int n) {
        revParams(3.f, pick(P_PRE, 0.f));
        float hold = clampf(pick(P_DECAY, 0.35f), 0.03f, 2.f) * sr;
        float thr = dbToLin(pick(P_THRESH, -36.f));
        float atk = onePoleCoef(2, sr), rel = onePoleCoef(25, sr);
        for (int i = 0; i < n; i++) {
            float e = envTrig.run((L[i] + R[i]) * 0.5f);
            if (e > thr) trigT = 0; else trigT += 1;
            float target = trigT < hold ? 1.f : 0.f;
            gateGain += (target > gateGain ? atk : rel) * (target - gateGain);
            float l, r; revRun(L[i], R[i], l, r);
            L[i] = l * gateGain * 1.4f; R[i] = r * gateGain * 1.4f;
        }
    }

    void stReverse(float* L, float* R, int n) {
        revParams(3.f, 0.f);
        float ramp = clampf(pick(P_DECAY, 0.6f), 0.03f, 4.f) * sr;
        bool retrig = (kAlgos[algo].opt & 1) != 0;
        float thrDb = pick(P_THRESH, -30.f);
        float rel = onePoleCoef(20, sr);
        for (int i = 0; i < n; i++) {
            bool on = trigger((L[i] + R[i]) * 0.5f, thrDb);
            if (on && (retrig || !trigActive)) { trigActive = true; trigT = 0; }
            float g;
            if (trigActive) {
                trigT += 1;
                float t = trigT / ramp;
                if (t >= 1.f) { trigActive = false; g = 1.f; } else g = t * t;
                trigEnv = g;
            } else { trigEnv += rel * (0.f - trigEnv); g = trigEnv; }
            float l, r; revRun(L[i], R[i], l, r);
            L[i] = l * g * 1.6f; R[i] = r * g * 1.6f;
        }
    }

    void stNonLin(float* L, float* R, int n) {
        revParams(2.5f, pick(P_PRE, 0.f));
        float dur = clampf(pick(P_DECAY, 0.3f), 0.05f, 3.f) * sr;
        float narrow = (kAlgos[algo].opt & 1) ? 0.4f : 1.f;
        for (int i = 0; i < n; i++) {
            if (trigger((L[i] + R[i]) * 0.5f, -40.f)) trigT = 0; else trigT += 1;
            float t = trigT / dur, g = t < 0.7f ? 1.f : (t < 1.f ? (1.f - t) / 0.3f : 0.f);
            float l, r; revRun(L[i], R[i], l, r);
            float m = (l + r) * 0.5f, s = (l - r) * 0.5f * narrow;
            L[i] = (m + s) * g * 1.5f; R[i] = (m - s) * g * 1.5f;
        }
    }

    inline float fbDamp(float dampVal) const {             // damping 0..99 -> one-pole coefficient
        float fc = 16000.f * powf(2.f, -dampVal / 99.f * 6.f);
        return 1.f - expf(-kTwoPi * fc / sr);
    }

    void stDDL(float* L, float* R, int n, bool single) {
        float tL = pick(P_DELAY, 350.f) * 0.001f * sr;
        float ratio = single ? 0.25f + val[P_BAL] / 99.f * 1.75f : 0.75f;
        float tR = clampf(tL * ratio, 1.f, sr * 2.2f);
        float fb = pick(P_FB, 30.f) / 99.f * 0.97f, k = fbDamp(pick(P_DAMP, 30.f));
        if (ddlCurL < 0) { ddlCurL = tL; ddlCurR = tR; }
        double glide = onePoleCoef(60, sr);
        for (int i = 0; i < n; i++) {
            ddlCurL += glide * (tL - ddlCurL); ddlCurR += glide * (tR - ddlCurR);
            float yl = ddlL.get((float)ddlCurL), yr = ddlR.get((float)ddlCurR);
            ddlLpL += k * (yl - ddlLpL); ddlLpR += k * (yr - ddlLpR);
            ddlL.put(L[i] + fb * ddlLpL); ddlR.put(R[i] + fb * ddlLpR);
            L[i] = yl; R[i] = yr;
        }
    }

    void stMTap(float* L, float* R, int n) {
        float t = pick(P_DELAY, 600.f) * 0.001f * sr;
        float fb = pick(P_FB, 25.f) / 99.f * 0.95f, k = fbDamp(pick(P_DAMP, 30.f));
        static const float pan[4] = {-0.8f, 0.8f, -0.4f, 0.4f}, g[4] = {0.9f, 0.75f, 0.6f, 0.5f};
        for (int i = 0; i < n; i++) {
            float ol = 0, orr = 0, last = 0;
            for (int j = 0; j < 4; j++) {
                float v = mtap.get(fmaxf(1.f, t * (float)(j + 1) * 0.25f));
                ol += v * g[j] * (1.f - pan[j]) * 0.5f; orr += v * g[j] * (1.f + pan[j]) * 0.5f; last = v;
            }
            mtapLp += k * (last - mtapLp);
            mtap.put((L[i] + R[i]) * 0.5f + fb * mtapLp);
            L[i] = ol * 1.4f; R[i] = orr * 1.4f;
        }
    }

    void stModDly(float* L, float* R, int n) {
        float t = pick(P_DELAY, 250.f) * 0.001f * sr;
        float fb = pick(P_FB, 40.f) / 99.f * 0.95f;
        float inc = lfoInc(pick(P_RATE, 0.4f)), dep = pick(P_DEPTH, 30.f) / 99.f * 0.012f * sr;
        if (mdCur < 0) mdCur = t;
        double glide = onePoleCoef(60, sr); float k = fbDamp(30.f);
        for (int i = 0; i < n; i++) {
            mdCur += glide * (t - mdCur);
            ph[0] = wrap1(ph[0] + inc);
            float m = fastSin(ph[0]) * dep, mc = (float)mdCur;
            float yl = mdL.get(fmaxf(2.f, mc + dep + m)), yr = mdR.get(fmaxf(2.f, mc + dep - m));
            mdLp += k * (yl - mdLp); mdLpR += k * (yr - mdLpR);
            mdL.put(L[i] + fb * mdLp); mdR.put(R[i] + fb * mdLpR);
            L[i] = yl; R[i] = yr;
        }
    }

    int chorusVoices() const {
        uint8_t o = kAlgos[algo].opt;
        if (o >> 4) return o >> 4;
        return (o & 0xF) >= 2 ? (o & 0xF) : 4;
    }

    void stChorus(float* L, float* R, int n) {
        int N = chorusVoices(); if (N > 8) N = 8;
        float rate = pick(P_RATE, 0.6f), depMs = pick(P_DEPTH, 40.f) / 99.f * 6.f;
        float regen = pick(P_RESO, 10.f) / 99.f * 0.7f;
        float baseMs = N <= 2 ? 8.f : 12.f;
        int perSide = N / 2 > 0 ? N / 2 : 1;
        float norm = 1.f / (float)perSide;
        for (int i = 0; i < n; i++) {
            float wl = 0, wr = 0, f0l = 0, f0r = 0;
            for (int v = 0; v < N; v++) {
                ph[v] = wrap1(ph[v] + lfoInc(rate * (1.f + 0.071f * (float)v)) );
                float d = (baseMs + depMs * 0.5f * (1.f + fastSin(ph[v] + (float)v / (float)N))) * 0.001f * sr;
                if (v & 1) { float y = chR.get(d); wr += y; if (v == 1) f0r = y; }
                else       { float y = chL.get(d); wl += y; if (v == 0) f0l = y; }
            }
            chL.put(L[i] + regen * f0l); chR.put(R[i] + regen * f0r);
            L[i] = 0.5f * (L[i] + wl * norm); R[i] = 0.5f * (R[i] + wr * norm);
        }
    }

    void stFlanger(float* L, float* R, int n) {
        float rate = pick(P_RATE, 0.25f), oct = pick(P_DEPTH, 60.f) / 99.f * 2.5f;
        float fb = pick(P_RESO, 50.f) / 99.f * 0.9f;
        float centerMs = clampf(500.f / pick(P_FREQ, 900.f), 0.15f, 12.f);
        float inc = lfoInc(rate);
        for (int i = 0; i < n; i++) {
            ph[0] = wrap1(ph[0] + inc);
            float triL = 2.f * fabsf(2.f * ph[0] - 1.f) - 1.f, p2 = wrap1(ph[0] + 0.25f), triR = 2.f * fabsf(2.f * p2 - 1.f) - 1.f;
            float dl = clampf(centerMs * powf(2.f, oct * triL * 0.5f), 0.08f, 40.f) * 0.001f * sr;
            float dr = clampf(centerMs * powf(2.f, oct * triR * 0.5f), 0.08f, 40.f) * 0.001f * sr;
            float yl = flL.get(fmaxf(1.f, dl)), yr = flR.get(fmaxf(1.f, dr));
            flL.put(L[i] + fb * yl); flR.put(R[i] + fb * yr);
            L[i] = 0.5f * (L[i] + yl); R[i] = 0.5f * (R[i] + yr);
        }
    }

    void stPhaser(float* L, float* R, int n) {          // 12-pole (ASR-10 manual: "12-pole phase shifter")
        float rate = pick(P_RATE, 0.3f), oct = pick(P_DEPTH, 60.f) / 99.f * 3.f;
        float fb = pick(P_RESO, 40.f) / 99.f * 0.85f, fc = pick(P_FREQ, 700.f);
        float inc = lfoInc(rate);
        float aL = 0, aR = 0;
        for (int i = 0; i < n; i++) {
            ph[1] = wrap1(ph[1] + inc);
            if ((i & 3) == 0) {
                float fL = clampf(fc * powf(2.f, oct * 0.5f * fastSin(ph[1])), 30.f, sr * 0.4f);
                float fR = clampf(fc * powf(2.f, oct * 0.5f * fastSin(ph[1] + 0.25f)), 30.f, sr * 0.4f);
                float tl = tanf(kPi * fL / sr), tr = tanf(kPi * fR / sr);
                aL = (tl - 1.f) / (tl + 1.f); aR = (tr - 1.f) / (tr + 1.f);
            }
            float xl = L[i] + fb * phFbL, xr = R[i] + fb * phFbR;
            for (int s = 0; s < 12; s++) {
                float yl = aL * xl + apStL[s]; apStL[s] = xl - aL * yl; xl = yl;
                float yr = aR * xr + apStR[s]; apStR[s] = xr - aR * yr; xr = yr;
            }
            phFbL = xl; phFbR = xr;
            L[i] = 0.5f * (L[i] + xl); R[i] = 0.5f * (R[i] + xr);
        }
    }

    void stVibrato(float* L, float* R, int n) {
        float inc = lfoInc(pick(P_RATE, 5.f)), dep = pick(P_DEPTH, 30.f) / 99.f * 4.f;
        for (int i = 0; i < n; i++) {
            ph[2] = wrap1(ph[2] + inc);
            float d = (6.f + dep * fastSin(ph[2])) * 0.001f * sr, d2 = (6.f + dep * fastSin(ph[2] + 0.5f)) * 0.001f * sr;
            vibL.put(L[i]); vibR.put(R[i]);
            L[i] = vibL.get(d); R[i] = vibR.get(d2);
        }
    }

    void stTrem(float* L, float* R, int n) {
        float inc = lfoInc(pick(P_RATE, 4.f)), dep = pick(P_DEPTH, 60.f) / 99.f;
        for (int i = 0; i < n; i++) {
            ph[3] = wrap1(ph[3] + inc);
            L[i] *= 1.f - dep * (0.5f + 0.5f * fastSin(ph[3]));
            R[i] *= 1.f - dep * (0.5f + 0.5f * fastSin(ph[3] + 0.5f));
        }
    }

    void stRotary(float* L, float* R, int n) {
        float target = pick(P_RATE, 6.5f), dep = pick(P_DEPTH, 70.f) / 99.f;
        float drive = dbToLin(clampf(pick(P_DRIVE, 3.f), 0.f, 36.f));
        float aH = onePoleCoef(700, sr) * 0.5f, aD = onePoleCoef(3500, sr) * 0.5f;
        for (int i = 0; i < n; i++) {
            rotSpdH += aH * (target - rotSpdH); rotSpdD += aD * (target * 0.85f - rotSpdD);
            rotHorn = wrap1(rotHorn + rotSpdH / sr); rotDrum = wrap1(rotDrum + rotSpdD / sr);
            float x = fastTanh((L[i] + R[i]) * 0.5f * drive) / fmaxf(1.f, sqrtf(drive));
            float lo = rotXL.run(x), hi = rotXHL.run(x);
            float hs = fastSin(rotHorn), hc = fastSin(rotHorn + 0.25f), ds = fastSin(rotDrum), dc = fastSin(rotDrum + 0.25f);
            rotHL.put(hi); rotDL.put(lo);
            float hDl = (2.f + 1.2f * dep * (1.f + hs)) * 0.001f * sr, hDr = (2.f + 1.2f * dep * (1.f + hc)) * 0.001f * sr;
            float dDl = (2.f + 0.6f * dep * (1.f + ds)) * 0.001f * sr, dDr = (2.f + 0.6f * dep * (1.f + dc)) * 0.001f * sr;
            float hl = rotHL.get(hDl) * (1.f - 0.35f * dep * (0.5f + 0.5f * hs));
            float hr = rotHL.get(hDr) * (1.f - 0.35f * dep * (0.5f + 0.5f * hc));
            float dl = rotDL.get(dDl) * (1.f - 0.2f * dep * (0.5f + 0.5f * ds));
            float dr = rotDL.get(dDr) * (1.f - 0.2f * dep * (0.5f + 0.5f * dc));
            L[i] = (hl + dl) * 1.1f; R[i] = (hr + dr) * 1.1f;
        }
    }

    void stSpinner(float* L, float* R, int n) {
        float inc = lfoInc(pick(P_RATE, 1.5f)), dep = pick(P_DEPTH, 80.f) / 99.f;
        for (int i = 0; i < n; i++) {
            ph[4] = wrap1(ph[4] + inc);
            float pan = dep * fastSin(ph[4]), m = (L[i] + R[i]) * 0.5f;
            float s = (L[i] - R[i]) * 0.5f * (1.f - dep);
            L[i] = m * sqrtf(1.f - pan) + s; R[i] = m * sqrtf(1.f + pan) - s;
        }
    }

    static inline float curve(int c, float x) {
        switch (c) {
        case 1: return clampf(x, -1.f, 1.f);
        case 2: { float b = 0.25f; return fastTanh(x + b) - fastTanh(b); }
        case 3: { float s = x < 0 ? -1.f : 1.f; return s * (1.f - expf(-fabsf(x) * 1.5f)); }
        case 4: { float y = x; for (int k = 0; k < 4 && fabsf(y) > 1.f; k++) y = (y > 0 ? 2.f : -2.f) - y; return clampf(y, -1.f, 1.f); }
        default: return fastTanh(x);
        }
    }

    void stDist(float* L, float* R, int n) {
        int c = kAlgos[algo].opt & 0xF; if (c > 4) c = 0;
        float g = dbToLin(clampf(pick(P_DRIVE, 18.f), 0.f, 48.f));
        float normv = 0.5f / fmaxf(1e-4f, fabsf(curve(c, g * 0.5f)));
        float tone = pick(P_FREQ, 5500.f);
        bq[2].set(Biquad::LP, tone, 0.707f, 0, sr);
        for (int i = 0; i < n; i++) {
            float xl = curve(c, g * L[i]) * normv, xr = curve(c, g * R[i]) * normv;
            L[i] = xl; R[i] = xr;
        }
        // tone filter per channel (bq[2] left, bq[3] right)
        bq[3].b0 = bq[2].b0; bq[3].b1 = bq[2].b1; bq[3].b2 = bq[2].b2; bq[3].a1 = bq[2].a1; bq[3].a2 = bq[2].a2;
        for (int i = 0; i < n; i++) { L[i] = bq[2].run(L[i]); R[i] = bq[3].run(R[i]); }
    }

    void stVCF(float* L, float* R, int n) {
        float fc = pick(P_FREQ, 900.f), k = 2.f - 1.94f * pick(P_RESO, 60.f) / 99.f, amt = pick(P_DEPTH, 60.f) / 99.f * 4.f;
        for (int i = 0; i < n; i++) {
            float e = envIn.run((L[i] + R[i]) * 0.5f);
            float f = clampf(fc * powf(2.f, amt * fminf(e * 4.f, 1.5f)), 30.f, sr * 0.42f);
            float g = SVF::gOf(f, sr), bp, hp;
            L[i] = svfL.run(L[i], g, k, bp, hp); R[i] = svfR.run(R[i], g, k, bp, hp);
        }
    }

    void stWah(float* L, float* R, int n) {
        bool autoWah = (kAlgos[algo].opt & 1) != 0;
        float fc = pick(P_FREQ, 500.f), range = pick(P_DEPTH, 70.f) / 99.f * (autoWah ? 3.5f : 3.f);
        float k = 1.f / (1.f + pick(P_RESO, 50.f) / 99.f * 9.f);
        for (int i = 0; i < n; i++) {
            float e = envIn.run((L[i] + R[i]) * 0.5f);
            float f = clampf(fc * powf(2.f, range * fminf(e * 5.f, 1.f)), 40.f, sr * 0.42f);
            float g = SVF::gOf(f, sr), bpl, bpr, hp;
            svfL.run(L[i], g, k, bpl, hp); svfR.run(R[i], g, k, bpr, hp);
            L[i] = bpl * k * 2.5f + L[i] * 0.1f; R[i] = bpr * k * 2.5f + R[i] * 0.1f;
        }
    }

    void stAmp(float* L, float* R, int n) {
        int model = kAlgos[algo].opt & 0xF;
        float g = dbToLin(clampf(pick(P_DRIVE, 30.f), 0.f, 48.f)), thr = dbToLin(pick(P_THRESH, -60.f));
        // bq: 0/1 HP, 2/3 pre bell, 4/5 post1, 6/7 post2, 8/9 LPF
        Biquad hp, pre, p1, p2, lp;
        hp.set(Biquad::HP, 80, 0.707f, 0, sr);
        pre.set(Biquad::PEAK, pick(P_FREQ, 800.f), 1.0f, model == 2 ? 4.f : 8.f, sr);
        switch (model) {
        case 2:  p1.set(Biquad::PEAK, 800, 0.8f, -4.f, sr); p2.set(Biquad::LSHELF, 120, 0.7f, 3.f, sr); lp.set(Biquad::LP, 4000, 0.7f, 0, sr); break;
        case 3:  p1.set(Biquad::HSHELF, 3000, 0.7f, 6.f, sr); p2.set(Biquad::PEAK, 1200, 1.f, 2.f, sr); lp.set(Biquad::LP, 6500, 0.7f, 0, sr); break;
        case 4:  p1.set(Biquad::PEAK, 650, 0.9f, 3.f, sr); p2.set(Biquad::PEAK, 2800, 1.2f, 3.f, sr); lp.set(Biquad::LP, 5200, 0.7f, 0, sr); break;
        default: p1.set(Biquad::PEAK, 500, 0.9f, 3.f, sr); p2.set(Biquad::PEAK, 2500, 1.2f, 4.f, sr); lp.set(Biquad::LP, 5000, 0.7f, 0, sr); break;
        }
        Biquad* st[5] = {&hp, &pre, &p1, &p2, &lp};
        for (int s = 0; s < 5; s++) {             // copy coefficients, keep per-channel state in bq[]
            for (int ch = 0; ch < 2; ch++) {
                Biquad& b = bq[s * 2 + ch];
                b.b0 = st[s]->b0; b.b1 = st[s]->b1; b.b2 = st[s]->b2; b.a1 = st[s]->a1; b.a2 = st[s]->a2;
            }
        }
        float normv = 0.5f / fmaxf(1e-4f, fabsf(fastTanh(fastTanh(g * 0.5f + 0.2f) - fastTanh(0.2f)) * 1.2f));
        float atk = onePoleCoef(1, sr), rel = onePoleCoef(120, sr);
        for (int i = 0; i < n; i++) {
            float e = envDet.run((L[i] + R[i]) * 0.5f);
            float sus = 1.f;
            if (model == 3 && e > thr) sus = clampf(powf(0.25f / fmaxf(e, 1e-4f), 0.4f), 1.f, 8.f);   // inverse expander (sustain)
            float tg = e > thr ? 1.f : 0.f;
            gateGain += (tg > gateGain ? atk : rel) * (tg - gateGain);
            for (int ch = 0; ch < 2; ch++) {
                float* X = ch ? R : L;
                float x = bq[2 + ch].run(bq[0 + ch].run(X[i])) * sus;
                x = fastTanh(g * x + 0.2f) - fastTanh(0.2f);          // asymmetric "tube" stage
                x = fastTanh(x * 1.2f);
                x = bq[8 + ch].run(bq[6 + ch].run(bq[4 + ch].run(x)));
                X[i] = x * normv * gateGain;
            }
        }
    }

    void cabFilters(Biquad* f) {    // 4 filters: HP, presence peak, LP, LP
        f[0].set(Biquad::HP, 90, 0.7f, 0, sr);
        f[1].set(Biquad::PEAK, 2200, 1.2f, 5.f, sr);
        f[2].set(Biquad::LP, 4500, 0.9f, 0, sr);
        f[3].set(Biquad::LP, 6000, 0.6f, 0, sr);
    }
    void stCab(float* L, float* R, int n, bool tunable) {
        Biquad c[5]; cabFilters(c);
        if (tunable) c[4].set(Biquad::PEAK, pick(P_FREQ, 1500.f), 0.5f + pick(P_RESO, 30.f) / 99.f * 9.5f, clampf(pick(P_DRIVE, 6.f), -24.f, 24.f), sr);
        int ns = tunable ? 5 : 4;
        for (int s = 0; s < ns; s++) for (int ch = 0; ch < 2; ch++) {
            Biquad& b = bq[s * 2 + ch]; b.b0 = c[s].b0; b.b1 = c[s].b1; b.b2 = c[s].b2; b.a1 = c[s].a1; b.a2 = c[s].a2;
        }
        for (int i = 0; i < n; i++) for (int ch = 0; ch < 2; ch++) {
            float* X = ch ? R : L; float x = X[i];
            for (int s = 0; s < ns; s++) x = bq[s * 2 + ch].run(x);
            X[i] = x * 1.2f;
        }
    }

    void stEQ(float* L, float* R, int n, bool param) {
        Biquad c;
        float q = param ? 0.3f + pick(P_RESO, 30.f) / 99.f * 12.f : 0.8f;
        c.set(Biquad::PEAK, pick(P_FREQ, 1000.f), q, clampf(pick(P_DRIVE, 0.f), -24.f, 24.f), sr);
        for (int ch = 0; ch < 2; ch++) { Biquad& b = bq[10 + ch]; b.b0 = c.b0; b.b1 = c.b1; b.b2 = c.b2; b.a1 = c.a1; b.a2 = c.a2; }
        for (int i = 0; i < n; i++) { L[i] = bq[10].run(L[i]); R[i] = bq[11].run(R[i]); }
    }

    inline float ratioOf(float reso) const { return reso >= 98.5f ? 1000.f : 1.f + powf(reso / 99.f, 2.f) * 19.f; }

    void stComp(float* L, float* R, int n, bool ducker) {
        float thr = pick(P_THRESH, -20.f), ratio = ratioOf(pick(P_RESO, 40.f));
        float makeup = ducker ? 0.f : -thr * (1.f - 1.f / ratio) * 0.5f;
        float rel = onePoleCoef(ducker ? 400.f : 120.f, sr), atk = onePoleCoef(3, sr);
        for (int i = 0; i < n; i++) {
            float lvl = linToDb(fmaxf(fabsf(L[i]), fabsf(R[i])) + 1e-9f);
            float over = lvl - thr, gr = 0;
            if (over > 3.f) gr = over * (1.f - 1.f / ratio);
            else if (over > -3.f) { float t = (over + 3.f) / 6.f; gr = t * t * 3.f * (1.f - 1.f / ratio); }
            float target = dbToLin(makeup - gr);
            gainSm += (target < gainSm ? atk : rel) * (target - gainSm);
            L[i] *= gainSm; R[i] *= gainSm;
        }
    }

    void stGate(float* L, float* R, int n) {
        float thr = dbToLin(pick(P_THRESH, -55.f));
        float atk = onePoleCoef(1, sr), rel = onePoleCoef(120, sr);
        for (int i = 0; i < n; i++) {
            float e = envKey.run(fmaxf(fabsf(L[i]), fabsf(R[i])));
            float tg = e > thr ? 1.f : (e < thr * 0.6f ? 0.f : gateGain);
            gateGain += (tg > gateGain ? atk : rel) * (tg - gateGain);
            L[i] *= gateGain; R[i] *= gateGain;
        }
    }

    void stExpander(float* L, float* R, int n, int kind) {   // 0 downward, 1 inverse (upward), 2 keyed
        float thr = pick(P_THRESH, -40.f), ratio = ratioOf(pick(P_RESO, 40.f));
        Biquad kh;
        if (kind == 2) { kh.set(Biquad::HP, pick(P_FREQ, 150.f), 0.707f, 0, sr); bq[0].b0 = kh.b0; bq[0].b1 = kh.b1; bq[0].b2 = kh.b2; bq[0].a1 = kh.a1; bq[0].a2 = kh.a2; }
        float atk = onePoleCoef(1, sr), rel = onePoleCoef(150, sr);
        for (int i = 0; i < n; i++) {
            float key = (L[i] + R[i]) * 0.5f;
            if (kind == 2) key = bq[0].run(key);
            float e = envKey.run(key), lvl = linToDb(e + 1e-9f), gdb = 0;
            if (kind == 1) {
                // raise quiet passages toward the threshold, never past it
                if (lvl < thr && lvl > -70.f) gdb = fminf(fminf((thr - lvl) * (1.f - 1.f / ratio), 24.f), thr - lvl);
            } else if (lvl < thr) gdb = -fminf((thr - lvl) * (ratio - 1.f), 90.f);
            float target = dbToLin(gdb);
            if (kind == 1) gainSm += (target < gainSm ? onePoleCoef(0.3f, sr) : rel) * (target - gainSm);
            else gainSm += (target > gainSm ? atk : rel) * (target - gainSm);
            L[i] *= gainSm; R[i] *= gainSm;
        }
    }

    void stDeEss(float* L, float* R, int n) {
        Biquad h; h.set(Biquad::HP, pick(P_FREQ, 5500.f), 0.707f, 0, sr);
        for (int ch = 0; ch < 2; ch++) { Biquad& b = bq[ch]; b.b0 = h.b0; b.b1 = h.b1; b.b2 = h.b2; b.a1 = h.a1; b.a2 = h.a2; }
        float thr = pick(P_THRESH, -28.f), atk = onePoleCoef(1, sr), rel = onePoleCoef(60, sr);
        for (int i = 0; i < n; i++) {
            float hl = bq[0].run(L[i]), hr = bq[1].run(R[i]);
            float lvl = linToDb(envDet.run(fmaxf(fabsf(hl), fabsf(hr))) + 1e-9f);
            float gr = lvl > thr ? (lvl - thr) * 0.75f : 0.f;
            float target = dbToLin(-gr);
            gainSm += (target < gainSm ? atk : rel) * (target - gainSm);
            L[i] = L[i] - hl + hl * gainSm; R[i] = R[i] - hr + hr * gainSm;
        }
    }

    void stPitch(float* L, float* R, int n, bool detuner) {
        float win = (float)(kAlgos[algo].opt ? kAlgos[algo].opt : 40);
        float cents = pick(P_DEPTH, 10.f) / 99.f * 50.f;
        float r1, r2;
        if (detuner) { r1 = powf(2.f, cents / 1200.f); r2 = powf(2.f, -cents / 1200.f); }
        else { float st = pick(P_PITCH, 0.f); r1 = powf(2.f, st / 12.f); r2 = powf(2.f, (st + cents / 100.f) / 12.f); }
        for (int i = 0; i < n; i++) {
            pitchL.push((L[i] + R[i]) * 0.5f);
            float v1 = pitchL.read(pv[0], r1, win), v2 = pitchL.read(pv[1], r2, win * 1.13f);
            L[i] = v1 * 0.8f + v2 * 0.25f; R[i] = v2 * 0.8f + v1 * 0.25f;
        }
    }

    void formantSet(float pos, float shift, float* f, float* g) const {
        static const float F[5][3] = {{730, 1090, 2440}, {530, 1840, 2480}, {270, 2290, 3010}, {570, 840, 2410}, {300, 870, 2240}};
        pos = clampf(pos, 0.f, 3.999f);
        int a = (int)pos; float t = pos - (float)a;
        for (int k = 0; k < 3; k++) f[k] = (F[a][k] + (F[a + 1][k] - F[a][k]) * t) * shift;
        g[0] = 1.f; g[1] = 0.6f; g[2] = 0.35f;
    }
    void stFormant(float* L, float* R, int n, bool chatter) {
        float rate = pick(P_RATE, chatter ? 6.f : 0.4f), dep = pick(P_DEPTH, 70.f) / 99.f;
        float shift = chatter ? 1.f : clampf(pick(P_FREQ, 1000.f) / 1000.f, 0.5f, 2.f);
        float slew = onePoleCoef(chatter ? 25.f : 5.f, sr);
        float f[3], g[3]; const float k = 0.12f;
        for (int i = 0; i < n; i++) {
            float x = (L[i] + R[i]) * 0.5f;
            if (chatter) {
                ph[5] += lfoInc(rate);
                float e = envIn.run(x);
                if (ph[5] >= 1.f) { ph[5] -= 1.f; if (e > 0.01f) formTarget = 2.f + (rnd() * 4.f - 2.f) * dep; }
            } else {
                ph[5] = wrap1(ph[5] + lfoInc(rate));
                formTarget = 2.f + 2.f * dep * fastSin(ph[5]);
            }
            formPos += slew * (formTarget - formPos);
            if ((i & 7) == 0 || i == 0) formantSet(formPos, shift, f, g);
            float yl = 0, yr = 0;
            for (int b = 0; b < 3; b++) {
                float gg = SVF::gOf(f[b], sr), bp, hp;
                fmt[b].run(x, gg, k, bp, hp); yl += bp * k * g[b];
                fmt[b + 3].run(x, gg, k * 1.1f, bp, hp); yr += bp * k * 1.1f * g[b];
            }
            L[i] = yl * 2.2f; R[i] = yr * 2.2f;
        }
    }

    void stRumble(float* L, float* R, int n) {
        float fc = pick(P_FREQ, 60.f);
        Biquad c[3];
        c[0].set(Biquad::HP, fc, 0.5412f, 0, sr); c[1].set(Biquad::HP, fc, 1.3066f, 0, sr); c[2].set(Biquad::LP, 12000.f, 0.707f, 0, sr);
        for (int s = 0; s < 3; s++) for (int ch = 0; ch < 2; ch++) {
            Biquad& b = bq[s * 2 + ch]; b.b0 = c[s].b0; b.b1 = c[s].b1; b.b2 = c[s].b2; b.a1 = c[s].a1; b.a2 = c[s].a2;
        }
        for (int i = 0; i < n; i++) for (int ch = 0; ch < 2; ch++) {
            float* X = ch ? R : L; float x = X[i];
            for (int s = 0; s < 3; s++) x = bq[s * 2 + ch].run(x);
            X[i] = x;
        }
    }

    void stExciter(float* L, float* R, int n) {   // "VAN DER POL": synthetic upper harmonics added to the input
        Biquad h; h.set(Biquad::HP, pick(P_FREQ, 3000.f), 0.707f, 0, sr);
        for (int s = 0; s < 4; s++) { Biquad& b = bq[s]; b.b0 = h.b0; b.b1 = h.b1; b.b2 = h.b2; b.a1 = h.a1; b.a2 = h.a2; }
        float amt = clampf(pick(P_DRIVE, 6.f), 0.f, 24.f) / 24.f;
        for (int i = 0; i < n; i++) for (int ch = 0; ch < 2; ch++) {
            float* X = ch ? R : L;
            float hf = bq[ch].run(X[i]);
            float hgen = fastTanh(3.f * hf) * 0.5f + 1.5f * hf * fabsf(hf);
            X[i] = X[i] + amt * bq[2 + ch].run(hgen);
        }
    }

    // ============================================================ chain
    void runStage(uint8_t s, float* L, float* R, int n, bool single) {
        switch (s) {
        case S_REV: stRev(L, R, n); break;
        case S_GATEDREV: stGatedRev(L, R, n); break;
        case S_REVERSE: stReverse(L, R, n); break;
        case S_NONLIN: stNonLin(L, R, n); break;
        case S_DDL: stDDL(L, R, n, single); break;
        case S_MTAP: stMTap(L, R, n); break;
        case S_MODDLY: stModDly(L, R, n); break;
        case S_CHORUS: stChorus(L, R, n); break;
        case S_FLANGER: stFlanger(L, R, n); break;
        case S_PHASER: stPhaser(L, R, n); break;
        case S_VIBRATO: stVibrato(L, R, n); break;
        case S_TREM: stTrem(L, R, n); break;
        case S_ROTARY: stRotary(L, R, n); break;
        case S_SPINNER: stSpinner(L, R, n); break;
        case S_DIST: stDist(L, R, n); break;
        case S_VCF: stVCF(L, R, n); break;
        case S_WAH: stWah(L, R, n); break;
        case S_AMP: stAmp(L, R, n); break;
        case S_CAB: stCab(L, R, n, false); break;
        case S_TUNSPKR: stCab(L, R, n, true); break;
        case S_EQ: stEQ(L, R, n, false); break;
        case S_PEQ: stEQ(L, R, n, true); break;
        case S_COMP: stComp(L, R, n, false); break;
        case S_DUCK: stComp(L, R, n, true); break;
        case S_GATE: stGate(L, R, n); break;
        case S_EXP: stExpander(L, R, n, 0); break;
        case S_INVEXP: stExpander(L, R, n, 1); break;
        case S_KEYEXP: stExpander(L, R, n, 2); break;
        case S_DEESS: stDeEss(L, R, n); break;
        case S_PITCH: stPitch(L, R, n, false); break;
        case S_DETUNE: stPitch(L, R, n, true); break;
        case S_FORMANT: stFormant(L, R, n, false); break;
        case S_CHATTER: stFormant(L, R, n, true); break;
        case S_RUMBLE: stRumble(L, R, n); break;
        case S_EXCITER: stExciter(L, R, n); break;
        default: break;
        }
    }

    void runChain(float* L, float* R, int n) {
        const AlgoDef& d = kAlgos[algo];
        const AlgoInfo& in = info[algo];
        int len = chainLen(d);
        float bal = val[P_BAL] / 99.f;
        for (int i = 0; i < len; i++) {
            own = in.own[i];
            if (in.balChain && isAdditive(d.st[i])) {
                memcpy(tmpL, L, sizeof(float) * (size_t)n); memcpy(tmpR, R, sizeof(float) * (size_t)n);
                runStage(d.st[i], tmpL, tmpR, n, false);
                for (int k = 0; k < n; k++) { L[k] += bal * tmpL[k]; R[k] += bal * tmpR[k]; }
            } else runStage(d.st[i], L, R, n, len == 1);
        }
        bool lofi30 = d.model == M_A10 && !strstr(d.name, "44KHZ") && sr > 32000.f;
        if (lofi30) for (int k = 0; k < n; k++) {      // ASR-10 effects run at the 30 kHz system rate
            L[k] = lofi[1].run(lofi[0].run(L[k])); R[k] = lofi[3].run(lofi[2].run(R[k]));
        }
    }

    // target: normalized parameter values set by the host
    void process(const float* inL, const float* inR, float* outL, float* outR, int n, const float* target) {
        if (!ready) { if (outL != inL) memcpy(outL, inL, sizeof(float) * (size_t)n); if (outR != inR) memcpy(outR, inR, sizeof(float) * (size_t)n); return; }
        int want = algoFromNorm(target[P_ALGO]);
        if (want != algo && fadeDir == 0) { pending = want; fadeDir = -1; }
        float bypTarget = target[P_BYPASS] >= 0.5f ? 1.f : 0.f;
        int pos = 0;
        while (pos < n) {
            int m = n - pos < kChunk ? n - pos : kChunk;
            // parameter smoothing (per chunk)
            float a = snap ? 1.f : 1.f - expf(-(float)m / (0.02f * sr));
            snap = false;
            for (int p = P_MIX; p < P_BYPASS; p++) {
                float t = toReal(p, target[p]);
                val[p] += a * (t - val[p]);
            }
            float gTarget = dbToLin(toReal(P_INPUT, target[P_INPUT]));
            float dry[2][kChunk];
            float pk[4] = {0, 0, 0, 0};
            for (int k = 0; k < m; k++) {
                inGain += 0.002f * (gTarget - inGain);
                float xl = inL[pos + k] * inGain, xr = inR[pos + k] * inGain;
                pk[0] = fmaxf(pk[0], fabsf(xl)); pk[1] = fmaxf(pk[1], fabsf(xr));
                dry[0][k] = xl; dry[1][k] = xr; wetL[k] = xl; wetR[k] = xr;
            }
            runChain(wetL, wetR, m);
            float mix = val[P_MIX] * 0.01f, outG = dbToLin(val[P_OUT]);
            float fstep = (float)1 / (0.012f * sr);
            for (int k = 0; k < m; k++) {
                if (fadeDir < 0) { fade -= fstep; if (fade <= 0) { fade = 0; resetAlgo(pending); fadeDir = 1; snap = true; } }
                else if (fadeDir > 0) { fade += fstep; if (fade >= 1) { fade = 1; fadeDir = 0; } }
                bypassMix += (bypTarget - bypassMix) * 0.002f;
                float wl = wetL[k], wr = wetR[k];
                if (!(fabsf(wl) < 1e4f) || !(fabsf(wr) < 1e4f)) { wl = wr = 0; resetAlgo(algo); }   // safety net
                float yl = (dry[0][k] * (1.f - mix) + wl * mix * fade) * outG;
                float yr = (dry[1][k] * (1.f - mix) + wr * mix * fade) * outG;
                float rl = inL[pos + k], rr = inR[pos + k];          // bypass = untouched input, before the trim
                outL[pos + k] = yl + (rl - yl) * bypassMix;
                outR[pos + k] = yr + (rr - yr) * bypassMix;
                pk[2] = fmaxf(pk[2], fabsf(outL[pos + k])); pk[3] = fmaxf(pk[3], fabsf(outR[pos + k]));
            }
            updateMeters(pk, m);
            pos += m;
        }
    }

    void updateMeters(const float* pk, int m) {
        float fall = expf(-(float)m / (0.3f * sr)), hold = 1.5f * sr;
        for (int c = 0; c < 4; c++) {
            meter[c] = pk[c] > meter[c] ? pk[c] : meter[c] * fall;
            if (pk[c] >= 0.999f) clipHold[c] = hold; else if (clipHold[c] > 0) clipHold[c] -= (float)m;
            float db = linToDb(meter[c] + 1e-9f);
            float n = (db - kMeterLoDb) / (kMeterHiDb - kMeterLoDb);
            n = n < 0 ? 0 : (n > 0.97f ? 0.97f : n);         // top frame is reserved for the clip light
            meterNorm[c] = clipHold[c] > 0 ? 1.f : n;
        }
    }
};

} // namespace rsfx
