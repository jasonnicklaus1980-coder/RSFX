# Source register

Per-algorithm verification against these sources: `docs/VERIFICATION.md`.

Every design decision that rests on documentation, with its source. "PDF p." is the page in the uploaded
`ensoniq_asr10_manual.pdf` (390 pages); section page numbers are those printed in the manual's footers.

**Sources**
- **[A10]** ENSONIQ ASR-10 Musician's Manual (scanned PDF, 390 pp., the user's upload).
- **[AXP]** ENSONIQ ASR-X Pro Reference Manual, chapter 4 "Effects", pp.41-47 (online copy of the chapter PDF).
- **[AXU]** ENSONIQ ASR-X Pro User's Guide v3.00, "Insert Effect Parameters" pp.27-43 (thesoniq.com/Manual/asrxpro.pdf),
  retrieved for insert effects 01-24.
- **[DAT]** J. Dattorro, "Effect Design, Part 1: Reverberator and Other Filters", J. Audio Eng. Soc. 45(9), 1997.

## Confirmed from the manuals

| Decision | Source |
|---|---|
| ASR-10 has 50 effect algorithms, 24-bit | [A10] Preface p.i (PDF p.1) |
| ASR-10 ROM effect names and order (ROM-01..50) | [A10] Section 5 pp.3-4 (PDF pp.88-89) |
| Selecting a new effect loads that effect's defaults → the Algorithm knob does the same | [A10] Section 5 p.3 (PDF p.88) |
| Standard ASR-10 effects run at the 30 kHz system rate; 44KHZ algorithms at 44.1 kHz (23 voices) → 13.5 kHz band limit on non-44KHZ ASR-10 algorithms | [A10] Preface p.ii (PDF p.2); Section 6 pp.2-3 (PDF pp.98-99) |
| Reverb parameter set: decay, low-freq decay, diffusion, HF damping, HF bandwidth (input low-pass), detune rate and depth, pre-delay 0-500 ms | [A10] Section 6 pp.2-3 (PDF pp.98-99) |
| Dual Delays: up to 2000 ms, feedback, separate L/R times | [A10] Section 6 p.3 (PDF p.99) |
| Phaser is 12-pole | [A10] Section 6 p.4 (PDF p.100) |
| Chorus uses four modulation rates (one per voice) | [A10] Section 6 p.4 (PDF p.100) |
| Rotary speaker: fast/slow rotor, inertia, distortion before the rotor | [A10] Section 6 (PDF pp.101-102) |
| Room/hall reverbs have early reflections; Hall Reverb has left-right (transverse) reflections | [A10] Section 5 p.3 (PDF p.88); Section 6 (PDF pp.106-108) |
| Reverse, gated and non-linear reverbs: trigger threshold, attack/hold/release envelope on the reverb | [A10] Section 6 (PDF pp.110-116) |
| Multi-tap delay has four taps with time, regen, level, pan | [A10] Section 6 (PDF p.117) |
| VCF+Distort uses an envelope-followed resonant filter before the distortion | [A10] Section 6 (PDF pp.120-121) |
| Guitar amps: pre-EQ, preamp gain, tube bias, gate, post EQ, low-pass; Amp 3 adds an inverse expander | [A10] Section 6 (PDF pp.121-125) |
| Pitch shifter is a "splicer type", two voices, long/short quality setting → two-tap splice shifter, 60/18 ms windows | [A10] Section 5 p.4 (PDF p.89); Section 6 (PDF pp.136-140) |
| Compressor ratio up to 40:1 and infinity; expander, keyed expander, inverse expander, de-esser, ducker exist | [A10] Section 5 p.4 (PDF p.89); Section 6 (PDF pp.140-150) |
| Rumble filter = high-pass in cascade with a low-pass | [A10] Section 5 p.4 (PDF p.89) |
| Van der Pol adds synthetic high harmonics → harmonic exciter | [A10] Section 5 p.4 (PDF p.89) |
| ASR-X Pro has 40 insert effects (names and numbering) | [AXP] p.41 |
| ASR-X Pro has 8 global reverbs (names) | [AXP] p.42 |
| ASR-X effects run on an ENSONIQ ESP2 chip; one insert effect plus one global reverb per sequence | [AXP] p.41 |
| Insert effects have an Input Mix from "Full Dry" to "Full Wet" → Mix knob | [AXP] p.44 |
| ASR-X has 5 distortion curves and 8 LFO shapes (names not available) | [AXP] p.47 |

