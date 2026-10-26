# ATmega168 Halloween Synth

PlatformIO project for an ATmega168 at 8 MHz. An RCWL-0516 motion sensor
activates audio while its OUT pin is HIGH. Connect sensor OUT to Arduino D2 and
connect sensor GND to the ATmega168 board GND. The six sounds (wind, ghost,
thunder, door squeak, bell, and owl) play for four seconds each and repeat while
motion is detected. Playback restarts at wind on each activation and fades out
when motion stops. Audio is output as 31.25 kHz PWM on PD6/OC0A, updated at 8 kHz.

For a 300-ohm speaker, put a 100 uF capacitor in series between PD6 and the
speaker, with the capacitor's positive lead toward PD6; connect the other
speaker lead to GND. The capacitor blocks the PWM output's DC bias. For more
volume, use an audio amplifier rather than connecting a low-impedance speaker
directly to the microcontroller pin.
