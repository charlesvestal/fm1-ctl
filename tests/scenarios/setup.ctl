# the Knobs page sets the bank's CC numbers; settings survive a restart, Bluetooth does not; glow
tap GLO
expect view 4
spin K1 -4
expect cc0 70
spin K4 13
expect cc3 20
wait 50
shot knobs.png
tap HOME
spin K4 1
expect_out USB B0 14 65
tap SAVE
expect dirty 0
spin ALGO 3
spin SELECT 10
wait 4500
expect dirty 0
leds
tap GLO
tap GLO
spin K4 -2
wait 50
shot setup.png
leds
tap GLO
spin K1 1
wait 1100
reboot
expect cc0 70
expect cc3 20
expect chan 3
expect tempo 130
expect ble 0
