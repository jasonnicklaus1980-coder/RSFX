// RSFX: one VST2 effect plugin holding approximations of the ASR-10 and ASR-X effect algorithms.
// Built for 32-bit ARM MPC OS (hand-written VST2 ABI, no Steinberg SDK), also builds on x86 Linux for tests.
#include "vst2.h"
#include "engine.h"
#include <new>
#include <stdio.h>
#include <string.h>
#if defined(__x86_64__) || defined(__i386__)
#include <xmmintrin.h>
#endif

using namespace rsfx;

namespace {

const int32_t kUniqueId = 0x52734678;   // 'RsFx'
const int32_t kVersion = 1100;

struct Plugin {
    AEffect fx;
    audioMasterCallback host;
    Engine eng;
    float param[NPARAMS];
    int program = 0;
    bool inAlgoChange = false;

    explicit Plugin(audioMasterCallback cb) : host(cb) {
        memset(&fx, 0, sizeof(fx));
        loadDefaults(0);
    }
    void loadDefaults(int a) {
        float input = param[P_INPUT];
        bool keepInput = programLoaded;
        for (int p = 0; p < NPARAMS; p++) param[p] = eng.info[a].def[p];
        param[P_BYPASS] = 0.f;
        if (keepInput) param[P_INPUT] = input;        // input trim belongs to the track, not the preset
        programLoaded = true;
    }
    bool programLoaded = false;
    // The MPC redraws a parameter only when the plugin announces it changed, so push the meters ~25x/s
    // and re-announce everything after a preset change (otherwise the Program name lags behind).
    int meterWait = 0;
    float meterSent[4] = {-1.f, -1.f, -1.f, -1.f};
    void notifyMeters(int n) {
        if (!host) return;
        meterWait -= n;
        if (meterWait > 0) return;
        meterWait = (int)((eng.sr > 0 ? eng.sr : 44100.f) * 0.04f);
        for (int c = 0; c < 4; c++) {
            float v = eng.meterNorm[c];
            if (fabsf(v - meterSent[c]) < 0.01f) continue;
            meterSent[c] = v;
            host(&fx, audioMasterAutomate, P_INL + c, 0, nullptr, v);
        }
    }
    void notifyAll() {
        if (!host) return;
        for (int p = 0; p < NPARAMS; p++) {
            if (p >= P_INL && p <= P_OUTR) continue;
            if (p == P_CHAIN || (p >= P_LED_REV && p <= P_LED_CHR)) continue;   // syncReadOnly(.., true) sends these
            host(&fx, audioMasterAutomate, p, 0, nullptr, param[p]);
        }
        syncReadOnly(algoFromNorm(param[P_ALGO]), true);
    }
    // the chain text and module lights follow the algorithm; tell the host so the screen redraws them
    void syncReadOnly(int a, bool notify) {
        param[P_CHAIN] = eng.info[a].def[P_CHAIN];
        for (int m = 0; m < 4; m++) param[P_LED_REV + m] = eng.info[a].def[P_LED_REV + m];
        if (notify && host) {
            host(&fx, audioMasterAutomate, P_CHAIN, 0, nullptr, param[P_CHAIN]);
            for (int m = 0; m < 4; m++) host(&fx, audioMasterAutomate, P_LED_REV + m, 0, nullptr, param[P_LED_REV + m]);
        }
    }
    void setParam(int i, float v, bool fromHost) {
        if (i < 0 || i >= NPARAMS || isReadOnly(i)) return;    // meters, chain text and lights are outputs
        v = v < 0 ? 0 : (v > 1 ? 1 : v);
        if (i == P_ALGO) {
            int oldA = algoFromNorm(param[P_ALGO]), newA = algoFromNorm(v);
            param[P_ALGO] = v;
            // like the hardware: selecting a new effect sets its parameters to that effect's defaults
            if (newA != oldA && !inAlgoChange) {
                inAlgoChange = true;
                float byp = param[P_BYPASS];
                for (int p = P_MIX; p < P_BYPASS; p++) {
                    param[p] = eng.info[newA].def[p];
                    if (fromHost && host) host(&fx, audioMasterAutomate, p, 0, nullptr, param[p]);
                }
                param[P_BYPASS] = byp;
                syncReadOnly(newA, fromHost);
                inAlgoChange = false;
            }
            return;
        }
        param[i] = v;
    }
};

inline Plugin* P(AEffect* e) { return (Plugin*)e->object; }

void copyStr(void* dst, const char* src, size_t max) {
    if (!dst) return;
    char* d = (char*)dst; size_t i = 0;
    for (; src[i] && i + 1 < max; i++) d[i] = src[i];
    d[i] = 0;
}

void display(Plugin* pl, int i, char* out, size_t max) {
    char b[32];
    int a = algoFromNorm(pl->param[P_ALGO]);
    const AlgoInfo& in = pl->eng.info[a];
    if (i == P_ALGO) { copyStr(out, kAlgos[a].name, max); return; }
    if (i == P_BYPASS) { copyStr(out, pl->param[i] >= 0.5f ? "On" : "Off", max); return; }
    if (i == P_CHAIN) { copyStr(out, in.chain, max); return; }
    if (i >= P_LED_REV && i <= P_LED_CHR) { copyStr(out, in.mod[i - P_LED_REV] ? "On" : "Off", max); return; }
    if (i >= P_INL && i <= P_OUTR) {
        float n = pl->eng.meterNorm[i - P_INL];
        if (n >= 1.f) copyStr(out, "CLIP", max);
        else if (n <= 0.f) copyStr(out, "-inf", max);
        else { snprintf(b, sizeof b, "%.1fdB", kMeterLoDb + n * (kMeterHiDb - kMeterLoDb)); copyStr(out, b, max); }
        return;
    }
    if (i == P_INPUT) { snprintf(b, sizeof b, "%+.1fdB", toReal(i, pl->param[i])); copyStr(out, b, max); return; }
    if (in.owner[i] < 0) { copyStr(out, "--", max); return; }
    float v = toReal(i, pl->param[i]);
    int owner = in.owner[i];
    switch (i) {
    case P_MIX: snprintf(b, sizeof b, "%.0f%%", v); break;
    case P_DECAY: snprintf(b, sizeof b, v < 10 ? "%.2fs" : "%.1fs", v); break;
    case P_PRE: case P_DELAY: snprintf(b, sizeof b, "%.0fms", v); break;
    case P_FB: snprintf(b, sizeof b, "%+.0f", v); break;
    case P_DAMP: snprintf(b, sizeof b, "%.0f", v); break;
    case P_RATE: snprintf(b, sizeof b, v < 1 ? "%.2fHz" : "%.1fHz", v); break;
    case P_DEPTH:
        if (owner == S_PITCH || owner == S_DETUNE) snprintf(b, sizeof b, "%.0fc", v / 99.f * 50.f);
        else snprintf(b, sizeof b, "%.0f", v);
        break;
    case P_DRIVE: snprintf(b, sizeof b, "%+.1fdB", v); break;
    case P_FREQ: snprintf(b, sizeof b, v < 1000 ? "%.0fHz" : "%.2fk", v < 1000 ? v : v / 1000.f); break;
    case P_RESO:
        if (owner == S_COMP || owner == S_DUCK || owner == S_EXP || owner == S_INVEXP || owner == S_KEYEXP) {
            if (v >= 98.5f) snprintf(b, sizeof b, "inf:1");
            else snprintf(b, sizeof b, "%.1f:1", 1.f + powf(v / 99.f, 2.f) * 19.f);
        } else snprintf(b, sizeof b, "%.0f", v);
        break;
    case P_THRESH: snprintf(b, sizeof b, "%.0fdB", v); break;
    case P_PITCH: snprintf(b, sizeof b, "%+.1fst", v); break;
    case P_BAL:
        if (in.balRatio) snprintf(b, sizeof b, "R x%.2f", 0.25f + v / 99.f * 1.75f);
        else snprintf(b, sizeof b, "%.0f", v);
        break;
    case P_OUT: snprintf(b, sizeof b, "%+.1fdB", v); break;
    default: snprintf(b, sizeof b, "%.2f", v);
    }
    // The skin groups knobs into modules. When a knob is used by a block from another module
    // (e.g. Damping darkening a delay), tag the value so the screen doesn't suggest the wrong module.
    int home = (i >= P_DECAY && i <= P_PRE) ? 0 : (i >= P_DELAY && i <= P_FB) ? 1 : (i == P_BAL) ? 1 :
               (i == P_RATE || i == P_DEPTH) ? 2 : (i >= P_DRIVE && i <= P_PITCH) ? 3 : -1;
    if (i == P_BAL && in.balChain) strncat(b, " SEND", sizeof b - strlen(b) - 1);
    else if (home >= 0 && owner < S_COUNT && moduleOf((uint8_t)owner) != home) {
        static const char* tag[S_COUNT] = {"", "REV", "REV", "REV", "REV", "DLY", "DLY", "DLY", "CHO", "FLG", "PHS",
            "VIB", "TRM", "ROT", "SPN", "DST", "VCF", "WAH", "AMP", "CAB", "SPK", "EQ", "EQ", "CMP", "GATE", "EXP",
            "EXP", "EXP", "DES", "DUCK", "PIT", "DET", "FMT", "CHT", "RMB", "EXC"};
        strncat(b, " ", sizeof b - strlen(b) - 1);
        strncat(b, tag[owner], sizeof b - strlen(b) - 1);
    }
    copyStr(out, b, max);
}

intptr_t dispatcher(AEffect* e, int32_t op, int32_t idx, intptr_t val, void* ptr, float opt) {
    Plugin* pl = P(e);
    // VST2 says 8 chars for parameter strings; hosts give more, 24 keeps longer names readable and safe.
    const size_t kStr = 24;
    switch (op) {
    case effOpen: return 0;
    case effClose: pl->~Plugin(); free(pl); return 0;
    case effSetProgram:
        if (val >= 0 && val < NALGOS) {
            pl->program = (int)val;
            pl->loadDefaults((int)val);
            pl->syncReadOnly((int)val, false);
            pl->notifyAll();
        }
        return 0;
    case effGetProgram: return pl->program;
    case effGetProgramName: copyStr(ptr, kAlgos[pl->program].name, kVstMaxProgNameLen); return 0;
    case effGetProgramNameIndexed:
        if (idx >= 0 && idx < NALGOS) { copyStr(ptr, kAlgos[idx].name, kVstMaxProgNameLen); return 1; }
        return 0;
    case effGetParamName: if (idx >= 0 && idx < NPARAMS) copyStr(ptr, kParams[idx].name, kStr); return 0;
    case effGetParamLabel: if (ptr) ((char*)ptr)[0] = 0; return 0;
    case effGetParamDisplay: if (idx >= 0 && idx < NPARAMS) display(pl, idx, (char*)ptr, kStr); return 0;
    case effCanBeAutomated: return (idx >= 0 && idx < NPARAMS && !isReadOnly(idx)) ? 1 : 0;
    case effSetSampleRate: pl->eng.prepare(opt); return 0;
    case effSetBlockSize: return 0;
    case effMainsChanged: if (val && !pl->eng.ready) pl->eng.prepare(pl->eng.sr); return 0;
    case effGetPlugCategory: return kPlugCategEffect;
    case effGetEffectName: copyStr(ptr, "RSFX", kVstMaxEffectNameLen); return 1;
    case effGetProductString: copyStr(ptr, "RSFX Retro Sampler FX", kVstMaxProductStrLen); return 1;
    case effGetVendorString: copyStr(ptr, "RSFX", kVstMaxVendorStrLen); return 1;
    case effGetVendorVersion: return kVersion;
    case effGetVstVersion: return 2400;
    case effGetTailSize: return (intptr_t)(pl->eng.sr * 12);
    case effCanDo: {
        const char* s = (const char*)ptr;
        if (!s) return 0;
        if (!strcmp(s, "plugAsChannelInsert") || !strcmp(s, "plugAsSend") || !strcmp(s, "2in2out")) return 1;
        return -1;
    }
    case effSetBypass: return 0;
    default: return 0;
    }
}

void setParameter(AEffect* e, int32_t i, float v) { P(e)->setParam(i, v, true); }
float getParameter(AEffect* e, int32_t i) {
    if (i < 0 || i >= NPARAMS) return 0.f;
    if (i >= P_INL && i <= P_OUTR) return P(e)->eng.meterNorm[i - P_INL];
    return P(e)->param[i];
}

inline void denormalsOff() {
#if defined(__arm__) && !defined(__aarch64__)
    unsigned int fpscr;
    __asm__ volatile("vmrs %0, fpscr" : "=r"(fpscr));
    fpscr |= (1u << 24);                       // flush-to-zero
    __asm__ volatile("vmsr fpscr, %0" : : "r"(fpscr));
#elif defined(__aarch64__)
    uint64_t fpcr;
    __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
    fpcr |= (1ull << 24);
    __asm__ volatile("msr fpcr, %0" : : "r"(fpcr));
#elif defined(__x86_64__) || defined(__i386__)
    _mm_setcsr(_mm_getcsr() | 0x8040);       // FTZ | DAZ
#endif
}

void processReplacing(AEffect* e, float** in, float** out, int32_t n) {
    Plugin* pl = P(e);
    denormalsOff();
    float tgt[NPARAMS]; memcpy(tgt, pl->param, sizeof tgt);
    pl->eng.process(in[0], in[1], out[0], out[1], n, tgt);
    pl->notifyMeters(n);
}

void processAccum(AEffect* e, float** in, float** out, int32_t n) {
    float tl[1024], tr[1024];
    for (int32_t pos = 0; pos < n; pos += 1024) {
        int32_t m = n - pos < 1024 ? n - pos : 1024;
        float* o[2] = {tl, tr}; float* i2[2] = {in[0] + pos, in[1] + pos};
        processReplacing(e, i2, o, m);
        for (int32_t k = 0; k < m; k++) { out[0][pos + k] += tl[k]; out[1][pos + k] += tr[k]; }
    }
}

} // namespace

