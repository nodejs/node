// Copyright (C) 2012 - Will Glozer.  All rights reserved.

#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <inttypes.h>

#include "units.h"
#include "aprintf.h"

typedef struct {
    int scale;
    char *base;
    char *units[];
} units;

units time_units_us = {
    .scale = 1000,
    .base  = "us",
    .units = { "ms", "s", NULL }
};

units time_units_s = {
    .scale = 60,
    .base  = "s",
    .units = { "m", "h", NULL }
};

units binary_units = {
    .scale = 1024,
    .base  = "",
    .units = { "K", "M", "G", "T", "P", NULL }
};

units metric_units = {
    .scale = 1000,
    .base  = "",
    .units = { "k", "M", "G", "T", "P", NULL }
};

static char *format_units(long double n, units *m, int p) {
    long double amt = n, scale;
    char *unit = m->base;
    char *msg = NULL;

    scale = m->scale * 0.85;

    for (int i = 0; m->units[i+1] && amt >= scale; i++) {
        amt /= m->scale;
        unit = m->units[i];
    }

    aprintf(&msg, "%.*Lf%s", p, amt, unit);

    return msg;
}

static int scan_units(char *s, uint64_t *n, units *m) {
    uint64_t base, scale = 1;
    char unit[3] = { 0, 0, 0 };
    int i, c;

    if ((c = sscanf(s, "%"SCNu64"%2s", &base, unit)) < 1) return -1;

    if (c == 2) {
        for (i = 0; m->units[i] != NULL; i++) {
            scale *= m->scale;
            if (!strncasecmp(unit, m->units[i], sizeof(unit))) break;
        }
        if (m->units[i] == NULL) return -1;
    }

    *n = base * scale;
    return 0;
}

char *format_binary(long double n) {
    return format_units(n, &binary_units, 2);
}

char *format_metric(long double n) {
    return format_units(n, &metric_units, 2);
}

char *format_time_us(long double n) {
    units *units = &time_units_us;
    if (n >= 1000000.0) {
        n /= 1000000.0;
        units = &time_units_s;
    }
    return format_units(n, units, 2);
}

int scan_metric(char *s, uint64_t *n) {
    return scan_units(s, n, &metric_units);
}
