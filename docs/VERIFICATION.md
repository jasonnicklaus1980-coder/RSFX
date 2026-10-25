# FX verification report

Every RSFX algorithm checked against the manuals: is the name right, what does the manual say the effect does, and
does RSFX do that? No code was changed for this report; it records what is and isn't faithful.

**Sources**
- **[A10]** ASR-10 Musician's Manual (uploaded scan). Names: Section 5 pp.3-4 (PDF pp.88-89). Descriptions and
  parameters: Section 6 (PDF pp.98-152).
- **[AXP]** ASR-X Pro Reference Manual ch.4 pp.41-42 (effect names).
- **[AXU]** ASR-X Pro User's Guide v3.00, "Insert Effect Parameters" pp.27-43 (thesoniq.com/Manual/asrxpro.pdf).
  Retrieved for effects 01-24 only; the retrieval cut off before 25-40.

**Status key**
- **Match**: same structure and controls as documented (sound still an approximation, nothing is tuned to hardware).
- **Partial**: right kind of effect and signal order, but documented controls or ranges are missing or different.
- **Differs**: the documented structure or routing is different from RSFX's.

## Names

- **ASR-10:** all 50 names and their order match the ROM Effects List exactly. Section 6 headings space two names
  differently ("EQ + DELAY LFO", "GUITAR AMP 1"); RSFX uses the list's spelling.
- **ASR-X:** all 40 insert effects and 8 global reverbs match [AXP], in order. "→" is written as ">" on the MPC.

## Findings that apply to many algorithms

- **Bus routing:** ASR-10 effects take up to three buses (BUS1/2/3) with separate wet/dry amounts, and in several
  algorithms a bus skips a block (e.g. BUS2 = reverb only). RSFX runs one stereo input through the BUS1 path.
- **Ranges:** ASR-10 reverb decays reach 100-250 s (RSFX: 30 s max). ASR-X reverbs are 0-10 s with 0-36 ms
  pre-delay; RSFX allows 500 ms pre-delay on them. Compressor ratios reach 40:1 (RSFX: 20:1, plus infinity).
- **Feedback polarity:** many ASR-10 chorus/flanger/phaser feedbacks are -99..+99; RSFX's Reso is positive only.
- **ASR-X LFOs** offer eight shapes and tempo sync (1/1 to 1/32 of system tempo) [AXU]; RSFX has fixed shapes, no sync.
- **Early reflections:** documented as two (ASR-10 rooms, ASR-X halls/rooms) or four (ASR-X plates) controllable
  reflections; RSFX uses six fixed taps for rooms and halls.

## ASR-10 ROM effects

