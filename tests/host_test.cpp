// Offline host test: loads the plugin .so the way a VST2 host does and stress-tests every algorithm.
#include "../src/vst2.h"
#include "../src/algos.h"
#include <dlfcn.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vector>

static intptr_t hostCb(AEffect*, int32_t op, int32_t, intptr_t, void*, float) { return op == audioMasterVersion ? 2400 : 0; }
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

struct Host {
    AEffect* fx; float sr;
    std::vector<float> inL, inR, outL, outR;
    intptr_t d(int32_t op, int32_t i = 0, intptr_t v = 0, void* p = nullptr, float o = 0) { return fx->dispatcher(fx, op, i, v, p, o); }
    void run(const float* l, const float* r, size_t n, float* ol, float* orr, int block = 256) {
        for (size_t pos = 0; pos < n; pos += block) {
            int m = (int)((n - pos) < (size_t)block ? n - pos : block);
            float* in[2] = {(float*)l + pos, (float*)r + pos}; float* out[2] = {ol + pos, orr + pos};
            fx->processReplacing(fx, in, out, m);
        }
    }
};

static double now() { timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

int main(int argc, char** argv) {
    const char* path = argc > 1 ? argv[1] : "./rsfx.so";
    void* h = dlopen(path, RTLD_NOW);
    if (!h) { printf("dlopen: %s\n", dlerror()); return 1; }
    typedef AEffect* (*Entry)(audioMasterCallback);
    Entry entry = (Entry)dlsym(h, "VSTPluginMain");
    CHECK(entry && dlsym(h, "main"), "entry points missing");
    AEffect* fx = entry(hostCb);
    CHECK(fx && fx->magic == kEffectMagic, "bad AEffect");
    CHECK(fx->numInputs == 2 && fx->numOutputs == 2, "io");
    CHECK(fx->numParams == rsfx::NPARAMS, "params");
    CHECK(fx->numPrograms == rsfx::NALGOS, "programs %d", fx->numPrograms);
    Host H{fx, 48000.f, {}, {}, {}, {}};
    H.d(effOpen); H.d(effSetSampleRate, 0, 0, nullptr, H.sr); H.d(effSetBlockSize, 0, 256); H.d(effMainsChanged, 0, 1);
    CHECK(H.d(effGetPlugCategory) == kPlugCategEffect, "category");

    // names
    char buf[256];
    for (int i = 0; i < rsfx::NALGOS; i++) {
        memset(buf, 'X', sizeof buf);
        H.d(effGetProgramNameIndexed, i, 0, buf);
        CHECK(strlen(buf) < kVstMaxProgNameLen, "program name too long: %s", buf);
    }
    for (int p = 0; p < rsfx::NPARAMS; p++) { memset(buf, 'X', sizeof buf); H.d(effGetParamName, p, 0, buf); CHECK(strlen(buf) < 24, "param name"); }

    const size_t N = (size_t)(H.sr * 4);
    std::vector<float> l(N), r(N), ol(N), orr(N);
    unsigned seed = 1;
    auto rnd = [&]() { seed = seed * 1664525u + 1013904223u; return (float)(seed >> 8) / 16777216.f * 2.f - 1.f; };
    // test signal: drum-like hits, then a noise burst, sine, then silence (denormal check)
    for (size_t i = 0; i < N; i++) {
        float t = (float)i / H.sr, x = 0;
        float ht = fmodf(t, 0.5f); if (t < 1.5f) x += expf(-ht * 30.f) * sinf(6.283f * 90.f * ht) * 0.8f + expf(-ht * 60.f) * rnd() * 0.3f;
        if (t >= 1.5f && t < 2.0f) x += rnd() * 0.5f;
        if (t >= 2.0f && t < 2.5f) x += 0.5f * sinf(6.283f * 440.f * t);
        l[i] = x; r[i] = i > 10 ? l[i - 10] * 0.9f : 0;
    }

    printf("%-4s %-18s %8s %8s %8s %8s %8s\n", "#", "algorithm", "peak", "rms", "diff", "tail", "xRT");
    double worst = 0; int worstA = 0;
    for (int a = 0; a < rsfx::NALGOS; a++) {
        H.d(effSetProgram, 0, a);
        // let the algorithm switch complete
        std::vector<float> z(4096, 0.f), zo(4096), zo2(4096);
        H.run(z.data(), z.data(), z.size(), zo.data(), zo2.data());
        double t0 = now();
        H.run(l.data(), r.data(), N, ol.data(), orr.data());
        double dt = now() - t0, xrt = dt / (N / H.sr);
        if (xrt > worst) { worst = xrt; worstA = a; }
        double peak = 0, rms = 0, diff = 0, tail = 0; bool finite = true;
        for (size_t i = 0; i < N; i++) {
            float a1 = ol[i], b1 = orr[i];
            if (!isfinite(a1) || !isfinite(b1)) finite = false;
            peak = fmax(peak, fmax(fabs(a1), fabs(b1)));
            rms += a1 * a1 + b1 * b1;
            diff += fabs(a1 - l[i]) + fabs(b1 - r[i]);
            if (i > (size_t)(H.sr * 3.8f)) tail = fmax(tail, fabs(a1));
        }
        rms = sqrt(rms / (2.0 * N)); diff /= (2.0 * N);
        buf[0] = 0; H.d(effGetProgramName, 0, 0, buf);
        printf("%-4d %-18s %8.3f %8.4f %8.4f %8.2g %8.4f\n", a, buf, peak, rms, diff, tail, xrt);
        CHECK(finite, "%s: non-finite output", buf);
        CHECK(peak < 4.0, "%s: peak %.2f", buf, peak);
        CHECK(diff > 1e-3, "%s: output identical to input (effect inactive?)", buf);
        CHECK(rms > 1e-3, "%s: silent", buf);
        // display strings must fit
        for (int p = 0; p < rsfx::NPARAMS; p++) { memset(buf, 'X', sizeof buf); H.d(effGetParamDisplay, p, 0, buf); CHECK(strlen(buf) < 24, "display %d", p); }
    }
    printf("slowest: algo %d at %.4f x realtime (x86, -O2)\n", worstA, worst);

    // parameter extremes: every algorithm with random parameter sets, including 0 and 1
    for (int a = 0; a < rsfx::NALGOS; a++) {
        H.d(effSetProgram, 0, a);
        for (int trial = 0; trial < 6; trial++) {
            for (int p = 1; p < rsfx::P_BYPASS; p++) {
                float v = trial == 0 ? 0.f : trial == 1 ? 1.f : (rnd() + 1.f) * 0.5f;
                fx->setParameter(fx, p, v);
            }
            size_t n2 = (size_t)(H.sr * 0.5f);
            H.run(l.data(), r.data(), n2, ol.data(), orr.data(), 64 + trial * 97);
            double peak = 0; bool finite = true;
            for (size_t i = 0; i < n2; i++) { if (!isfinite(ol[i]) || !isfinite(orr[i])) finite = false; peak = fmax(peak, fmax(fabs(ol[i]), fabs(orr[i]))); }
            buf[0] = 0; H.d(effGetProgramNameIndexed, a, 0, buf);
            CHECK(finite, "%s extreme trial %d: non-finite", buf, trial);
            CHECK(peak < 30.0, "%s extreme trial %d: peak %.1f", buf, trial, peak);
        }
    }

    // algorithm switching while audio runs (Q-Link sweep on the Algorithm knob)
    H.d(effSetProgram, 0, 0);
    for (int step = 0; step < 300; step++) {
        fx->setParameter(fx, rsfx::P_ALGO, (float)(step % rsfx::NALGOS) / (float)(rsfx::NALGOS - 1));
        H.run(l.data() + step * 128, r.data() + step * 128, 128, ol.data(), orr.data(), 128);
        for (int i = 0; i < 128; i++) CHECK(isfinite(ol[i]) && fabs(ol[i]) < 30, "switch step %d", step);
    }
    // selecting an algorithm via the knob loads its defaults
    fx->setParameter(fx, rsfx::P_ALGO, 3.f / (float)(rsfx::NALGOS - 1));
    CHECK(fabsf(fx->getParameter(fx, rsfx::P_DELAY) - rsfx::toNorm(rsfx::P_DELAY, 375)) < 1e-4, "algo defaults not loaded");

    // bypass passes input through
    H.d(effSetProgram, 0, 0);
    fx->setParameter(fx, rsfx::P_BYPASS, 1.f);
    H.run(l.data(), r.data(), N, ol.data(), orr.data());
    double bd = 0; for (size_t i = N / 2; i < N; i++) bd = fmax(bd, fabs(ol[i] - l[i]));
    CHECK(bd < 1e-3, "bypass not transparent (%.4f)", bd);
    fx->setParameter(fx, rsfx::P_BYPASS, 0.f);

    // reverb decay sanity: measured RT60 vs. Decay setting for HALL REVERB
    for (float rt : {1.0f, 2.5f, 5.0f}) {
        H.d(effSetProgram, 0, 1);       // 44KHZ REVERB (no 30k band limit)
        fx->setParameter(fx, rsfx::P_MIX, 1.f);
        fx->setParameter(fx, rsfx::P_DECAY, rsfx::toNorm(rsfx::P_DECAY, rt));
        fx->setParameter(fx, rsfx::P_PRE, 0.f);
        fx->setParameter(fx, rsfx::P_DAMP, 0.f);
        size_t n3 = (size_t)(H.sr * (rt * 1.6f + 1.f));
        std::vector<float> il(n3, 0.f), o1(n3), o2(n3), zz(48000, 0.f), zo(48000);
        H.run(zz.data(), zz.data(), zz.size(), zo.data(), zo.data());
        for (int i = 0; i < 480; i++) il[i] = (i % 7 == 0) ? 0.5f : -0.3f;     // short broadband burst
        H.run(il.data(), il.data(), n3, o1.data(), o2.data());
        // Schroeder backward integration, fit -5..-25 dB, extrapolate to -60
        std::vector<double> e(n3); double acc = 0;
        for (size_t i = n3; i-- > 0;) { acc += o1[i] * o1[i] + o2[i] * o2[i]; e[i] = acc; }
        double t5 = -1, t25 = -1;
        for (size_t i = 0; i < n3; i++) {
            double db = 10 * log10(e[i] / e[0] + 1e-30);
            if (t5 < 0 && db < -5) t5 = i / H.sr;
            if (t25 < 0 && db < -25) { t25 = i / H.sr; break; }
        }
        double meas = (t25 - t5) * 3.0;
        printf("RT60 target %.1fs measured %.2fs\n", rt, meas);
        CHECK(meas > rt * 0.6 && meas < rt * 1.5, "RT60 off: target %.1f measured %.2f", rt, meas);
    }

    // denormal / silence cost: long silence after signal must not get slower
    H.d(effSetProgram, 0, 17);
    H.run(l.data(), r.data(), N, ol.data(), orr.data());
    std::vector<float> sil((size_t)H.sr * 20, 0.f), so((size_t)H.sr * 20);
    double t0 = now(); H.run(sil.data(), sil.data(), sil.size(), so.data(), so.data()); double ts = now() - t0;
    printf("20 s of silence through LARGE PLATE: %.3f s CPU\n", ts);

    // ---- v1.1 skin parameters
    using namespace rsfx;
    for (int a = 0; a < NALGOS; a++) {
        fx->setParameter(fx, P_ALGO, (float)a / (float)(NALGOS - 1));
        buf[0] = 0; H.d(effGetParamDisplay, P_CHAIN, 0, buf);
        CHECK(strlen(buf) > 1 && strlen(buf) < 24, "algo %d chain text '%s'", a, buf);
        int lit = 0; for (int m = 0; m < 4; m++) lit += fx->getParameter(fx, P_LED_REV + m) > 0.5f;
        CHECK(lit >= 1, "algo %d lights no module", a);
        CHECK(fabsf(fx->getParameter(fx, P_CHAIN) - (float)a / (float)(NALGOS - 1)) < 1e-6f, "chain value");
    }
    fx->setParameter(fx, P_ALGO, 0.f);
    H.d(effGetParamDisplay, P_CHAIN, 0, buf); CHECK(!strcmp(buf, "REVERB"), "chain for HALL REVERB: %s", buf);
    CHECK(fx->getParameter(fx, P_LED_REV) > 0.5f && fx->getParameter(fx, P_LED_DLY) < 0.5f, "hall lights");
    // read-only parameters ignore writes (also when a host restores them from a project)
    float before = fx->getParameter(fx, P_LED_REV);
    fx->setParameter(fx, P_LED_REV, 0.f); fx->setParameter(fx, P_CHAIN, 0.7f);
    CHECK(fx->getParameter(fx, P_LED_REV) == before && fx->getParameter(fx, P_CHAIN) == 0.f, "read-only written");
    CHECK(H.d(effCanBeAutomated, P_OUTL) == 0 && H.d(effCanBeAutomated, P_INPUT) == 1, "automatable flags");
    // meters: -6 dBFS sine through bypass reads about -6 dB in and out
    fx->setParameter(fx, P_BYPASS, 1.f);
    std::vector<float> sl((size_t)H.sr), so1((size_t)H.sr), so2((size_t)H.sr);
    for (size_t i = 0; i < sl.size(); i++) sl[i] = 0.5f * sinf(6.2831853f * 1000.f * (float)i / H.sr);
    H.run(sl.data(), sl.data(), sl.size(), so1.data(), so2.data());
    for (int c = 0; c < 4; c++) {
        float n = fx->getParameter(fx, P_INL + c), db = kMeterLoDb + n * (kMeterHiDb - kMeterLoDb);
        CHECK(fabsf(db + 6.02f) < 0.5f, "meter %d reads %.2f dB for a -6 dB sine", c, db);
    }
    H.d(effGetParamDisplay, P_OUTL, 0, buf); printf("meter display for -6 dB sine: %s\n", buf);
    // clip latch, then release
    for (size_t i = 0; i < sl.size(); i++) sl[i] = sinf(6.2831853f * 1000.f * (float)i / H.sr) * 1.2f;
    H.run(sl.data(), sl.data(), 4800, so1.data(), so2.data());
    CHECK(fx->getParameter(fx, P_INL) == 1.f, "clip not latched");
    H.d(effGetParamDisplay, P_INL, 0, buf); CHECK(!strcmp(buf, "CLIP"), "clip display %s", buf);
    std::vector<float> zz2((size_t)H.sr * 3, 0.f), zo3((size_t)H.sr * 3);
    H.run(zz2.data(), zz2.data(), zz2.size(), zo3.data(), zo3.data());
    CHECK(fx->getParameter(fx, P_INL) < 0.05f, "meter didn't fall after clip (%.2f)", fx->getParameter(fx, P_INL));
    // input trim: +6 dB doubles level into the effect; bypass stays the untouched input
    fx->setParameter(fx, P_INPUT, toNorm(P_INPUT, 6.0206f));
    for (size_t i = 0; i < sl.size(); i++) sl[i] = 0.25f * sinf(6.2831853f * 1000.f * (float)i / H.sr);
    H.run(sl.data(), sl.data(), sl.size(), so1.data(), so2.data());
    double bmax = 0; for (size_t i = sl.size() / 2; i < sl.size(); i++) bmax = fmax(bmax, fabs(so1[i] - sl[i]));
    CHECK(bmax < 1e-4, "bypass not untouched with input trim (%.5f)", bmax);
    float inDb = kMeterLoDb + fx->getParameter(fx, P_INL) * (kMeterHiDb - kMeterLoDb);
    CHECK(fabsf(inDb - (-6.02f)) < 0.5f, "input meter after +6 dB trim reads %.2f", inDb);
    fx->setParameter(fx, P_BYPASS, 0.f);
    // the trim survives algorithm and program changes
    fx->setParameter(fx, P_ALGO, 5.f / (float)(NALGOS - 1)); H.d(effSetProgram, 0, 12);
    CHECK(fabsf(toReal(P_INPUT, fx->getParameter(fx, P_INPUT)) - 6.02f) < 0.05f, "input trim reset by preset");
    fx->setParameter(fx, P_INPUT, toNorm(P_INPUT, 0.f));

    H.d(effMainsChanged, 0, 0); H.d(effClose);
    printf(fails ? "\n%d FAILURES\n" : "\nALL TESTS PASSED\n", fails);
    return fails ? 1 : 0;
}
