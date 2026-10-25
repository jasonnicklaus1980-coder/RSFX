# Algorithm reference

Generated from `src/algos.h` by `make docs`. The Q-Link order is the parameter order below; a knob the algorithm
doesn't use shows `--` on the MPC.


## ASR-10 ROM effects (Musician's Manual Section 5, pp.3-4)

| # | Name | Chain | Knobs in use |
|---|---|---|---|
| 1 | 10:HALL REVERB | Reverb | Decay, Damping, PreDelay |
| 2 | 10:44KHZ REVERB | Reverb | Decay, Damping, PreDelay |
| 3 | 10:ROOM REVERB | Reverb | Decay, Damping, PreDelay |
| 4 | 10:DUAL DELAYS | Delay | Damping, Delay, Feedback, Balance (R/L time) |
| 5 | 10:44KHZ DELAYS | Delay | Damping, Delay, Feedback, Balance (R/L time) |
| 6 | 10:CHORUS+REVRB | Chorus → Reverb | Decay, Damping, PreDelay, ModRate, ModDepth, Reso, Balance (send) |
| 7 | 10:PHASER+REVRB | Phaser (12-pole) → Reverb | Decay, Damping, PreDelay, ModRate, ModDepth, Freq, Reso, Balance (send) |
| 8 | 10:FLANGER+REV | Flanger → Reverb | Decay, Damping, PreDelay, ModRate, ModDepth, Freq, Reso, Balance (send) |
| 9 | 10:ROT. SPKR+REV | Rotary speaker → Reverb | Decay, Damping, PreDelay, ModRate, ModDepth, Drive, Balance (send) |
| 10 | 10:CHOR+REV+DDL | Chorus → Reverb → Delay | Decay, Damping, PreDelay, Delay, Feedback, ModRate, ModDepth, Reso, Balance (send) |
| 11 | 10:CMP+DIST+REV | Compressor → Distortion → Reverb | Decay, Damping, PreDelay, Drive, Freq, Reso, Thresh, Balance (send) |
| 12 | 10:DIST+CHO+REV | Distortion → Chorus → Reverb | Decay, Damping, PreDelay, ModRate, ModDepth, Drive, Freq, Reso, Balance (send) |
| 13 | 10:WAH+DIST+REV | Wah → Distortion → Reverb | Decay, Damping, PreDelay, ModDepth, Drive, Freq, Reso, Balance (send) |
| 14 | 10:SMALL ROOM | Reverb | Decay, Damping, PreDelay |
| 15 | 10:LARGE ROOM | Reverb | Decay, Damping, PreDelay |
| 16 | 10:HALL REVERB2 | Reverb | Decay, Damping, PreDelay |
| 17 | 10:SMALL PLATE | Reverb | Decay, Damping, PreDelay |
| 18 | 10:LARGE PLATE | Reverb | Decay, Damping, PreDelay |
| 19 | 10:REVRSE REVRB | Reverse reverb | Decay, Damping, Thresh |
| 20 | 10:REVRSE REVB2 | Reverse reverb | Decay, Damping, Thresh |
| 21 | 10:GATED REVERB | Gated reverb | Decay, Damping, PreDelay, Thresh |
| 22 | 10:NLIN REVRB 1 | Non-linear reverb | Decay, Damping, PreDelay |
| 23 | 10:NLIN REVRB 2 | Non-linear reverb | Decay, Damping, PreDelay |
| 24 | 10:NLIN REVRB 3 | Non-linear reverb | Decay, Damping, PreDelay |
| 25 | 10:MULTITAP DDL | Multi-tap delay | Damping, Delay, Feedback |
| 26 | 10:EQ+DELAY LFO | EQ → LFO delay | Delay, Feedback, ModRate, ModDepth, Drive, Freq, Balance (send) |
| 27 | 10:VCF+DISTORT | Resonant VCF → Distortion | ModDepth, Drive, Freq, Reso |
| 28 | 10:GUITAR AMP1 | Guitar amp | Drive, Freq, Thresh |
| 29 | 10:GUITAR AMP2 | Guitar amp | Drive, Freq, Thresh |
| 30 | 10:GUITAR AMP3 | Guitar amp | Drive, Freq, Thresh |
| 31 | 10:SPKR CABINET | Speaker cabinet |  |
| 32 | 10:TUNABLE SPKR | Tunable speaker | Drive, Freq, Reso |
| 33 | 10:EQ+CHOR+DDL | EQ → Chorus → Delay | Damping, Delay, Feedback, ModRate, ModDepth, Drive, Freq, Reso, Balance (send) |
| 34 | 10:EQ+VIBR+DDL | EQ → Vibrato → Delay | Damping, Delay, Feedback, ModRate, ModDepth, Drive, Freq, Balance (send) |
| 35 | 10:EQ+FLNGR+DDL | EQ → Flanger → Delay | Damping, Delay, Feedback, ModRate, ModDepth, Drive, Freq, Reso, Balance (send) |
| 36 | 10:EQ+TREM+DDL | EQ → Tremolo → Delay | Damping, Delay, Feedback, ModRate, ModDepth, Drive, Freq, Balance (send) |
| 37 | 10:PHASER+DDL | Phaser (12-pole) → Delay | Damping, Delay, Feedback, ModRate, ModDepth, Freq, Reso, Balance (send) |
| 38 | 10:8-VOICE CHOR | Chorus | ModRate, ModDepth, Reso |
| 39 | 10:PITCH SHIFT | Pitch shifter | ModDepth, Pitch |
| 40 | 10:PITCH+DDL | Pitch shifter → Delay | Damping, Delay, Feedback, ModDepth, Pitch, Balance (send) |
| 41 | 10:FAST PITCHSH | Pitch shifter | ModDepth, Pitch |
| 42 | 10:EQ+CMPRESSOR | EQ → Compressor | Drive, Freq, Reso, Thresh |
| 43 | 10:EXPANDER | Expander | Reso, Thresh |
| 44 | 10:KEYED EXPNDR | Keyed expander | Freq, Reso, Thresh |
| 45 | 10:INVRS EXPNDR | Inverse expander | Reso, Thresh |
| 46 | 10:DE-ESSER | De-esser | Freq, Thresh |
| 47 | 10:DUCKER | Ducker | Reso, Thresh |
| 48 | 10:RUMBLE FILTR | Rumble filter | Freq |
| 49 | 10:PARAM EQ | Parametric EQ | Drive, Freq, Reso |
| 50 | 10:VAN DER POL | Harmonic exciter | Drive, Freq |

