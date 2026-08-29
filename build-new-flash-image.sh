#!/bin/bash

cd /media/FALCON/binextracts/extractions/W25Q64FV@SOIC8_butreallySOIC16_AHB7804R-LM-V3.BIN.extracted/30000 && cp ../../../W25Q64FV@SOIC8_butreallySOIC16_AHB7804R-LM-V3.BIN new-fw-full.bin && echo "Before modification:" && binwalk new-fw-full.bin && rm newrootfs.bin ; mksquashfs squashfs-root/ newrootfs.bin -comp xz -b 262144 && du -b newrootfs.bin && du -b ../../W25Q64FV@SOIC8_butreallySOIC16_AHB7804R-LM-V3.BIN_196608_squashfs.raw && dd if=newrootfs.bin of=new-fw-full.bin bs=1 seek=196608 count=$(du -b new-fw-full.bin | awk '{ print $1 }') conv=notrunc && echo "After modification:" && binwalk new-fw-full.bin
echo
echo 'IMPORTANT: Check if start-section in modified firmware after 0x30000 is still 0x220000 before flashing!!!'
