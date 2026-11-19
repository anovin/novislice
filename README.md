# NoviSlice

A loop-slicing instrument for FL Studio, with MPC-style chopping and pad controls. Drop in a loop and NoviSlice chops it by tempo, into equal regions, on the hits, or wherever you put the markers. Each slice goes on its own key and pad, with its own pitch, level, pan, filter and play mode. Then you can roll, reverse, stop and throw it with 16 performance effects, use Note Repeat and 16 Levels, and drag the slices or a MIDI pattern straight into FL Studio.

The panel has four pages, chosen with the buttons under the control strip: **SOUND** (effects), **CHOP** (slicing, processing and export), **PAD EDIT** (the selected pad) and **PLAY MODES** (polyphony, Note Repeat, 16 Levels, mutes).

## Getting started

1. Add **NoviSlice** to the Channel Rack. It's an instrument, not a mixer effect.
2. Drag a WAV, AIFF, FLAC or MP3 onto the green screen, or press **LOAD**.
3. Check the **Loop BPM**. If the file name contains a tempo (for example `soul_loop_90bpm.wav`), NoviSlice fills it in for you. Otherwise it shows **AUTO** and works the tempo out from **BARS**. The screen shows the loop's tempo, then FL Studio's tempo after `>` while Sync is on (a `?` after the tempo means it's a guess).
4. Play the pads or the piano roll. Slices sit on the **white keys from C5** (C5, D5, E5…). The performance effects sit on **C3–D#4**.

The sample is saved inside your FL project, so projects still open if the file moves.

## Slicing and playback

| Control | What it does |
|---|---|
| **PLAY** | Plays the loaded sample from start to finish (between the trim points), through Sync, Pitch and the effects. With **LOOP** on it goes round and round seamlessly (turn LOOP off and it finishes the current pass). It lights while playing; click again to stop. The screen shows the time. |
| **LOOP BPM** | The tempo of the sample. Drag it up or down, click the arrows (Shift for 0.1 steps), or double-click and type it. With Sync on, the loop plays at FL Studio's tempo. Clicking the up arrow while it shows AUTO locks in the guessed tempo. Go below 40, or type nothing, to return to AUTO. |
| **BARS / PER BAR** | The BPM grid: how many bars to slice (1–8 or 16) and how many slices per bar (1–8), up to 32 slices. With a Loop BPM set, bars are counted at that tempo, and anything past them shows as NOT SLICED. On AUTO, BARS also sets the guessed tempo. Slices 17–32 are on pad bank **B**. |
| **TRIPLET** | Switches to a triplet grid: 2, 4 or 8 slices per bar become 3, 6 or 12. |
| **FULL** | Keeps playing past the end of the slice to the end of the loop. |
| **LOOP** | Loops the slice while the key is held. |
| **SYNC** | Plays the loop at FL Studio's tempo, using the Loop BPM, without changing its tone: an 80 BPM loop in a 95 BPM session keeps up with FL. How it gets there is set by STRETCH. Turn Sync off to always play the loop at its own speed. |
| **STRETCH** | **BEATS** (default, for drums): every slice plays untouched and only the timing between slices follows FL's tempo, like ReCycle/REX files or MPC chops, so drum hits stay perfectly sharp. Faster tempos trim each slice; slower ones leave a little space after it. Pitch works like an MPC (varispeed), but the groove never changes speed. **TONAL** (for chords, vocals, melodic loops): a spectral stretcher (Signalsmith Stretch) that changes speed and pitch smoothly and independently. It sounds best within about 0.75× to 1.5× of the loop's tempo. |
| Click the screen | Auditions the slice you click and selects its pad. |

By default playback is mono: a new slice cuts off the one before it, with a short crossfade. Switch to POLY on the PLAY MODES page to let pads overlap.

## Chop page (MPC Chop Mode)

| Control | What it does |
|---|---|
| **MODE** | *BPM GRID* slices by tempo (Loop BPM, BARS, PER BAR). *REGIONS* cuts into equal pieces. *TRANSIENT* puts a slice on every hit. *MANUAL* uses the markers you place. |
| **REGIONS** | How many equal regions (REGIONS mode). |
| **SENSITIVITY / MIN TIME** | Transient mode: higher sensitivity finds quieter hits; MIN TIME is the shortest slice allowed. |
| **TRIM START / END** | The part of the sample that gets sliced. You can also drag the **S** and **E** flags on the screen. |
| **0 SNAP** | Moves slice points to the nearest zero crossing, which avoids clicks. |
| **LINK** | When on, moving a slice's start also moves the end of the slice before it. Turn it off to make gaps or overlaps. |
| **SPLIT / COMBINE / CLEAR** | Split the selected slice in two, merge it with the one before, or start again with a single slice. |
| On the screen | Drag a marker to move it, double-click to add one, Alt-click a marker to delete it. Moving a marker switches to MANUAL, starting from the current slices. |
| **NORMALIZE / REVERSE / TO MONO** | Change the sample itself (Normalize to -0.1 dB, reverse it, or make it mono from both sides, the left or the right). The changed audio is saved in your project. |
| **EXPORT WAVS** | Saves every slice as its own WAV file in a folder you choose. |
| **DRAG MIDI** | Drag this button into FL Studio's piano roll or playlist to get a MIDI pattern that plays the slices in order, at the loop's tempo. |
| Drag a pad out | Drag any pad out of NoviSlice to drop its slice into FL Studio as a WAV. |

## Pad Edit page

Settings for the selected pad (click a pad, or a slice on the screen, to select it). The readout next to the page buttons shows which pad, slice and key it is.

| Control | What it does |
|---|---|
| **TUNE / LEVEL / PAN / CUTOFF / TAIL** | The pad's own pitch (±24 semitones, fine steps), volume, pan, low-pass filter, and extra audio after the slice ends so chops don't stop abruptly. Tune never changes the timing. |
| **REVERSE / LOOP / MUTE / SOLO** | Play the slice backwards, loop it while held, mute the pad, or solo it (only soloed pads sound). |
| **PLAY MODE** | *NOTE ON* plays while held, *ONE SHOT* plays the whole slice from one hit, *NOTE OFF* plays when you let go. |
| **MUTE GROUP** | Pads in the same group (1–8) cut each other off, like open and closed hi-hats. |
| **REPEATS** | With LOOP on: *HOLD* loops while you hold the pad, or 1X–16X plays the slice that many times. |
| **MOTION / CYCLES** | Slice Motion: each hit plays the *NEXT* slice or a *RANDOM* one, moving through CYCLES slices from the pad's own. |
| **RESET PAD / COPY TO ALL** | Put the pad back to normal, or copy its settings to every pad (mutes and solos stay as they are). |

Pads show small tags for their settings: **M** mute, **S** solo, **1** one shot, **R** note off, **>** slice motion.

## Play Modes page

| Control | What it does |
|---|---|
| **POLYPHONY** | MONO (a new pad cuts the last one) or POLY (pads overlap, and mute groups still cut). |
| **FULL LEVEL / VEL SENS** | Full Level plays every hit at full volume. Vel Sens sets how much velocity matters. |
| **NOTE REPEAT / RATE / SWING** | Hold a pad to retrigger it at 1/4 to 1/32 or triplets, in time with FL Studio. Swing (50–75%, MPC-style) works on the straight rates. |
| **16 LEVELS / TYPE** | Spreads one pad over pads 1–16 at rising *VELOCITY*, *TUNE* (pad 9 is the original pitch), *FILTER* or *DECAY*. Turning it on uses the selected pad; **USE SELECTED PAD** changes it. |
| **CLEAR MUTES / CLEAR SOLOS** | Unmute or unsolo every pad. |
| **SHUFFLE / RESET ORDER** | Shuffles which slice each key plays, or puts them back in order. Pads show the slice they play. |

## Sound page

- **Texture:** *Amount* turns on a granular effect that scatters little grains of the slice for a smeared, lo-fi sound (it replaces the STRETCH engine while it's above 0). *Grain* sets the grain size for it. *Vinyl* adds crackle and hiss while slices play.
- **Play:** *Pitch* (±24 semitones) changes the pitch but never the tempo, so slices stay in time. *Glide* smooths pitch changes, and *X-Fade* sets the fade at slice edges and between slices.
- **Filter:** LP, HP or BP with resonance. An LFO synced to the tempo (1/1 to 1/16) can move the cutoff.
- **Crush:** bit depth and sample-rate reduction.
- **Delay:** tempo-synced (1/16 to 1/2, including dotted values), with mix and feedback.
- **Amp Env:** attack and release for each slice.
- **Output:** level and meter.

Double-click any knob to reset it.

## Performance effects (C3–D#4)

Hold a button or key to use an effect. **Shift-click** a button to latch it on, and shift-click again to release it.

| Key | Effect | Key | Effect |
|---|---|---|---|
| C3 | Reverse | G#3 | Tape stop (over one beat) |
| C#3 | Roll 1/4 | A3 | Half speed |
| D3 | Roll 1/8 | A#3 | Double speed |
| D#3 | Roll 1/16 | B3 | Dive (pitch drops an octave) |
| E3 | Roll 1/32 | C4 | Sweep (filter sweep over two beats) |
| F3 | Roll 1/8T | C#4 | Gate (1/16 chop) |
| F#3 | Roll 1/16T | D4 | Crush (hard bitcrush) |
| G3 | Burst (accelerating roll) | D#4 | Throw (sends to the delay and lets the tail ring) |

Rolls, the gate and tape stop follow FL Studio's tempo.

## Install (Mac)

1. Download `NoviSlice-mac.zip` from the repo's **Releases** page ("latest") and unzip it.
2. Right-click `Install-NoviSlice.command` and choose **Open**. You only need to right-click the first time, because the build isn't notarised by Apple.
3. In FL Studio, go to **Options → Manage plugins → Find installed plugins**, then add NoviSlice to the Channel Rack.

## Building from source

**Easiest on your own Mac:** right-click `packaging/Build-NoviSlice-on-Mac.command` and choose **Open**. It builds a universal VST3 and AU, installs both, and logs to `build.log`.

Or build by hand (needs CMake 3.22+ and the Xcode command line tools):

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target NoviSlice_VST3 NoviSlice_AU
```

The engine has a standalone test: `cmake --build build --target EngineTest && ./build/EngineTest`.

Time-stretching in TONAL mode uses [Signalsmith Stretch](https://github.com/Signalsmith-Audio/signalsmith-stretch) and Signalsmith Linear (MIT License, in `ThirdParty/` with their licence files).

Built with JUCE 8 (free for personal use under its licence terms). The fonts (Barlow Condensed and Share Tech Mono) are under the SIL Open Font License; their licence files are in `Resources/fonts`.
