# MIDI in: USB -> BLE, BLE -> USB, TRS -> USB and BLE; incoming notes light the keys
midi USB 90 3C 50
expect notes_out 0
midi TRS 90 3E 50
expect_out USB 90 3E 50
out
tap GLO
tap GLO
tap GLO
expect view 6
# Bluetooth started at boot
wait 1100
expect ble 2
ble connect
midi USB 91 40 60
expect_out BLE 91 40 60
midi BLE B0 07 64
expect_out USB B0 07 64
midi TRS 90 41 50
expect_out BLE 90 41 50
expect_out USB 90 41 50
tapkey 12
expect_out BLE 90 41 64
wait 50
shot ble.png
tap HOME
wait 50
shot thru.png
leds
