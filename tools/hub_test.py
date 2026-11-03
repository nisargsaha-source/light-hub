#!/usr/bin/env python3
"""
hub_test.py - Send protocol packets to the light hub and check the responses.
 
Usage:
    python3 tools/hub_test.py /dev/pts/N
 
Implements the PC side of PROTOCOL.md:
  - header: magic, version, type, seq, length (uint16 LE)
  - CRC-16/CCITT-FALSE (binascii.crc_hqx with init 0xFFFF), little-endian,
    covering every byte from magic through the end of the payload.
Anything the hub sends that isn't a packet is its log output, which is
printed with a "[hub]" prefix.
"""
import binascii
import struct
import sys
import time
 
import serial
 
# ---- Protocol constants (must match packet.h / PROTOCOL.md) ----
MAGIC = 0xAA
VERSION = 0x01
HEADER_LEN = 6
CRC_LEN = 2
MAX_PAYLOAD = 32
 
TYPE_SET_COLOR = 0x01
TYPE_GET_STATE = 0x02
TYPE_RESPONSE = 0x80
TYPE_STATE = 0x81
 
STATUS_OK = 0x00
STATUS_BAD_VERSION = 0x01
STATUS_UNKNOWN_TYPE = 0x02
STATUS_BAD_LENGTH = 0x03
STATUS_INVALID_BULB = 0x04
STATUS_BUSY = 0x05
 
STATUS_NAMES = {
    STATUS_OK: "OK",
    STATUS_BAD_VERSION: "BAD_VERSION",
    STATUS_UNKNOWN_TYPE: "UNKNOWN_TYPE",
    STATUS_BAD_LENGTH: "BAD_LENGTH",
    STATUS_INVALID_BULB: "INVALID_BULB",
    STATUS_BUSY: "BUSY",
}
 
RESPONSE_TIMEOUT = 1.0   # seconds to wait for an expected response
SILENCE_TIMEOUT = 0.5    # seconds of silence that count as "no response"
 
 
def crc16(data):
    """CRC-16/CCITT-FALSE, same as Zephyr's crc16_itu_t(0xFFFF, ...)."""
    return binascii.crc_hqx(bytes(data), 0xFFFF)
 
 
def build_packet(ptype, seq, payload=b"", version=VERSION, length=None):
    """Build a complete packet. `version` and `length` can be overridden
    to create deliberately invalid packets for testing."""
    if length is None:
        length = len(payload)
 
    # struct.pack turns numbers into raw bytes, laid out by the format string.
    # It's the Python version of sys_put_le16 / filling out[] in pkt_build.
    #   <  = little-endian, no padding (always put this first for protocols)
    #   B  = unsigned 8-bit (1 byte)   -> magic, version, type, seq
    #   H  = unsigned 16-bit (2 bytes) -> length
    # Example: (0xAA, 1, 1, 7, 4) -> AA 01 01 07 04 00   (6-byte header)
    # bytes(payload) turns a list like [2, 255, 0, 0] into raw bytes,
    # and + joins byte sequences together (like memcpy onto the end).
    body = struct.pack("<BBBBH", MAGIC, version, ptype, seq, length) + bytes(payload)
 
    # CRC over the whole body (magic..payload), packed as 2 little-endian
    # bytes and appended to the end, just like sys_put_le16(crc, ...) in C.
    return body + struct.pack("<H", crc16(body))
 
 
