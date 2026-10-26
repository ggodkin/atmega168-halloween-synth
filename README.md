# ATmega168 Halloween Synth

PlatformIO project for an ATmega168 at 8 MHz. Audio cycles through wind, ghost,
thunder, door squeak, bell, and owl, playing each sound for four seconds before
repeating. Audio is output as 31.25 kHz PWM on PD6/OC0A, updated at 8 kHz.

For a 300-ohm speaker, put a 100 uF capacitor in series between PD6 and the
speaker, with the capacitor's positive lead toward PD6; connect the other
speaker lead to GND. The capacitor blocks the PWM output's DC bias. For more
volume, use an audio amplifier rather than connecting a low-impedance speaker
directly to the microcontroller pin.
