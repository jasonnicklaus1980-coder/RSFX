// Dumps the plugin's parameter values, names and display strings as JSON, after playing a drum-like signal,
// so skin previews show the plugin's real state.  Usage: dump_state plugin.so program [level_dB]
#include "../src/vst2.h"
#include <dlfcn.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
static intptr_t cb(AEffect*, int32_t op, int32_t, intptr_t, void*, float) { return op == audioMasterVersion ? 2400 : 0; }
static void js(const char* s) { putchar('"'); for (; *s; s++) { if (*s == '"' || *s == '\\') putchar('\\'); putchar(*s); } putchar('"'); }
int main(int argc, char** argv) {
    if (argc < 3) return 2;
    void* h = dlopen(argv[1], RTLD_NOW); if (!h) return 1;
    AEffect* fx = ((AEffect* (*)(audioMasterCallback))dlsym(h, "VSTPluginMain"))(cb);
    fx->dispatcher(fx, effSetSampleRate, 0, 0, nullptr, 44100.f); fx->dispatcher(fx, effMainsChanged, 0, 1, nullptr, 0);
    fx->dispatcher(fx, effSetProgram, 0, atoi(argv[2]), nullptr, 0);
    float lvl = argc > 3 ? powf(10.f, (float)atof(argv[3]) / 20.f) : 0.5f;
    const int N = 44100; std::vector<float> l(N), r(N), o1(N), o2(N);
    for (int i = 0; i < N; i++) { float t = fmodf(i / 44100.f, 0.25f); l[i] = lvl * expf(-t * 25) * sinf(6.283f * 110.f * t); r[i] = l[i] * 0.85f; }
    for (int p = 0; p < N; p += 512) { int m = N - p < 512 ? N - p : 512; float* in[2] = {&l[p], &r[p]}; float* out[2] = {&o1[p], &o2[p]}; fx->processReplacing(fx, in, out, m); }
    char b[64];
    printf("{\"values\":{");
    for (int p = 0; p < fx->numParams; p++) printf("%s\"%d\":%.5f", p ? "," : "", p, fx->getParameter(fx, p));
    printf("},\"names\":{");
    for (int p = 0; p < fx->numParams; p++) { b[0] = 0; fx->dispatcher(fx, effGetParamName, p, 0, b, 0); printf("%s\"%d\":", p ? "," : "", p); js(b); }
    printf("},\"display\":{");
    for (int p = 0; p < fx->numParams; p++) { b[0] = 0; fx->dispatcher(fx, effGetParamDisplay, p, 0, b, 0); printf("%s\"%d\":", p ? "," : "", p); js(b); }
    printf("}}\n");
    return 0;
}