class PacketReader:
    """Pulls packets out of the hub's TX stream, which mixes binary
    packets with plain-text log lines."""
 
    def __init__(self, ser):
        # self is the object itself, like the rx pointer in your C functions;
        # self.x is a field stored in the object, like rx->buf.
        self.ser = ser
        # Empty, growable byte buffer that collects one line of hub log text.
        # It lives on self (not as a local) so a line split across several
        # read_packet calls isn't lost, like line[] declared outside the loop
        # in your Lesson 3 rx_thread.
        self.log_line = bytearray()
 
    def _log_byte(self, b):
        # Collect non-packet bytes into text lines and print each full line.
        # b is a number (0-255), "\n" is text; ord("\n") converts the
        # character to its number (10) so they can be compared.
        # In C you didn't need this, because '\n' already IS the number 10.
        if b == ord("\n"):
            text = self.log_line.decode("utf-8", errors="replace").rstrip("\r")
            if text:
                print(f"    [hub] {text}")
            self.log_line.clear()
        else:
            self.log_line.append(b)
 
    def read_packet(self, timeout):
        """Return the next valid packet as a dict, or None on timeout."""
        # time.monotonic() = seconds on a counter that only ever moves forward.
        # The value itself is meaningless; only differences matter. Unlike the
        # wall clock (time.time()), it doesn't jump if Windows resyncs the
        # clock, so a 1-second timeout always means 1 second.
        # Same idea as k_uptime_get() in Zephyr.
        deadline = time.monotonic() + timeout
        buf = bytearray()
        length = 0
 
        while time.monotonic() < deadline:
            chunk = self.ser.read(1)
            if not chunk:
                continue
            b = chunk[0]
 
            # Waiting for the start of a packet: everything else is log text
            if not buf:
                if b == MAGIC:
                    buf.append(b)
                else:
                    self._log_byte(b)
                continue
 
            buf.append(b)
 
            # Header complete: read and sanity-check the length.
            # struct.unpack is the reverse of pack: bytes -> numbers, like
            # sys_get_le16 in C. buf[4:6] is a slice: bytes 4 and 5 (the
            # length field). unpack always returns a tuple, even for one
            # value, e.g. (4,). Writing (length,) = ... pulls the single
            # value out of the tuple into length.
            if len(buf) == HEADER_LEN:
                (length,) = struct.unpack("<H", buf[4:6])
                if length > MAX_PAYLOAD:
                    print(f"    [pc] dropped packet with bad length {length}")
                    buf.clear()
                    continue
 
            # Full packet: check the CRC
            if len(buf) >= HEADER_LEN and len(buf) == HEADER_LEN + length + CRC_LEN:
                # Slicing: seq[start:end] takes start up to (not including) end;
                # a blank side means "from the start" / "to the end", and
                # negative numbers count from the end. With CRC_LEN = 2:
                #   buf[:-2] = everything except the last 2 bytes (the data)
                #   buf[-2:] = only the last 2 bytes (the CRC)
                # bytes(...) makes an unchangeable copy for crc16().
                body = bytes(buf[:-CRC_LEN])
                # unpack returns a 1-item tuple like (4660,); (received_crc,)
                # unpacks it so received_crc is the number, not the tuple.
                (received_crc,) = struct.unpack("<H", buf[-CRC_LEN:])
                if crc16(body) == received_crc:
                    return {
                        "version": buf[1],
                        "type": buf[2],
                        "seq": buf[3],
                        "payload": bytes(buf[HEADER_LEN:HEADER_LEN + length]),
                        "raw": bytes(buf),
                    }
                print(f"    [pc] dropped packet with bad CRC: {buf.hex(' ')}")
                buf.clear()
 
        return None
 
    def drain(self, duration=0.1):
        """Read and print anything pending (logs, stray packets)."""
        while True:
            pkt = self.read_packet(duration)
            if pkt is None:
                return
            print(f"    [pc] unexpected packet: {pkt['raw'].hex(' ')}")

#helper            
def send(ser, data):
    # data may arrive as bytes (unchangeable), bytearray (changeable, used by
    # the bad-CRC test to flip a byte), or a list of ints. bytes(data) turns
    # any of them into plain bytes. C analogy: bytes ~ const uint8_t[],
    # bytearray ~ uint8_t[]. .hex(' ') prints them as "aa 01 80 ...". ser is the serial port
    print(f"    sent: {bytes(data).hex(' ')}")
    ser.write(bytes(data))
    
def expect_status(reader, ser, pkt, seq, status):
    send(ser, pkt)
    resp = reader.read_packet(RESPONSE_TIMEOUT)
    if resp is None:
        return False, "no response"
    print(f"    got:  {resp['raw'].hex(' ')}")
    if resp["type"] != TYPE_RESPONSE:
        return False, f"expected RESPONSE, got type 0x{resp['type']:02x}"
    if resp["seq"] != seq:
        return False, f"expected seq {seq}, got {resp['seq']}"
    if resp["payload"] != bytes([status]):
        got = resp["payload"][0] if resp["payload"] else None
        return False, f"expected {STATUS_NAMES[status]}, got {STATUS_NAMES.get(got, got)}"
    return True, STATUS_NAMES[status]
 
 
def expect_silence(reader, ser, pkt):
    send(ser, pkt)
    resp = reader.read_packet(SILENCE_TIMEOUT)
    if resp is not None:
        return False, f"expected no response, got {resp['raw'].hex(' ')}"
    return True, "no response, as expected"
 
 
# ---- Test cases ----
 
def test_set_color(reader, ser):
    pkt = build_packet(TYPE_SET_COLOR, 1, [2, 255, 0, 0])
    return expect_status(reader, ser, pkt, 1, STATUS_OK)
 
 
