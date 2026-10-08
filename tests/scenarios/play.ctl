# keys, octave, velocity, channel, program, knobs, sustain, bend, transport, panic
wait 300
shot boot.png
key 0 down
expect_out USB 90 35 64
key 0 up
expect_out USB 80 35 00
out
tap OCT+
expect octave 1
key 7 down
expect_out USB 90 48 64
tap OCT-
key 7 up
# the note-off is the note that was sent, not the key's new note
expect_out USB 80 48 00
out
spin SELECT 20
expect vel 120
spin ALGO 2
expect chan 2
tapkey 0
expect_out USB 92 35 78
spin PRESET 5
expect_out USB C2 05 00
spin K1 -10
expect_out USB B2 4A 36
spin K3 3
expect_out USB B2 01 03
out
press ARP
expect_out USB B2 40 7F
expect sustain 1
release ARP
expect_out USB B2 40 00
press ENV
expect_out USB E2 00 00
press LFO
expect_out USB E2 7F 7F
release LFO
expect_out USB E2 00 00
release ENV
expect_out USB E2 00 40
out
tap PLAY
expect_out USB FA
expect playing 1
key 3 down
tap REC
expect_out USB 82 38 00
expect_out USB B2 7B 00
out
tap PLAY
expect_out USB FC
wait 200
shot play.png
leds
