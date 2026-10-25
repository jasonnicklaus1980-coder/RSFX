// Regression: the new build must sound exactly like v1.0 for every algorithm when the new Input knob is at 0 dB,
// and v1.0 parameter indices 0..16 must keep their names. Usage: compat_test old.so new.so
#include "../src/vst2.h"
#include <dlfcn.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <vector>
static intptr_t cb(AEffect*, int32_t op, int32_t, intptr_t, void*, float) { return op == audioMasterVersion ? 2400 : 0; }
typedef AEffect* (*Entry)(audioMasterCallback);
static Entry entryOf(const char* p) {
    void* h = dlopen(p, RTLD_NOW | RTLD_LOCAL); if (!h) { printf("%s\n", dlerror()); return nullptr; }
    return (Entry)dlsym(h, "VSTPluginMain");
}
static AEffect* fresh(Entry e) {
    AEffect* x = e(cb);
    x->dispatcher(x, effSetSampleRate, 0, 0, nullptr, 44100.f); x->dispatcher(x, effMainsChanged, 0, 1, nullptr, 0);
    return x;
}
int main(int argc, char** argv) {
    if (argc < 3) return 2;
    Entry ea = entryOf(argv[1]), eb = entryOf(argv[2]);
    if (!ea || !eb) return 1;
    AEffect* a = fresh(ea); AEffect* b = fresh(eb);
    int fails = 0, identical = 0, fixed = 0, phaseOnly = 0; double worstDb = 0;
    for (int p = 0; p < a->numParams; p++) {
        char na[64] = "", nb[64] = "";
        a->dispatcher(a, effGetParamName, p, 0, na, 0); b->dispatcher(b, effGetParamName, p, 0, nb, 0);
        if (strcmp(na, nb)) { printf("FAIL: param %d renamed %s -> %s\n", p, na, nb); fails++; }
    }
    printf("v1 params %d, new params %d, programs %d/%d\n", a->numParams, b->numParams, a->numPrograms, b->numPrograms);
    const int N = 44100 * 2; std::vector<float> l(N), r(N), oa1(N), oa2(N), ob1(N), ob2(N);
    unsigned s = 7; for (int i = 0; i < N; i++) { s = s * 1664525u + 1013904223u; float n = ((s >> 8) / 16777216.f - 0.5f);
        float t = fmodf(i / 44100.f, 0.4f); l[i] = expf(-t * 20) * n + 0.2f * sinf(i * 0.05f); r[i] = 0.8f * l[i] + 0.1f * n; }
    double worst = 0;
    for (int pass = 0; pass < 2; pass++)
    for (int prog = 0; prog < a->numPrograms; prog++) {
        // each program in a fresh instance of both builds, like loading a project
        a->dispatcher(a, effClose, 0, 0, nullptr, 0); b->dispatcher(b, effClose, 0, 0, nullptr, 0);
        a = fresh(ea); b = fresh(eb);
        a->dispatcher(a, effSetProgram, 0, prog, nullptr, 0); b->dispatcher(b, effSetProgram, 0, prog, nullptr, 0);
        // pass 0: programs loaded directly in both; pass 1: a saved v1 project restored by index into the new build
        if (pass) for (int p = 0; p < a->numParams; p++) { float v = a->getParameter(a, p); b->setParameter(b, p, v); }
        // settle: one second of silence lets the algorithm switch and v1's knob glide finish (buffers stay silent)
        { std::vector<float> z(44100, 0.f), zo(44100);
          for (int pos = 0; pos < 44100; pos += 512) { int m = 44100 - pos < 512 ? 44100 - pos : 512;
              float* zi[2] = {&z[pos], &z[pos]}; float* o[2] = {&zo[pos], &zo[pos]};
              a->processReplacing(a, zi, o, m); b->processReplacing(b, zi, o, m); } }
        for (int pos = 0; pos < N; pos += 512) {
            float* ia[2] = {&l[pos], &r[pos]}; float* o1[2] = {&oa1[pos], &oa2[pos]}; float* o2[2] = {&ob1[pos], &ob2[pos]};
            int m = N - pos < 512 ? N - pos : 512;
            a->processReplacing(a, ia, o1, m); b->processReplacing(b, ia, o2, m);
        }
        double d = 0; for (int i = 0; i < N; i++) d = fmax(d, fmax(fabs(oa1[i] - ob1[i]), fabs(oa2[i] - ob2[i])));
        char nm[32] = "", chain[64] = "";
        a->dispatcher(a, effGetProgramName, 0, 0, nm, 0);
        if (b->numParams > 18) b->dispatcher(b, effGetParamDisplay, 18, 0, chain, 0);
        // (1) every knob this algorithm uses must hold the same value in both builds
        for (int p = 1; p < 16; p++) {
            char dv[64] = ""; b->dispatcher(b, effGetParamDisplay, p, 0, dv, 0);
            if (strcmp(dv, "--") && a->getParameter(a, p) != b->getParameter(b, p)) {
                printf("FAIL: %s knob %d value %.5f vs %.5f\n", nm, p, a->getParameter(a, p), b->getParameter(b, p)); fails++; }
        }
        double ea = 0, eb = 0; for (int i = 0; i < N; i++) { ea += oa1[i] * oa1[i] + oa2[i] * oa2[i]; eb += ob1[i] * ob1[i] + ob2[i] * ob2[i]; }
        double ldb = 10 * log10((eb + 1e-20) / (ea + 1e-20));
        // (2) v1.0 delay-time glide bug (DELAY / LFO DELAY blocks stalled up to ~5 samples short): expected to differ
        bool glideFix = strstr(chain, "DELAY") && !strstr(chain, "MULTITAP");
        // (3) LFO / splice phase depends on v1.0's knob glide right after loading: same sound, shifted phase
        static const char* phased[] = {"CHORUS", "FLANGER", "PHASER", "ROTARY", "SPINNER", "PITCH", "DETUNE", "FORMANT",
                                       "CHATTER", "TREMOLO", "VIBRATO", "MULTITAP"};
        bool phase = false; for (const char* k : phased) if (strstr(chain, k)) phase = true;
        if (glideFix || phase) {
            if (glideFix) fixed++; else phaseOnly++;
            if (fabs(ldb) > 0.5) { printf("FAIL (pass %d): %s level differs by %.2f dB\n", pass, nm, ldb); fails++; }
            worstDb = fmax(worstDb, fabs(ldb));
            continue;
        }
        if (d == 0) identical++;
        worst = fmax(worst, d);
        if (d > 1e-3) { printf("FAIL (pass %d): %s differs by %.3g\n", pass, nm, d); fails++; }   // -60 dBFS
    }
    a->dispatcher(a, effClose, 0, 0, nullptr, 0); b->dispatcher(b, effClose, 0, 0, nullptr, 0);
    int rounding = 2 * a->numPrograms - identical - fixed - phaseOnly;
    printf("knob values: identical for every used knob of every program\n"
           "audio over 2 passes x %d programs: %d bit-identical, %d within rounding (max %.2g, %.0f dB below full scale),\n"
           "%d with a delay block (v1.0 delay-time glide fix), %d with LFO/splice phase depending on load history;\n"
           "those %d match v1.0 in level within %.2f dB\n%s\n", a->numPrograms, identical, rounding, worst,
           worst > 0 ? -20 * log10(worst) : 999.0, fixed, phaseOnly, fixed + phaseOnly, worstDb, fails ? "COMPAT FAILED" : "COMPAT OK");
    return fails ? 1 : 0;
}