## ASR-X / ASR-X Pro insert effects (Reference Manual ch.4, p.41)

| # | Name | Chain | Knobs in use |
|---|---|---|---|
| 51 | X:Parametric EQ | Parametric EQ | Drive, Freq, Reso |
| 52 | X:Hall Reverb | Reverb | Decay, Damping, PreDelay |
| 53 | X:Large Room | Reverb | Decay, Damping, PreDelay |
| 54 | X:Small Room | Reverb | Decay, Damping, PreDelay |
| 55 | X:Large Plate | Reverb | Decay, Damping, PreDelay |
| 56 | X:Small Plate | Reverb | Decay, Damping, PreDelay |
| 57 | X:NonLinReverb1 | Non-linear reverb | Decay, Damping, PreDelay |
| 58 | X:NonLinReverb2 | Non-linear reverb | Decay, Damping, PreDelay |
| 59 | X:Gated Reverb | Gated reverb | Decay, Damping, PreDelay, Thresh |
| 60 | X:Stereo Chorus | Chorus | ModRate, ModDepth, Reso |
| 61 | X:8-VoiceChorus | Chorus | ModRate, ModDepth, Reso |
| 62 | X:Rev>Chorus | Reverb → Chorus | Decay, Damping, PreDelay, ModRate, ModDepth, Reso, Balance (send) |
| 63 | X:Rev>Flanger | Reverb → Flanger | Decay, Damping, PreDelay, ModRate, ModDepth, Freq, Reso, Balance (send) |
| 64 | X:Rev>Phaser | Reverb → Phaser (12-pole) | Decay, Damping, PreDelay, ModRate, ModDepth, Freq, Reso, Balance (send) |
| 65 | X:Chorus>Rev | Chorus → Reverb | Decay, Damping, PreDelay, ModRate, ModDepth, Reso, Balance (send) |
| 66 | X:Flanger>Rev | Flanger → Reverb | Decay, Damping, PreDelay, ModRate, ModDepth, Freq, Reso, Balance (send) |
| 67 | X:Phaser>Rev | Phaser (12-pole) → Reverb | Decay, Damping, PreDelay, ModRate, ModDepth, Freq, Reso, Balance (send) |
| 68 | X:EQ>Reverb | EQ → Reverb | Decay, Damping, PreDelay, Drive, Freq, Balance (send) |
| 69 | X:Spinner>Rev | Spinner (auto-pan) → Reverb | Decay, Damping, PreDelay, ModRate, ModDepth, Balance (send) |
| 70 | X:DDL>Chorus | Delay → Chorus | Damping, Delay, Feedback, ModRate, ModDepth, Reso, Balance (send) |
| 71 | X:DDL>Flanger | Delay → Flanger | Damping, Delay, Feedback, ModRate, ModDepth, Freq, Reso, Balance (send) |
| 72 | X:DDL>Phaser | Delay → Phaser (12-pole) | Damping, Delay, Feedback, ModRate, ModDepth, Freq, Reso, Balance (send) |
| 73 | X:DDL>EQ | Delay → EQ | Damping, Delay, Feedback, Drive, Freq, Balance (send) |
| 74 | X:Multi-Tap DDL | Multi-tap delay | Damping, Delay, Feedback |
| 75 | X:Dist>Chorus | Distortion → Chorus | ModRate, ModDepth, Drive, Freq, Reso |
| 76 | X:Dist>Flanger | Distortion → Flanger | ModRate, ModDepth, Drive, Freq, Reso |
| 77 | X:Dist>Phaser | Distortion → Phaser (12-pole) | ModRate, ModDepth, Drive, Freq, Reso |
| 78 | X:Dist>Auto Wah | Distortion → Wah | ModDepth, Drive, Freq, Reso |
| 79 | X:ResVCF>DDL | Resonant VCF → Delay | Damping, Delay, Feedback, ModDepth, Freq, Reso, Balance (send) |
| 80 | X:Dist>VCF>DDL | Distortion → Resonant VCF → Delay | Damping, Delay, Feedback, ModDepth, Drive, Freq, Reso, Balance (send) |
| 81 | X:Pitch Detuner | Pitch detuner | ModDepth |
| 82 | X:Chatter Box | Chatter box | ModRate, ModDepth |
| 83 | X:Formant Morph | Formant morph | ModRate, ModDepth, Freq |
| 84 | X:RotarySpeaker | Rotary speaker | ModRate, ModDepth, Drive |
| 85 | X:Tunable Spkr | Tunable speaker | Drive, Freq, Reso |
| 86 | X:Guitar Amp | Guitar amp | Drive, Freq, Thresh |
| 87 | X:Dist>DDL>Trem | Distortion → Delay → Tremolo | Damping, Delay, Feedback, ModRate, ModDepth, Drive, Freq, Balance (send) |
| 88 | X:Comp>Dist>DDL | Compressor → Distortion → Delay | Damping, Delay, Feedback, Drive, Freq, Reso, Thresh, Balance (send) |
| 89 | X:EQ>Comp>Gate | EQ → Compressor → Gate | Drive, Freq, Reso, Thresh |
| 90 | X:EQ>Chorus>DDL | EQ → Chorus → Delay | Damping, Delay, Feedback, ModRate, ModDepth, Drive, Freq, Reso, Balance (send) |

## ASR-X / ASR-X Pro global reverbs (Reference Manual ch.4, p.42)

| # | Name | Chain | Knobs in use |
|---|---|---|---|
| 91 | XG:SmoothPlate | Reverb | Decay, Damping, PreDelay |
| 92 | XG:Large Hall | Reverb | Decay, Damping, PreDelay |
| 93 | XG:Small Hall | Reverb | Decay, Damping, PreDelay |
| 94 | XG:Big Room | Reverb | Decay, Damping, PreDelay |
| 95 | XG:Small Room | Reverb | Decay, Damping, PreDelay |
| 96 | XG:Reflections | Reverb | Decay, Damping, PreDelay |
| 97 | XG:Bright | Reverb | Decay, Damping, PreDelay |
| 98 | XG:Huge Place | Reverb | Decay, Damping, PreDelay |
