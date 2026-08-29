#!/bin/bash

# mount -t nfs -o nolock -o tcp 192.168.168.175:/media/nfsroot /mnt/nfsroot

{
  sleep 2; echo "root"
  sleep 1; echo "xc3511"
  sleep 1; echo "mount -t nfs -o nolock -o tcp 192.168.168.175:/media/nfsroot /home"
  sleep 1; echo "cd /home/AHB7804R-V3-FW-dump-with-mods"

  sleep 1; echo "/home/AHB7804R-V3-FW-dump-with-mods/frame-manip/sofia_wdt_supervisor --alive-ms 150 --dead-ms 25"
  sleep 1; echo "while true ; do sleep 1 ; /home/AHB7804R-V3-FW-dump-with-mods/frame-manip/frame_stream_tcp --start-sofia --reconnect --ip 192.168.168.37 --port 5000 --pool 5 --tl --down 2 --fps 0 --no-stats --wait-vb-ms 120000 --wait-step-ms 500 ; done"
} | netcat 192.168.168.10 23

# 192.168.168.10 is NVR (AHB7804R-V3)
# 192.168.168.175 is NFS server (RPi CM4)
# 192.168.168.37 is playback system (ie: an RPi CM4 or other system)
# /home/AHB7804R-V3-FW-dump-with-mods is the on-AHB7804R-V3 folder containing test files/binaries/build environment shared between all systems (many early memory manipulation files/test programs/source code not on the Github)
# /media/nfsroot should be mounted at /media/nfsroot on all systems except for the AHB7804R-V3
# sofia_wdt_supervisor is to reduce CPU and I/O utilization. It tickles the WDT ocassionally effectively. It keeps the WDT happy while not stopping the Sofia process entirely.
