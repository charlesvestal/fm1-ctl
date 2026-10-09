# ENV: sustain while held; a double tap locks it on, a press unlocks it (sustain then ends at the release)
wait 300
tap ENV
expect sustain 0
wait 500
# two taps 80 ms apart: locked
tap ENV
tap ENV
expect sus_lock 1
expect sustain 1
expect_out USB B0 40 7F
out
wait 1000
expect sustain 1
wait 1500
shot locked.png
leds
# one press unlocks: held as usual, off at the release
press ENV
expect sus_lock 0
expect sustain 1
release ENV
expect sustain 0
expect_out USB B0 40 00
out
# a slow second tap does not lock
tap ENV
wait 400
tap ENV
expect sus_lock 0
expect sustain 0
# locked, then REC (panic) unlocks
wait 500
tap ENV
tap ENV
expect sus_lock 1
tap REC
expect sus_lock 0
expect sustain 0
