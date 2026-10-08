# the Setup page sets the knobs' CC numbers; settings survive a restart, Bluetooth does not
tap GLO
expect view 1
spin K1 -4
expect cc0 70
spin K4 13
expect cc3 20
wait 50
shot setup.png
tap HOME
spin K4 1
expect_out USB B0 14 65
tap SAVE
expect dirty 0
spin ALGO 3
wait 4500
expect dirty 0
tap SEL
spin K1 1
wait 1100
reboot
expect cc0 70
expect cc3 20
expect chan 3
expect ble 0
