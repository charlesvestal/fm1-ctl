# chromatic keys, octave, channel, program, knob banks, sustain, panic, MASTER as a controller
wait 300
shot boot.png
key 0 down
expect_out USB 90 35 64
key 0 up
expect_out USB 80 35 00
tap OCT+
expect octave 1
key 7 down
expect_out USB 90 48 64
tap OCT-
key 7 up
# the note-off is the note that was sent, not the key's new note
expect_out USB 80 48 00
out
spin ALGO 2
expect chan 2
tapkey 0
expect_out USB 92 35 64
spin PRESET 5
expect_out USB C2 05 00
spin K1 -10
expect_out USB B2 4A 36
out
# FX: the next bank (B: 73 72 75 76)
tap FX
expect bank 1
spin K1 3
expect_out USB B2 49 43
tap FX
tap FX
tap FX
expect bank 0
press ENV
expect_out USB B2 40 7F
expect sustain 1
release ENV
expect_out USB B2 40 00
out
key 3 down
tap REC
expect_out USB 82 38 00
expect_out USB B2 7B 00
out
# MASTER: the onboard synth's volume while Sound is on (the default); with Sound off, the Setup page's
# KNOB 3 gives it a controller
master 2000
expect notes_out 0
tap GLO
tap GLO
expect view 5
spin K3 11
master 4095
tap GLO
tap GLO
expect view 7
spin K1 2
tap GLO
tap GLO
expect view 5
master 2000
master 4095
expect_out USB B2 0B 7F
master 0
expect_out USB B2 0B 00
tap HOME
wait 200
shot play.png
leds
# the onboard GM synth: Sound back on (Keys + in), a held key sounds
tap GLO
tap GLO
tap GLO
tap GLO
expect view 7
spin K1 -2
key 12 down
wait 300
expect voices >0
shot sound.png
key 12 up
wait 100
