# FX held on Play: KNOB 1-4 set the bank's CC numbers (no bank change); settings survive a restart,
# Bluetooth does not; glow
press FX
spin K1 -4
expect cc0 70
spin K4 13
expect cc3 20
wait 50
shot knobs.png
release FX
expect bank 0
spin K4 1
expect_out USB B0 14 65
tap SAVE
expect dirty 0
spin ALGO 3
# the tempo: Setup, KNOB 1; SELECT: the banks on Play (stops at A and D), nothing on Setup
tap GLO
spin K1 10
spin SELECT 1
wait 50
expect view 5
tap HOME
expect view 0
spin SELECT 1
wait 50
expect bank 1
spin SELECT -1
wait 50
spin SELECT -1
wait 50
expect bank 0
wait 4500
expect dirty 0
leds
tap GLO
expect view 5
spin K4 -2
wait 50
shot setup.png
leds
reboot
expect cc0 70
expect cc3 20
expect chan 3
expect tempo 130
# Bluetooth starts again at boot
expect ble 1
