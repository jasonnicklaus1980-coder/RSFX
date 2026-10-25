// Prints docs/ALGORITHMS.md from the algorithm table, so the docs always match the code.
#include "../src/algos.h"
#include <stdio.h>
using namespace rsfx;
static const char* stName(int s) {
    static const char* n[] = {"", "Reverb", "Gated reverb", "Reverse reverb", "Non-linear reverb", "Delay", "Multi-tap delay",
        "LFO delay", "Chorus", "Flanger", "Phaser (12-pole)", "Vibrato", "Tremolo", "Rotary speaker", "Spinner (auto-pan)",
        "Distortion", "Resonant VCF", "Wah", "Guitar amp", "Speaker cabinet", "Tunable speaker", "EQ", "Parametric EQ",
        "Compressor", "Gate", "Expander", "Inverse expander", "Keyed expander", "De-esser", "Ducker", "Pitch shifter",
        "Pitch detuner", "Formant morph", "Chatter box", "Rumble filter", "Harmonic exciter"};
    return s < S_COUNT ? n[s] : "?";
}
int main() {
    printf("# Algorithm reference\n\nGenerated from `src/algos.h` by `make docs`. The Q-Link order is the parameter order below; a knob the algorithm\n"
           "doesn't use shows `--` on the MPC.\n\n");
    const char* model[] = {"ASR-10 ROM effects (Musician's Manual Section 5, pp.3-4)", "ASR-X / ASR-X Pro insert effects (Reference Manual ch.4, p.41)",
                           "ASR-X / ASR-X Pro global reverbs (Reference Manual ch.4, p.42)"};
    int lastModel = -1;
    for (int a = 0; a < NALGOS; a++) {
        const AlgoDef& d = kAlgos[a];
        if (d.model != lastModel) {
            printf("\n## %s\n\n| # | Name | Chain | Knobs in use |\n|---|---|---|---|\n", model[d.model]);
            lastModel = d.model;
        }
        AlgoInfo in; buildInfo(a, in);
        char chain[256] = ""; int n = chainLen(d);
        for (int i = 0; i < n; i++) { strcat(chain, stName(d.st[i])); if (i + 1 < n) strcat(chain, " → "); }
        char knobs[512] = "";
        for (int p = P_DECAY; p < P_OUT; p++) if (in.owner[p] >= 0) {
            if (knobs[0]) strcat(knobs, ", ");
            strcat(knobs, kParams[p].name);
            if (p == P_BAL && in.balRatio) strcat(knobs, " (R/L time)");
            else if (p == P_BAL) strcat(knobs, " (send)");
        }
        printf("| %d | %s | %s | %s |\n", a + 1, d.name, chain, knobs);
    }
    return 0;
}