extern "C" {
__attribute__((visibility("default"))) AEffect* VSTPluginMain(audioMasterCallback host) {
    if (!host || host(nullptr, audioMasterVersion, 0, 0, nullptr, 0) == 0) return nullptr;
    void* mem = malloc(sizeof(Plugin));
    if (!mem) return nullptr;
    Plugin* pl = new (mem) Plugin(host);
    AEffect& fx = pl->fx;
    fx.magic = kEffectMagic;
    fx.dispatcher = dispatcher;
    fx.process = processAccum;
    fx.setParameter = setParameter;
    fx.getParameter = getParameter;
    fx.processReplacing = processReplacing;
    fx.numPrograms = NALGOS;
    fx.numParams = NPARAMS;
    fx.numInputs = 2;
    fx.numOutputs = 2;
    fx.flags = effFlagsCanReplacing;
    fx.object = pl;
    fx.uniqueID = kUniqueId;
    fx.version = kVersion;
    fx.ioRatio = 1.f;
    pl->eng.prepare(44100.f);
    return &fx;
}
// legacy Linux entry point name
__attribute__((visibility("default"))) AEffect* main_plugin(audioMasterCallback host) __asm__("main");
AEffect* main_plugin(audioMasterCallback host) { return VSTPluginMain(host); }
}
