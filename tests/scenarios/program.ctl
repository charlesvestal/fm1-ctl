# the onboard synth plays the program the screen shows: after a restart, on a new channel, with Sound off
wait 300
expect synth_program 0
spin PRESET 4
expect program 4
wait 50
expect synth_program 4
tap SAVE
reboot
wait 300
expect program 4
expect synth_program 4
# another channel: the synth's channel too
spin ALGO 1
wait 50
expect chan 1
expect synth_program 4
# Sound off (Sound page, KNOB 1): a program change still reaches it
tap EDIT
expect view 4
spin K1 -1
wait 50
expect sound 2
spin PRESET 1
expect program 5
wait 100
expect synth_program 5
spin K1 1
wait 100
expect sound 0
expect synth_program 5
