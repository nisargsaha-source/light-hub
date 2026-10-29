#pragma once

#include "bulb_cmd.h"

/*
 * Parse "<bulb> <r> <g> <b>" (each 0-255) into *out.
 * Returns 0 on success, -EINVAL otherwise. *out is only written on success.
 */
int parse_bulb_cmd(const char *line, struct bulb_cmd *out);