def test_get_state(reader, ser):
    # SET_COLOR is queued, not applied instantly; give bulb_mgr a moment
    time.sleep(0.05)
    send(ser, build_packet(TYPE_GET_STATE, 2, [2]))
    resp = reader.read_packet(RESPONSE_TIMEOUT)
    if resp is None:
        return False, "no response"
    print(f"    got:  {resp['raw'].hex(' ')}")
    if resp["type"] != TYPE_STATE or resp["seq"] != 2:
        return False, f"expected STATE seq 2, got type 0x{resp['type']:02x} seq {resp['seq']}"
    if resp["payload"] != bytes([2, 255, 0, 0]):
        return False, f"expected bulb 2 = 255,0,0, got {list(resp['payload'])}"
    return True, "bulb 2 is R=255 G=0 B=0"
 
 
def test_invalid_bulb(reader, ser):
    pkt = build_packet(TYPE_SET_COLOR, 3, [9, 0, 255, 0])
    return expect_status(reader, ser, pkt, 3, STATUS_INVALID_BULB)
 
 
def test_bad_length(reader, ser):
    pkt = build_packet(TYPE_SET_COLOR, 4, [2, 255, 0])   # 3 bytes, needs 4
    return expect_status(reader, ser, pkt, 4, STATUS_BAD_LENGTH)
 
 
def test_unknown_type(reader, ser):
    pkt = build_packet(0x33, 5)
    return expect_status(reader, ser, pkt, 5, STATUS_UNKNOWN_TYPE)
 
 
def test_bad_version(reader, ser):
    pkt = build_packet(TYPE_SET_COLOR, 6, [2, 0, 0, 255], version=0x02)
    return expect_status(reader, ser, pkt, 6, STATUS_BAD_VERSION)
 
 
def test_bad_crc(reader, ser):
    pkt = bytearray(build_packet(TYPE_SET_COLOR, 7, [2, 0, 255, 0]))
    pkt[-1] ^= 0xFF   # corrupt one CRC byte
    return expect_silence(reader, ser, pkt)
 
 
def test_oversized_length(reader, ser):
    # Header claims a 500-byte payload; the hub must drop it right after the header
    pkt = build_packet(TYPE_SET_COLOR, 8, b"", length=500)
    return expect_silence(reader, ser, pkt)
 
 
def test_garbage_then_valid(reader, ser):
    # Garbage deliberately contains no 0xAA (see think-question 2)
    garbage = b"\x00\x13\x37hello\xff\x55"
    pkt = build_packet(TYPE_SET_COLOR, 9, [1, 0, 0, 255])
    return expect_status(reader, ser, garbage + pkt, 9, STATUS_OK)
 
 
def test_truncated_then_valid(reader, ser):
    partial = build_packet(TYPE_SET_COLOR, 10, [1, 255, 255, 0])[:5]
    send(ser, partial)
    time.sleep(0.2)   # longer than the hub's 100 ms timeout
    pkt = build_packet(TYPE_SET_COLOR, 11, [1, 255, 255, 0])
    return expect_status(reader, ser, pkt, 11, STATUS_OK)
 
 
TESTS = [
    ("Set color", test_set_color),
    ("Read it back", test_get_state),
    ("Invalid bulb", test_invalid_bulb),
    ("Bad length", test_bad_length),
    ("Unknown type", test_unknown_type),
    ("Bad version", test_bad_version),
    ("Bad CRC", test_bad_crc),
    ("Oversized length", test_oversized_length),
    ("Garbage first", test_garbage_then_valid),
    ("Truncated packet", test_truncated_then_valid),
]
 
 
def main():
    if len(sys.argv) != 2:
        print("usage: python3 tools/hub_test.py /dev/pts/N")
        return 2
 
    # Sanity check: our CRC matches the CRC-16/CCITT-FALSE check value
    assert crc16(b"123456789") == 0x29B1, "CRC variant mismatch"
 
    ser = serial.Serial(sys.argv[1], timeout=0.05)
    reader = PacketReader(ser)
 
    print("Waiting for the hub...")
    reader.drain(0.5)   # show startup logs, clear old data
 
    passed = 0
    for name, test in TESTS:
        print(f"\n[{name}]")
        reader.drain()   # print any pending logs before the test
        ok, detail = test(reader, ser)
        print(f"  {'PASS' if ok else 'FAIL'}: {detail}")
        passed += ok
 
    reader.drain(0.3)
    print(f"\n{passed}/{len(TESTS)} tests passed")
    ser.close()
    return 0 if passed == len(TESTS) else 1
 
 
if __name__ == "__main__":
    sys.exit(main())

