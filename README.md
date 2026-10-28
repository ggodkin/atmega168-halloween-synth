# ATmega168 Halloween Synth

Bare-metal AVR firmware for an ATmega168 at 8 MHz; no Arduino framework is
required. Connect the RCWL-0516 VCC to a suitable 5 V supply, GND to board GND,
and OUT to PD2 (Arduino D2). A low-to-high transition starts one complete
sequence: wind and ghost (four seconds each), thunder (three seconds), then
door squeak, bell, and owl (four seconds each).
If OUT is already HIGH at startup, it starts the first sequence. Sensor
transitions during playback are ignored; another sequence can start only after
completion and a new low-to-high transition. Audio is output as 31.25 kHz PWM
on PD6/OC0A, updated at 8 kHz.

Thunder uses a loud, 300-sample high-passed noise crack with an initial click,
followed by a slow, fading rumble with three low-frequency gain swells.

The oscillator uses a 64-entry quarter-wave sine table in flash. The sample
rate remains 8 kHz because some sound paths use AVR 16-bit division helpers.
At 11.025 kHz, only about 725 cycles are available per sample, so raising the
rate requires optimizing and rechecking the ISR budget.

Noise combines an independent xorshift32 generator and a 16-bit Galois LFSR to
produce a less repetitive white-noise source. Three parallel low-pass stages
mix that source into a pink-noise approximation for the sound effects; their
filter rates and mix weights are in `include/config.h`.

Wind uses two overlapping gust rates and separate low-pass noise bands for
slow air movement and higher-frequency hiss. Their rates and mix levels are
configurable in `include/config.h`.

The ghost combines two slightly detuned voices, vibrato-locked tremolo, and a
gradual amplitude decay across its sound slot.

The door effect layers three overlapping creaks with irregular pitch jitter,
brief catches, and a short decaying metallic resonance.

The bell adds a fourth inharmonic partial, small randomized partial detunes on
each strike, exponential ring-down, and a brief noise-and-tone strike transient.

The owl uses paired, rounded hoots with a brief upward pitch inflection,
downward glide, and subtle second-harmonic body; the second call is lower and
lightly vibrato-modulated. No noise is mixed into the calls.

Edit `include/config.h` to change the sensor/PWM pins, sample timing, sound
durations, and per-sound tuning parameters. PlatformIO build and upload options
remain in `platformio.ini`.

For a 300-ohm speaker, put a 100 uF capacitor in series between PD6 and the
speaker, with the capacitor's positive lead toward PD6; connect the other
speaker lead to GND. The capacitor blocks the PWM output's DC bias. For more
volume, use an audio amplifier rather than connecting a low-impedance speaker
directly to the microcontroller pin.