## Not from the manuals (my design choices or estimates)

| Decision | Basis |
|---|---|
| Reverb core = Dattorro figure-8 plate tank, delay lengths and taps from the paper | [DAT]. Its parameters match the ASR-10's reverb parameter list one-to-one, but this is **not** a claim that Ensoniq used this code |
| Reverb voicings (size, diffusion, early-reflection taps) per algorithm | Estimates |
| Default settings of every algorithm | Estimates, chosen to sound usable |
| Distortion curves (soft, hard, asymmetric tube, fuzz, fold) | Generic, the ASR-X curve names are unknown |
| Chatter Box (random vowel steps), Formant Morph (LFO vowel sweep) | Inferred from names; no parameter documentation found |
| Spinner (rotating auto-pan) | A simplification: [AXU] p.38 documents a pseudo-3D panner with delay, level and filter modulation |
| ASR-X effect parameters | Reuse the ASR-10 designs. [AXU] documents 01-24; `docs/VERIFICATION.md` lists where RSFX departs from it |
| Shared 15-knob layout instead of each effect's own parameter pages | Needed for one VST2 with fixed parameters on Q-Links |
| Ducker / Keyed Expander self-keyed | MPC inserts have no sidechain input |

## How to improve accuracy

With access to real hardware, record each algorithm at known settings (a 0 dBFS click, a 10 s log sine sweep, and
a dry drum loop, 44.1 kHz/24-bit, Mix at 100%), and note the panel values. Those let the reverb RT60 per band,
delay filtering, modulation rates and distortion transfer curves be measured and matched, algorithm by algorithm.

## Documented but not implemented yet: Version 2 O.S. disk algorithms

The ASR-10 Version 2 O.S. disk adds 12 algorithms at the 44 kHz rate, each with four factory variations, a routing
diagram and a parameter list, in the Appendix "44 kHz Effect Descriptions and Variations" ([A10] PDF pp.367-375):
44LUSH PLATE, 44LUSH PLAT2, 44PERC PLATE (p.367); 44EQ+DDL (p.368); 44DDL+CH+REV (p.369); 44DDL+CHORUS (p.370);
44DLYLFO+REV (p.371); 44EQ+DDL+CHO, 44PARAM EQ (p.372); 44EQ+REVERB (p.373); 44ROTO+REVRB (p.374); 44EQ+ROT+DDL (p.375).

## Secondary source checked: "ASR-10 & ASR-X FX Descriptions" (user-supplied working reference)

It describes itself as unverified. Checked against [A10] and [AXP]; where they disagree, the manuals win:
- Names such as SWIRLING CHORUS, DDL+SLOW CHORUS, FLANGED CHORUS, WARM CHORUS, PING PONG CHORUS, SIDE TO SIDE, DETUNER
  and WHEEL WOW are listed there as algorithms; in [A10] they are factory *variations* of the 44 kHz algorithms (PDF pp.369-372).
- It describes VAN DER POL as a resonant filter; [A10] Section 5 p.4 says it adds synthetic high harmonics. RSFX follows [A10].
- It omits 44LUSH PLATE, 44LUSH PLAT2, 44PERC PLATE and 44PARAM EQ, which [A10] documents.
- Some names differ from the display names in [A10] (8-VOICE CHORUS vs 8-VOICE CHOR, EQ+FLANGER+DDL vs EQ+FLNGR+DDL,
  TUNABLE SPEAKER vs TUNABLE SPKR); RSFX uses the display names.
- Its ASR-X section lists generic effect types only; RSFX uses the 40 insert effects and 8 global reverbs named in [AXP].
