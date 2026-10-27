# ATmega168 Halloween Synth

Bare-metal AVR firmware for an ATmega168 at 8 MHz; no Arduino framework is
required. An RCWL-0516 motion sensor activates audio while its OUT pin is HIGH.
Connect sensor OUT to PD2 (Arduino D2) and sensor GND to board GND. The six
sounds (wind, ghost, thunder, door squeak, bell, and owl) play for four seconds
each and repeat while motion is detected. Playback restarts at wind on each
activation and fades out when motion stops. Audio is output as 31.25 kHz PWM on
PD6/OC0A, updated at 8 kHz.

Edit `include/config.h` to change the sensor/PWM pins, sample timing, sound
durations, and per-sound tuning parameters. PlatformIO build and upload options
remain in `platformio.ini`.

For a 300-ohm speaker, put a 100 uF capacitor in series between PD6 and the
speaker, with the capacitor's positive lead toward PD6; connect the other
speaker lead to GND. The capacitor blocks the PWM output's DC bias. For more
volume, use an audio amplifier rather than connecting a low-impedance speaker
directly to the microcontroller pin.
