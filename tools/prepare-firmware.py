import sys, struct, hashlib, zlib

if len(sys.argv) != 3:
    print("Usage: prepare-firmware.py firmware.bin version")
    sys.exit(1)

fw = open(sys.argv[1], 'rb').read()
version = sys.argv[2].encode()

# metadata
crc = zlib.crc32(fw)
sha = hashlib.sha256(fw).hexdigest().encode()
size = len(fw)

# header format:
# [8 bytes version][4 bytes size][4 bytes CRC][32 bytes SHA256]
header = struct.pack(
    "<8sI4s32s",
    version.ljust(8, b'\0'),
    size,
    crc.to_bytes(4, 'little'),
    sha
)

with open("firmware_prepared.bin", "wb") as f:
    f.write(header)
    f.write(fw)

print("Firmware prepared with metadata header.")
