# NoviSlice

A loop-slicing instrument for FL Studio. Drop in a loop and NoviSlice chops it onto a grid. Each slice goes on its own key and pad, so you can replay the loop in a new order, then roll, reverse, stop and throw it with 16 performance effects.

## Getting started

1. Add **NoviSlice** to the Channel Rack. It's an instrument, not a mixer effect.
2. Drag a WAV, AIFF, FLAC or MP3 onto the green screen, or press **LOAD**.
3. Check the **Loop BPM**. If the file name contains a tempo (for example `soul_loop_90bpm.wav`), NoviSlice fills it in for you. Otherwise it shows **AUTO** and works the tempo out from **BARS**. The screen shows the loop's tempo, then FL Studio's tempo after `>` while Sync is on (a `?` after the tempo means it's a guess).
4. Play the pads or the piano roll. Slices sit on the **white keys from C5** (C5, D5, E5…). The performance effects sit on **C3–D#4**.

The sample is saved inside your FL project, so projects still open if the file moves.

## Slicing and playback

| Control | What it does |
|---|---|
| **LOOP BPM** | The tempo of the sample. Drag it up or down, click the arrows (Shift for 0.1 steps), or double-click and type it. With Sync on, the loop plays at FL Studio's tempo. Clicking the up arrow while it shows AUTO locks in the guessed tempo. Go below 40, or type nothing, to return to AUTO. |
| **BARS / PER BAR** | Sets the slice grid: 1–8 bars × 1–8 slices per bar, up to 32 slices. When Loop BPM is set, the grid follows the tempo instead and BARS is greyed out. Slices 17–32 are on pad bank **B**. |
| **TRIPLET** | Switches to a triplet grid: 2, 4 or 8 slices per bar become 3, 6 or 12. |
| **FULL** | Keeps playing past the end of the slice to the end of the loop. |
| **LOOP** | Loops the slice while the key is held. |
| **SYNC** | Plays the loop at FL Studio's tempo, using the Loop BPM. |
| **KEY LOCK** | Keeps the original pitch when Sync changes the speed. With it off you get the classic sampler sound, where slowing down to match FL Studio also lowers the pitch. The Pitch knob never changes the tempo either way. |
| **SHUFFLE / RESET** | Shuffles which slice each key plays, or puts them back in order. Pads show the slice they play. |
| Click the screen | Auditions the slice you click. |

Playback is mono: a new slice cuts off the one before it, with a short crossfade.

## Sound shaping

- **Texture:** *Amount* scatters the grains for a smeared, granular sound. *Grain* sets the grain size used by Key Lock and Texture. *Vinyl* adds crackle and hiss while slices play.
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

Built with JUCE 8 (free for personal use under its licence terms). The fonts (Barlow Condensed and Share Tech Mono) are under the SIL Open Font License; their licence files are in `Resources/fonts`.
