# ATmega168 Halloween Synth

Bare-metal AVR firmware for an ATmega168 at 8 MHz; no Arduino framework is
required. Connect the RCWL-0516 VCC to a suitable 5 V supply, GND to board GND,
and OUT to PD2 (Arduino D2). A low-to-high transition starts one complete
sequence: wind, ghost, door squeak, bell, and owl (four seconds each).
If OUT is already HIGH at startup, it starts the first sequence. Sensor
transitions during playback are ignored; another sequence can start only after
completion and a new low-to-high transition. Audio is output as 31.25 kHz PWM
on PD6/OC0A, updated at 8 kHz.
An LED connected from 5 V through a series resistor to PD5/OC0B continuously
fades from off to full brightness over two seconds and back to off over two
seconds throughout the entire sound sequence, without restarting at effect
boundaries. This active-low connection keeps the LED off while idle.

The oscillator uses a 64-entry quarter-wave sine table in flash. The sample
rate remains 8 kHz because some sound paths use AVR 16-bit division helpers.
At 11.025 kHz, only about 725 cycles are available per sample, so raising the
rate requires optimizing and rechecking the ISR budget.

Noise combines an independent xorshift32 generator and a 16-bit Galois LFSR to
produce a less repetitive white-noise source. Three parallel low-pass stages
mix that source into a pink-noise approximation for the sound effects; their
filter rates and mix weights are in `include/config.h`.

Wind uses two overlapping gust rates and separate low-pass noise bands for
slow air movement and higher-frequency hiss. Its corrected mix scaling makes
the gust-shaped airflow clearly audible instead of leaving it as faint hiss.
Gust rates and mix levels are configurable in `include/config.h`.

The ghost uses the owl's wavetable voice and shaped pitch contour, with a
lower-then-higher paired call separated by a short pause.

The door effect plays a two-second recorded squeak from a flash-resident,
low-pass-resampled sample table. It plays once at the beginning of the
four-second door slot.

The bell adds a fourth inharmonic partial, small randomized partial detunes on
each strike and exponential ring-down. Its partials now fade in over a smooth
128 ms curve, with only a quiet filtered-noise strike transient.

The owl replays the first second of the supplied recording from a 4 kHz,
flash-resident sample table, linearly interpolated to the 8 kHz audio rate and
looped through its four-second sound slot.

Edit `include/config.h` to change the sensor/PWM pins, sample timing, sound
durations, and per-sound tuning parameters. PlatformIO build and upload options
remain in `platformio.ini`.

For a 300-ohm speaker, put a 100 uF capacitor in series between PD6 and the
speaker, with the capacitor's positive lead toward PD6; connect the other
speaker lead to GND. The capacitor blocks the PWM output's DC bias. For more
volume, use an audio amplifier rather than connecting a low-impedance speaker
directly to the microcontroller pin.
