# the arpeggiator: ARP shows its page, KNOB 1 turns it on; at 120, 1/16 = 125 ms a note; up over a held chord;
# KNOB 1 to Latch
tap ARP
expect view 2
expect arp 0
spin K1 1
expect arp 1
out
key 0 down
key 4 down
key 7 down
wait 1000
expect notes_out >6
expect_out USB 90 35 64
expect_out USB 90 39 64
expect_out USB 90 3C 64
key 0 up
key 4 up
key 7 up
out
wait 500
expect notes_out 0
spin K1 1
key 2 down
wait 300
key 2 up
out
wait 600
# latched: it keeps playing after the keys are let go
expect notes_out >3
expect_out USB 90 37 64
wait 50
shot arp.png
spin K1 -2
expect arp 0
out
wait 300
expect notes_out 0
