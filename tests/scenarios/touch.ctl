# Touch (SELECT on Setup): velocity and aftertouch from the keys, on by default (Poly); the keys land at the
# velocity the script gives them (host/ctl_host.c reads them as the FM-1's sensing would)
tap GLO
expect view 5
turn SELECT 1
expect setup_pg 1
expect touch 1
spin K1 -1
spin K3 -1
expect touch 0
shot touch-off.png
# off: every note at the Keys page's velocity, whatever the key
out
key 0 down 30
wait 20
expect_out USB 90 35 64
key 0 up
wait 20
# Velocity: Keys
spin K1 1
expect touch 1
out
key 0 down 30
wait 30
expect notes_out 1
expect last_vel >24
expect last_vel <36
key 0 up
wait 30
out
key 2 down 120
wait 30
expect last_vel >114
shot touch-on.png
key 2 up
wait 30
# a top key (no converter): its velocity from the probe, at the contact
out
key 22 down 60
wait 10
expect notes_out 1
expect last_vel >50
expect last_vel <70
key 22 up
wait 30
# Aftertouch: Poly, then Channel
spin K3 1
out
key 4 down 80 60
wait 800
expect at >40
shot touch-pressure.png
key 4 up
wait 30
spin K3 1
out
key 4 down 80 80
wait 800
expect at >40
key 4 up
wait 100
expect at 0
# the Sequencer page: the keys edit at the contact, as always
tap SEQ
out
tapkey 0
expect notes_out 0
# kept
tap SAVE
reboot
expect touch 1
tap GLO
turn SELECT 1
spin K1 -1
spin K3 -2
expect touch 0
