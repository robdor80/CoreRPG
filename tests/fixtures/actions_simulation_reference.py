"""Independent fixed-width RNG/v2 bytes for ActionDeterminismTest.

Run manually with Python 3; never imports or executes CoreRPG. The final world
is specified by the test's hand-audited trace: IDs 1..4, revision 12,
containment (2,3), relation (2,7,4), master seed 42 and two consumed streams.
"""

import struct

MASK = (1 << 64) - 1
INCREMENT = 0x9E3779B97F4A7C15


def next_value(state):
    state = (state + INCREMENT) & MASK
    value = ((state ^ (state >> 30)) * 0xBF58476D1CE4E5B9) & MASK
    value = ((value ^ (value >> 27)) * 0x94D049BB133111EB) & MASK
    return state, value ^ (value >> 31)


def seed(stream):
    mixed = 42 ^ (((stream + 0xD1B54A32D192ED03) * INCREMENT) & MASK)
    return next_value(mixed)[1]


def roll(state, minimum, maximum):
    width = maximum - minimum + 1
    threshold = (1 << 64) % width
    while True:
        state, value = next_value(state)
        if value >= threshold:
            return state, minimum + value % width


def crc32c(data):
    crc = MASK & 0xFFFFFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0x82F63B78 if crc & 1 else 0)
    return crc ^ 0xFFFFFFFF


if __name__ == "__main__":
    stream9, full = roll(seed(9), 0, MASK)
    stream9, rejection_range = roll(stream9, 0, (1 << 63) + 1)
    stream_max, die = roll(seed(MASK), 1, 6)
    data = b"CRPGSAVE" + struct.pack("<II9Q", 2, 1, 4, 12, 42, 1, 2, 0, 1, 0, 0)
    data += struct.pack("<6Q", 2, 3, 9, stream9, MASK, stream_max)
    data += struct.pack("<3Q", 2, 7, 4)
    data += struct.pack("<I", crc32c(data))
    print("roll values:", full, rejection_range, die)
    print("stream states:", hex(stream9), hex(stream_max))
    print("v2 bytes:", data.hex())
