# The manual's screenshots (tools/gen_screens.sh: the USB-only build, as released, into docs/img).
wait 1500
# Play: a controller just sent, a chord held
spin K1 -12
spin K2 6
wait 100
key 4 down
key 8 down
key 11 down
wait 60
shot screen-play.png
key 4 up
key 8 up
key 11 up
# FX held: the bank's controller numbers
press FX
spin K3 1
wait 500
shot screen-cc.png
release FX
wait 1200
# Keys: D Dorian
tap SEL
spin K2 3
spin K1 2
wait 1300
key 7 down
wait 60
shot screen-keys.png
key 7 up
# Arp: on, a chord held
tap ARP
spin K1 1
wait 50
key 0 down
key 4 down
key 7 down
wait 1030
shot screen-arp.png
key 0 up
key 4 up
key 7 up
spin K1 -1
# Sequencer: playing, track 2 selected
tap SEQ
key 3 down
key 3 up
tap PLAY
wait 1590
shot screen-seq.png
tap PLAY
# Sound
tap GLO
key 2 down
wait 1300
shot screen-sound.png
key 2 up
# Setup, KNOB 1 just turned
tap EDIT
spin K1 -8
wait 300
shot screen-setup.png
# ENV tapped twice: sustain locked
tap HOME
wait 1300
tap ENV
tap ENV
wait 200
shot screen-sus.png
