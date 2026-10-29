#include <errno.h>
#include <stdlib.h>

#include "parser.h"

int parse_bulb_cmd(const char *line, struct bulb_cmd *out)
{
    unsigned long vals[4];
    const char *p = line;
    char *end;

    for (int i = 0; i < 4; i++) {
        vals[i] = strtoul(p, &end, 10);
        if (end == p) {
            return -EINVAL; /* no number found */
        }
        if (vals[i] > 255) {
            return -EINVAL; /* won't fit in uint8_t */
        }
        p = end; /* continue after this number */
    }

    /* Only allow trailing spaces */
    while (*p == ' ') {
        p++;
    }
    if (*p != '\0') {
        return -EINVAL;
    }

    out->bulb_id = (uint8_t)vals[0];
    out->r = (uint8_t)vals[1];
    out->g = (uint8_t)vals[2];
    out->b = (uint8_t)vals[3];
    return 0;
}
