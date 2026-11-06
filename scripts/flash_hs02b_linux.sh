#!/usr/bin/env bash
# FNIRSI HS-02B MSD flasher for Linux
# Copies HS02B_*_firmware.bin to the BOOTLOADER mass-storage device TWICE + sync.
# Reason: the FNIRSI MSD bootloader mishandles USB write caching; a single copy
# may leave the iron hanging in the bootloader with a half-written image.
set -euo pipefail

usage() {
    echo "Usage: $0 <HS02B_*_firmware.bin> [MOUNT_POINT]"
    echo ""
    echo "Example:"
    echo "  $0 source/Hexfile/HS02B_EN_firmware.bin"
    echo "  $0 source/Hexfile/HS02B_EN_firmware.bin /media/\$USER/BOOTLOADER"
    echo ""
    echo "Steps before running:"
    echo "  1. Hold OK and connect HS-02B via USB."
    echo "  2. Release OK on 'Please drag the firmware file to the USB flash disk!'."
    echo "  3. Wait for the BOOTLOADER drive with READY.TXT."
}

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    usage
    exit 1
fi

FW="$1"
if [ ! -f "$FW" ]; then
    echo "ERROR: '$FW' is not a regular file." >&2
    usage
    exit 1
fi

case "$FW" in
    *HS02B*firmware.bin) ;;
    *)
        echo "WARNING: '$FW' does not look like HS02B_*_firmware.bin." >&2
        echo "Only *_firmware.bin images (with the 16-byte bin header) flash via MSD." >&2
        read -r -p "Continue anyway? [y/N] " ans
        case "$ans" in
            [yY]*) ;;
            *) exit 1 ;;
        esac
        ;;
esac

if [ "$#" -eq 2 ]; then
    MNT="$2"
else
    # Auto-detect BOOTLOADER mount: prefer a mountpoint containing READY.TXT
    MNT=""
    for d in /media/"${USER:-$(id -nu)}"/BOOTLOADER /media/*/BOOTLOADER /mnt/BOOTLOADER; do
        if [ -f "$d/READY.TXT" ] || [ -d "$d" ]; then
            MNT="$d"
            break
        fi
    done
    if [ -z "$MNT" ]; then
        echo "ERROR: BOOTLOADER mount not found. Pass it explicitly:" >&2
        usage
        exit 1
    fi
fi

if [ ! -d "$MNT" ]; then
    echo "ERROR: mount point '$MNT' is not a directory." >&2
    exit 1
fi

echo "Flashing '$FW' -> '$MNT' (copy x2 + sync)..."
cp "$FW" "$MNT/"
sync
cp "$FW" "$MNT/"
sync
echo "Done. Wait for 'Updating firmware!' on the iron, then it reboots."
echo "If the 40s bootloader timeout expired, re-enter bootloader mode and retry."
