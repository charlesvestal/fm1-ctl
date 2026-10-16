# Setup's Bluetooth page: a Bluetooth MIDI device found and connected (the simulator's stand-in, "BLE MIDI"), its MIDI
# in and passed on to USB; let go (the page looks again); Bluetooth switched off, and off again after a restart
# (saved by itself, 4 s after the last change); on again
wait 1500
tap GLO
expect view 5
spin SELECT 1
spin SELECT 1
wait 700
expect ble_dev 1
shot bt-found.png
tap SAVE
wait 400
expect ble_dev 4
shot bt-connected.png
midi BLE B0 01 40
wait 20
expect_out USB B0 01 40
out
tap SAVE
wait 50
expect ble_dev 1
spin K4 -1
wait 50
expect ble 0
expect ble_off 1
shot bt-off.png
wait 4500
reboot
wait 1500
expect ble 0
tap GLO
spin SELECT 1
spin SELECT 1
spin K4 1
wait 1200
expect ble 2
expect ble_off 0
