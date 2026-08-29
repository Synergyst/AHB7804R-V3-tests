#!/bin/bash

ARCH=arm
CROSS_COMPILE=arm-linux-gnueabi-
NFS_DEST="/media/nfsroot/AHB7804R-V3-FW-dump-with-mods/modules-custom/modules"
JOBS=$(nproc)

# Export via environment to avoid stripping Makefile internal architecture flags (-march, -D__LINUX_ARM_ARCH__)
export KCFLAGS="-w -fgnu89-inline -fno-ipa-sra -Wno-attribute-alias -Wno-error=attributes -D__LINUX_ARM_ARCH__=7"
export KAFLAGS="-w -Wa,-mimplicit-it=always"

echo "[+] Cleaning build tree..."
make -s ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE mrproper > /dev/null 2>&1 || true

echo "[+] Setting Makefile SUBLEVEL to 8..."
sed -i 's/^SUBLEVEL = .*/SUBLEVEL = 8/' Makefile

echo "[+] Loading ARMv7 base configuration (vexpress_defconfig)..."
make -s ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE HOSTCFLAGS="-w" vexpress_defconfig > /dev/null 2>&1

echo "[+] Enabling common USB serial drivers..."
{
    echo "CONFIG_USB_SUPPORT=y"
    echo "CONFIG_USB=y"
    echo "CONFIG_USB_SERIAL=m"
    echo "CONFIG_USB_SERIAL_CP210X=m"
    echo "CONFIG_USB_SERIAL_FTDI_SIO=m"
    echo "CONFIG_USB_SERIAL_PL2303=m"
    echo "CONFIG_USB_SERIAL_CH341=m"
} >> .config

echo "[+] Disabling legacy/incompatible subsystems..."
sed -i 's/CONFIG_FAT_FS=.*/# CONFIG_FAT_FS is not set/' .config
sed -i 's/CONFIG_MSDOS_FS=.*/# CONFIG_MSDOS_FS is not set/' .config
sed -i 's/CONFIG_VFAT_FS=.*/# CONFIG_VFAT_FS is not set/' .config
sed -i 's/CONFIG_VT=.*/# CONFIG_VT is not set/' .config
sed -i 's/CONFIG_DRM=.*/# CONFIG_DRM is not set/' .config
sed -i 's/CONFIG_SOUND=.*/# CONFIG_SOUND is not set/' .config
sed -i 's/CONFIG_WIRELESS=.*/# CONFIG_WIRELESS is not set/' .config
sed -i 's/CONFIG_WLAN=.*/# CONFIG_WLAN is not set/' .config
sed -i 's/CONFIG_STAGING=.*/# CONFIG_STAGING is not set/' .config

echo "[+] Applying Hi3520D target kernel ABI fixes (no SMP, no SysRq, no Unwind)..."
sed -i 's/CONFIG_SMP=y/# CONFIG_SMP is not set/' .config
sed -i 's/CONFIG_MAGIC_SYSRQ=y/# CONFIG_MAGIC_SYSRQ is not set/' .config
sed -i 's/CONFIG_ARM_UNWIND=y/# CONFIG_ARM_UNWIND is not set/' .config

echo "[+] Syncing configuration non-interactively..."
yes "" | make -s ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE HOSTCFLAGS="-w" oldconfig > /dev/null 2>&1

echo "[+] Compiling kernel modules..."
make -s ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE -j"$JOBS" -k HOSTCFLAGS="-w" modules 2>&1 | grep -v "WARNING: Appending" | grep -v "selects ARM_ERRATA" || true

echo -e "\n[+] Build sequence completed."

if [ -f "drivers/usb/serial/cp210x.ko" ]; then
    echo "[+] CP210x Module built successfully. Vermagic:"
    modinfo drivers/usb/serial/cp210x.ko | grep vermagic
fi

echo "[+] Modules found:"
for m in $(find drivers/ -name "*.ko") ; do
  echo "[+] $m:"
  modinfo "$m" | grep -v 'alias:'
done

if [ -d "$NFS_DEST" ]; then
    echo "[+] Copying all compiled .ko modules to NFS root..."
    COUNT=$(find . -type f -name "*.ko" -exec cp {} "$NFS_DEST/" \; -print | wc -l)
    echo "[+] Done! $COUNT modules successfully copied to $NFS_DEST"
fi
