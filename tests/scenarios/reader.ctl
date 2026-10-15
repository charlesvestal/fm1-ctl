# the screen reader (fm1-reader, announce.c): a reader listens, and each page, knob, key, button and
# transport change is said once, in the screen's own text (the words file says it in full)
wait 200
listen
wait 100
said Play page, bank A: CC 74 cutoff 64, CC 71 resonance 64, CC 1 mod wheel 0, CC 7 volume 100. Channel 1, tempo 120, stopped. Lowest key F3. Program 1, Acoustic Grand
# a knob: its controller's name and its value, once it settles
spin K1 3
wait 300
said CC 74 cutoff 67
# SELECT: the next bank, said as the page (not also as the top bar's KNOB BANK message)
spin SELECT 1
wait 300
said Play page, bank B: CC 73 attack 64, CC 72 release 64, CC 75 decay 64, CC 76 vibrato rate 64
unsaid KNOB BANK
spin SELECT -1
wait 300
# FX held: the controller numbers
press FX
wait 700
said Controller numbers, bank A: Knob 1, CC 74 cutoff, Knob 2, CC 71 resonance
spin K1 1
wait 300
said Knob 1, CC 75 decay
spin K1 -1
release FX
wait 300
said Play page, bank A
# a key: its note is not said (the notes played are never read out)
tapkey 0
wait 200
unsaid F3
tap SEL
wait 300
said Keys page: Root C, Scale chromatic, Velocity 100, Humanize 0
# D dorian: a black key's chord is not said either
spin K1 2
wait 300
said Root D
spin K2 3
wait 300
said Scale dorian
tapkey 1
wait 200
unsaid D minor
tapkey 3
wait 200
unsaid E minor
tapkey 5
wait 200
unsaid F major
# a knob at its end: said again (an answer to every turn)
spin K2 20
wait 300
said Scale blues
spin K2 1
wait 300
said Scale blues
spin K2 -8
wait 300
said Scale chromatic
# ALGORITHM, PRESETS
turn ALGO 1
wait 300
said Channel 2
turn PRESET 52 20
wait 300
said Program 53, Choir Aahs
turn ALGO -1
turn PRESET -52 20
wait 300
# the transport, the buttons' messages
tap PLAY
wait 50
said Play
tap PLAY
wait 50
said Stop
tap OCT+
said LOWEST KEY F4
tap OCT-
said LOWEST KEY F3
press ENV
release ENV
press ENV
wait 30
said SUSTAIN LOCKED
release ENV
press ENV
wait 30
said Sustain unlocked
release ENV
tap REC
wait 50
said All notes off
tap SAVE
said SAVED
# the Sequencer: steps, a step past the end, mutes, a track
tap SEQ
wait 300
said Sequencer page, track 1: Steps 16, Hits 4, Rotate 0, Note kick
tapkey 2
wait 50
said Step 2 on
tapkey 2
wait 50
said Step 2 off
key 8 down
key 8 up
wait 300
said Sequencer page, track 4: Steps 12, Hits 3, Rotate 2, Note clap
tapkey 24
wait 50
said Step 15 not set: track 4 has 12 steps
tapkey 10
wait 50
said Track 1 muted
tapkey 10
wait 50
said Track 1 unmuted
where
said Steps on: 3, 7, 11
# Setup, and its Touch page: the velocity and aftertouch settings, a key's velocity (never the pressure)
tap EDIT
wait 300
said Setup page: Tempo 120, Clock internal and sent out, Seq ch 10, Glow Low
turn SELECT 1
wait 300
said Touch page: Velocity from the keys, Curve 0, Aftertouch per note, AT sens 0
spin K1 -1
wait 300
said Velocity fixed
spin K1 1
wait 300
said Velocity from the keys
spin K2 2
wait 300
said Curve +2
spin K3 1
wait 300
said Aftertouch for the channel
spin K3 -1
wait 300
said Aftertouch per note
spin K4 -1
wait 300
said AT sens -1
key 2 down 90 60
wait 800
unsaid G3
unsaid velocity
unsaid Pressure
key 2 up
wait 100
where
said Touch page: Velocity from the keys, Curve +2, Aftertouch per note, AT sens -1. Last velocity
spin K1 -1
spin K2 -2
spin K3 -1
spin K4 1
wait 300