| # | Effect | What [A10] documents | RSFX | Status |
|---|---|---|---|---|
| 1 | HALL REVERB | Long decay, transverse reflections; decay, LF decay, diffusion, HF damping/bandwidth, detune, pre-delay 0-500 ms | Plate-type tank with transverse reflections; no LF decay control | Partial |
| 2 | 44KHZ REVERB | Hall at 44 kHz; pre-delay 0-340 ms | As 1 without the 30 kHz band limit; pre-delay goes to 500 ms | Partial |
| 3 | ROOM REVERB | Shorter, room-like decay | Room voicing | Partial |
| 4 | DUAL DELAYS | Two delays: BUS1 0-2000 ms, BUS2 0-150 ms, each with mix and feedback | L/R delay, R time a ratio of L (up to 2x) | Partial |
| 5 | 44KHZ DELAYS | As 4 at 44 kHz; BUS1 0-1300 ms | Allows 2000 ms | Partial |
| 6 | CHORUS+REVRB | Four-voice chorus into reverb; chorus feedback -99..+99, early-reflection level | Four voices at four rates into reverb; feedback positive only | Partial |
| 7 | PHASER+REVRB | 12-pole phaser into reverb | Same | Match |
| 8 | FLANGER+REV | Flanger with a single LFO into reverb | Single LFO, right channel 90° offset | Match |
| 9 | ROT. SPKR+REV | Rotor fast/slow, inertia, fast/slow switch, distortion (optional) | Continuous speed with inertia, drive; no switch | Partial |
| 10 | CHOR+REV+DDL | Chorus + reverb on BUS1; delay on **separate BUS3** | Delay in series after the reverb | Differs |
| 11 | CMP+DIST+REV | Compressor → distortion → **EQ → flanger (feedback)** → reverb | Compressor → distortion → reverb; no EQ or flanger | Differs |
| 12 | DIST+CHO+REV | Distortion, chorus, reverb; serial or parallel by bus | Serial | Match (serial case) |
| 13 | WAH+DIST+REV | Amplitude-following wah, distortion, **compression**, reverb; wah range ±99 | No compression; upward wah only | Partial |
| 14-16 | SMALL ROOM, LARGE ROOM, HALL REVERB2 | Decay to 100/150/250 s, pre-delay 0-450 ms, 2 diffusers, definition, 2 reflections | 30 s cap, fixed diffusion, 6 fixed reflections | Partial |
| 17-18 | SMALL PLATE, LARGE PLATE | Decay to 100/140 s, pre-delay 0-500/430 ms, L/R balance | Plate tank; 30 s cap | Partial |
| 19 | REVRSE REVRB | Plate with ramped output, triggers once; attack/hold/release, threshold, slapback | Ramp length (Decay) and threshold; no hold/release/slapback | Partial |
| 20 | REVRSE REVB2 | As 19 but retriggers; pre-trigger time | Retriggers; no pre-trigger | Partial |
| 21 | GATED REVERB | Attack/hold/decay/release, trigger and retrigger thresholds, slapback | Hold (Decay) and threshold; fixed attack/release | Partial |
| 22-24 | NLIN REVRB 1-3 | Input passes the diffusers **once** ("density"); non-exponential; reflections to 600 ms (85 ms on 2) | Recirculating tank cut by an envelope | Differs |
| 25 | MULTITAP DDL | Four delays to 2186 ms, each with regen, level, pan | Four taps at fixed fractions of one time | Partial |
| 26 | EQ+DELAY LFO | Bass/treble EQ; L/R delays to 845 ms, LFO, **cross regen**, regen damping | One bell EQ; delay to 2000 ms; no cross regen | Partial |
| 27 | VCF+DISTORT | Pre-distortion VCF with ±envelope, distortion, **post-distortion VCF** | Pre-VCF (upward envelope) → distortion | Partial |
| 28-29 | GUITAR AMP1/2 | Preamp gain, tube bias, pre-EQ, gate, HP, two EQs, LP 2-15 kHz | Same blocks; bias and post-EQ fixed per model | Partial |
| 30 | GUITAR AMP3 | Amp with inverse expander (ratio, threshold) | Inverse expansion fixed | Partial |
| 31 | SPKR CABINET | Speaker resonances **and nonlinearity** | Linear filters only | Partial |
| 32 | TUNABLE SPKR | **Three** tunable parametric filters | Fixed cabinet + one tunable band | Partial |
| 33 | EQ+CHOR+DDL | Bass/treble EQ → chorus → delay and echo | Bell EQ → chorus → delay | Partial |
| 34 | EQ+VIBR+DDL | Vibrato with sample & hold ("chirping"), delay | No sample & hold | Partial |
| 35 | EQ+FLNGR+DDL | Flanger with notch depth, ±feedback, delay | Positive feedback only | Partial |
| 36 | EQ+TREM+DDL | Tremolo to output, delay with regen | Tremolo then delay | Match |
| 37 | PHASER+DDL | Stereo 12-pole phaser, delay to 1600 ms | Same; delay to 2000 ms | Match |
| 38 | 8-VOICE CHOR | Eight voices, **randomized** LFOs, **cross-coupled stereo delay** | Eight voices, fixed rate spread, no delay | Partial |
| 39 | PITCH SHIFT | Two voices, each ±12 semitones with fine, level, pan; long/short quality | Voice 2 follows voice 1 within 50 cents; fixed pans | Partial |
| 40 | PITCH+DDL | Delay **feeds back into the shifter** (spiralling pitch) | Delay after the shifter, no spiral | Differs |
| 41 | FAST PITCHSH | 10 ms delay, **at most ±1 semitone** (fine only) | Pitch knob allows ±12 semitones | Differs |
| 42 | EQ+CMPRESSOR | Ratio to 40:1/inf, attack 50 µs-100 ms, release, gate, EQ in signal and side chain | Ratio to 20:1/inf, fixed timing, no gate | Partial |
| 43 | EXPANDER | Ratio, attack/hold/release, side-chain HP/LP, trigger mask | Ratio and threshold only | Partial |
| 44 | KEYED EXPNDR | **Left channel expanded, right channel is the key** | Self-keyed through a high-pass | Differs |
| 45 | INVRS EXPNDR | Boosts levels below threshold for sustain | Same | Match |
| 46 | DE-ESSER | Compresses sibilant band | Same | Match |
| 47 | DUCKER | **Music on the left, voice-over on the right as key**, mono output | Self-keyed compressor | Differs |
| 48 | RUMBLE FILTR | High-pass **and** low-pass, both 24 dB/oct | 24 dB/oct high-pass; fixed 12 dB/oct low-pass | Partial |
| 49 | PARAM EQ | **Four**-band parametric | One band | Differs |
| 50 | VAN DER POL | Adds synthetic high harmonics, transient enhancement | Harmonic exciter | Match |

