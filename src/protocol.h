#pragma once

#include <stddef.h>
#include <stdint.h>
#include "packet.h"

/* Handle a complete, CRC-valid packet.
 * Writes a response packet into resp (PKT_MAX_LEN bytes).
 * Returns the response length, or 0 for no response. */
size_t proto_handle(const struct pkt_rx *rx, uint8_t *resp);