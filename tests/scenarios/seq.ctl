# the sequencer: four Euclidean tracks on channel 10, clock out, keys edit steps, mute, external clock
tap SEQ
expect view 3
expect pat0 4369
out
wait 500
# Int: the clock goes out all the time, 24 a beat (120 BPM: about 24 in half a second)
expect clocks_out >20
expect clocks_out <26
tap PLAY
expect_out USB FA
wait 1000
expect_out USB 99 24 64
expect_out USB 99 26 64
expect_out USB 99 2A 64
out
# track 1 (snare, black key 2 = key 3): two hits; step 0 by hand
key 3 down
key 3 up
expect track 1
expect pat1 4112
key 0 down
key 0 up
expect pat1 4113
# KNOB 2 regenerates it: 4 hits
spin K2 2
expect pat1 4369
# mute track 2 (the hat), black key 7 = key 15
key 15 down
key 15 up
expect mute2 1
out
wait 1000
expect notes_on 0
tap PLAY
expect_out USB FC
wait 50
shot seq.png
leds
# external clock: follows MIDI clock and start from the TRS jack
tap EDIT
spin K1 2
out
wait 300
expect clocks_out 0
midi TRS FA
midi TRS F8
expect_out USB 99 24 64
