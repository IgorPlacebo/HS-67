# HS-02 power-on melody

`startup.rtttl` is played once at power-on, together with the boot splash, at the volume of
*System → Sound* (0 = silent). Any button that skips the splash, or any UI beep, stops it.

Put one RTTTL ringtone in `startup.rtttl` (`name:d=8,o=6,b=140:e,g,a,2b,p,…`; a MIDI export can
be converted to RTTTL with any online converter) and run:

```sh
python3 tools/hs02-ui-sounds/gen_startup_melody.py            # as written
python3 tools/hs02-ui-sounds/gen_startup_melody.py --transpose 12   # one octave up
```

It writes `source/Core/BSP/Fnirsi/Hs02StartupMelody.h` (a `{Hz, ms}` table, ~4 bytes per note)
and `startup_preview.wav`, a rough idea of the sound (the real piezo is brighter and louder
around 2-4 kHz, quiet below ~1 kHz). Each note gets a 12 ms gap so repeated notes stay separate.

The buzzer is one piezo driven by TIM2 (1 MHz tick): one note at a time, no chords. The player
counts note periods in the TIM2 update interrupt, so its timing does not depend on the GUI.

Only use a melody you wrote yourself or that is free to use.
