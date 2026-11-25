# SMK-37: full keyboard, velocity, controls and saved settings
wait 300
out
key37 0 down 35
expect_out USB 90 30 23
key37 0 up
expect_out USB 80 30 00
key37 5 down 70
expect_out USB 90 35 46
key37 5 up
key37 36 down 120
expect_out USB 90 54 78
tap OCT+
key37 36 up
expect_out USB 80 54 00
tap OCT-
out
tap SEL
spin K2 1
key37 0 down 80
expect_out USB 90 2B 50
key37 0 up
key37 1 down 90
expect_out USB 90 2B 5A
expect_out USB 90 2F 5A
expect_out USB 90 32 5A
key37 1 up
tap HOME
out
pad 0 down 110
expect_out USB 99 24 6E
pad 0 press 90
expect_out USB A9 24 5A
tap GLO
spin K3 -1
pad 0 up
expect_out USB 89 24 00
spin K3 1
tap HOME
pad 15 down 45
tap REC
expect_out USB 89 33 00
out
wait 30
expect notes_out 0
pad 15 up
out
fader 0 90
expect_out USB B0 4A 5A
fader 3 80
expect_out USB B0 07 50
tap FBANK
fader 0 30
expect_out USB B0 49 1E
tap KBANK
expect bank 1
tap RIGHT
expect program 1
expect_out USB C0 01 00
tap LEFT
expect bank 1
expect program 0
expect_out USB C0 00 00
out
spin K5 1
expect_out USB B0 1C 41
spin K6 2
expect_out USB B0 1D 42
spin K7 3
expect_out USB B0 1E 43
spin K8 4
expect_out USB B0 1F 44
expect bank 1
expect chan 0
expect program 0
press FX
spin K5 1
release FX
expect smk_cc0 29
expect bank 1
out
spin K5 1
expect_out USB B0 1D 42
out
spin K5 -1
expect_out USB B0 1D 41
spin K5 1
expect_out USB B0 1D 42
tap GLO
expect view 5
tap RIGHT
expect setup_pg 1
tap LEFT
expect setup_pg 0
tap HOME
wheel pitch -8192
expect_out USB E0 00 00
wheel pitch 8191
expect_out USB E0 7F 7F
wheel pitch 0
expect_out USB E0 00 40
wheel mod 64
expect_out USB B0 01 40
press LFO
wait 150
release LFO
out
wait 150
expect_out USB B0 01 40
pedal down
expect_out USB B0 40 7F
press ENV
pedal up
expect sustain 1
release ENV
expect_out USB B0 40 00
expect sustain 0
out
pedal down
press GLO
key37 1 down 70
expect_out USB B0 40 00
expect_out USB B1 40 7F
expect_out USB B0 01 00
expect_out USB B1 01 40
expect chan 1
release GLO
expect view 0
key37 1 up
expect_no_out USB 91 2B 46
pedal up
expect_out USB B1 40 00
out
midi USB 90 40 60
expect trs_status 144
expect trs_data1 64
expect trs_data2 96
expect_no_out USB 90 40 60
out
tap SEQ
press GLO
key37 15 down 70
expect chan 15
release GLO
key37 15 up
expect view 3
expect notes_out 0
press GLO
tapkey37 1 70
release GLO
expect chan 1
expect view 3
tapkey37 0 70
expect notes_out 0
tapkey37 5 70
expect notes_out 0
tap HOME
out
key37 20 down 70
press GLO
key37 15 down 70
expect chan 15
expect notes_out 3
expect notes_on 0
release GLO
key37 15 up
key37 20 up
expect notes_on 0
press GLO
tapkey37 1 70
release GLO
expect chan 1
fixed
out
key37 36 down 20
expect_out USB 91 4F 64
key37 36 up
tap SAVE
reboot
expect bank 1
expect smk_cc0 29
expect smk_val0 66
# Older projects keep their settings and gain defaults for K5-8.
legacy_project
reboot
expect bank 1
expect smk_cc0 28
expect smk_val0 64
