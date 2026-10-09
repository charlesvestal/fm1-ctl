# the USB-only build (CT_BLE 0): Bluetooth never starts, its knob and port are hidden; USB and TRS work
wait 1500
expect ble 0
tapkey 0
expect_out USB 90 35 64
midi TRS 90 3E 50
expect_out USB 90 3E 50
tap EDIT
expect view 5
spin K4 1
expect ble 0
wait 50
shot setup.png
tap HOME
wait 50
shot play.png
reboot
wait 1500
expect ble 0
