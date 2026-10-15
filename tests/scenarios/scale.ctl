# a scale: white keys its steps, black keys its chords
fixed
tap SEL
expect view 1
spin K2 1
expect scale 1
# D dorian
spin K1 2
spin K2 2
key 0 down
expect_out USB 90 32 64
key 0 up
key 2 down
expect_out USB 90 34 64
key 2 up
out
# black key 1: the triad on step 1 (D F A)
key 1 down
expect_out USB 90 32 64
expect_out USB 90 35 64
expect_out USB 90 39 64
key 1 up
expect_out USB 80 39 00
expect notes_on 0
wait 50
press ENV
wait 50
shot scale.png
release ENV
leds