## ASR-X / ASR-X Pro insert effects

| # | Effect | What [AXU] documents | RSFX | Status |
|---|---|---|---|---|
| 01 | Parametric EQ | **Four bands**: low shelf, two mids (Q 1-40), high shelf | One band | Differs |
| 02-04 | Hall Reverb, Large Room, Small Room | Decay 0-10 s, LF decay, HF damping/bandwidth, diffusion 1/2, definition, detune, pre-delay 0-36 ms, 2 reflections, 3 positions | Tank + 6 fixed reflections; pre-delay to 500 ms | Partial |
| 05-06 | Large Plate, Small Plate | Decay 0-10 s, damping, bandwidth (on the **output**), diffusion, definition, pre-delay 0-36 ms, 4 reflection levels | Plate tank, input bandwidth, no reflections | Partial |
| 07-08 | NonLinReverb1/2 | Single pass through a "density", **nine envelope tap levels** | Recirculating tank cut by an envelope | Differs |
| 09 | Gated Reverb | Threshold, hysteresis, attack/hold/release, decay, slapback, 4 reflections | Hold and threshold; fixed attack/release | Partial |
| 10 | Stereo Chorus | LFO 0-20 Hz, depth 0-25 ms, centre 0-50 ms, spread, L/R phase | Two voices, depth to 6 ms, fixed centre | Partial |
| 11 | 8-VoiceChorus | EQ, high-pass, 8 voices with randomized LFOs, depth/centre to 300 ms, ±feedback | Eight voices, short delays, no EQ/HPF | Partial |
| 12-14 | Rev>Chorus/Flanger/Phaser | Plate → modulation effect, with **System Feedback** from its output back into the reverb | Reverb then modulation, no system feedback | Partial |
| 15-17 | Chorus/Flanger/Phaser>Rev | Modulation effect → reverb with Rev Mix | Same | Match |
| 18 | EQ>Reverb | **Three-band** EQ (shelves + mid) → reverb | One-band EQ → reverb | Partial |
| 19 | Spinner>Rev | **Pseudo-3D panner**: L/R and front/back LFOs, head-size delay, level and filter modulation, cancellation | Simple rotating auto-pan | Partial |
| 20-23 | DDL>Chorus/Flanger/Phaser/EQ | **Four independent delays** (0-630 ms each, level, feedback, damping, pan) → effect | One stereo delay to 2000 ms → effect | Differs |
| 24 | Multi-Tap DDL | EQ, **four diffusers in series feeding a nine-tap delay** | Four plain taps | Differs |
| 25-40 | Dist>Chorus … EQ>Chorus>DDL | Parameter pages not retrieved | Built from the ASR-10-documented blocks | Unverified |

Formant Morph has eight factory presets, including "Vel Vocoder" and "Synced Saws" (Sound On Sound ASR-X review);
RSFX has no presets for it. Chatter Box remains undocumented here.

## ASR-X global reverbs

Names verified ([AXP] p.42). No parameter documentation found, so their voicings are unverified.

## Tally

| | Match | Partial | Differs | Unverified |
|---|---|---|---|---|
| ASR-10 (50) | 8 | 32 | 10 | 0 |
| ASR-X inserts (40) | 3 | 13 | 8 | 16 |
| ASR-X globals (8) | 0 | 0 | 0 | 8 |
