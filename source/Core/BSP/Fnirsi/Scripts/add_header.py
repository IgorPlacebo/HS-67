#!/usr/bin/env python3
import sys
import struct

# Usage: python3 add_header.py <format_tag> <input_bin_file>
# Generates input_bin_file_firmware.bin with the exact 16-byte FNIRSI header (BIN\x00 + uint32_le len)

with open(sys.argv[2], 'rb') as input_file, open(sys.argv[2][:-4]+'_firmware.bin', 'wb') as output_file:
    data = input_file.read()

    header = bytearray(0x10)
    # FNIRSI bootloader requires UPPERCASE 'BIN\x00' (0x42 0x49 0x4E 0x00)
    tag = sys.argv[1].upper().encode('ASCII')[:3]
    header[0:8] = struct.pack('<3sBI', tag, 0x00, len(data))

    output_file.write(header)
    output_file.write(data)
