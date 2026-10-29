# Light Hub Protocol v1

## Overview
Binary protocol for sending commands to the light hub over a byte stream
(UART now, BLE later). All multi-byte fields are **little-endian**.

## Packet layout

| Offset | Field   | Size | Description |
|--------|---------|------|-------------|
| 0      | magic   | 1    | Always `0xAA`. Marks the start of a packet. |
| 1      | version | 1    | Protocol version. Currently `0x01`. |
| 2      | type    | 1    | Message type (see table below). |
| 3      | seq     | 1    | Sequence number, chosen by the sender. Echoed in the response. |
| 4      | length  | 2    | Payload length in bytes (uint16, little-endian). Max 32. |
| 6      | payload | N    | Message-specific data, N = length. |
| 6+N    | crc     | 2    | CRC-16 (uint16, little-endian). |

Header = 6 bytes. Smallest packet = 8 bytes (no payload). Largest = 40 bytes.

## CRC
- Algorithm: CRC-16/CCITT-FALSE: polynomial 0x1021, initial value 0xFFFF, not reflected, no final XOR. Check value for “123456789” is 0x29B1. Implemented with Zephyr’s crc16_itu_t(0xFFFF, data, len)
- Covers: every byte from `version` through the end of `payload`
  (excludes `magic` and the CRC itself).

## Message types

| Type | Name        | Direction    | Payload | Response |
|------|-------------|--------------|---------|----------|
| 0x01 | SET_COLOR   | host → hub   | `bulb_id` (1), `r` (1), `g` (1), `b` (1) | RESPONSE |
| 0x02 | GET_STATE   | host → hub   | `bulb_id` (1) | STATE |
| 0x80 | RESPONSE    | hub → host   | `status` (1) | none |
| 0x81 | STATE       | hub → host   | `bulb_id` (1), `r` (1), `g` (1), `b` (1) | none |

Responses always reuse the request's `seq`. Hub-to-host types have the high bit set.

## Status codes (RESPONSE payload)

| Code | Name          | Meaning |
|------|---------------|---------|
| 0x00 | OK            | Command applied |
| 0x01 | BAD_VERSION   | Unsupported protocol version |
| 0x02 | UNKNOWN_TYPE  | Message type not recognized |
| 0x03 | BAD_LENGTH    | Length doesn't match what this type expects |
| 0x04 | INVALID_BULB  | Bulb ID out of range (valid: 0–7) |


## Validation rules (receiver)
Checked in this order:
1. Discard bytes until `magic` (0xAA) is seen.
2. If `length` > 32: drop the packet immediately, resync. No response.
3. Wait for the full packet (header + payload + CRC).
   If it doesn't arrive within 100 ms: drop it, resync.
4. If the CRC doesn't match: drop silently, resync. No response
   (the header may be corrupted, so `seq` can't be trusted).
5. If `version` ≠ 0x01: respond BAD_VERSION.
6. If `type` is unknown: respond UNKNOWN_TYPE.
7. If `length` doesn't match the type's payload size: respond BAD_LENGTH.
8. If `bulb_id` > 7: respond INVALID_BULB.
9. Otherwise: apply the command, respond OK (or STATE for GET_STATE).

## Worked example: set bulb 2 to red, seq 7

```
AA  01  01  07  04 00  02 FF 00 00  CC CC
|   |   |   |   |      |            |
|   |   |   |   |      |            CRC (computed in Lesson 6)
|   |   |   |   |      payload: bulb 2, R=255, G=0, B=0
|   |   |   |   length = 4 (little-endian: 0x0004)
|   |   |   seq = 7
|   |   type = SET_COLOR
|   version = 1
magic
```

Expected response:
```
A5  01  80  07  01 00  00  CC CC
                       |
                       status = OK
```