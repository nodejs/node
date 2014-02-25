// Copyright (C) 2012 - Will Glozer.  All rights reserved.

#include <inttypes.h>
#include <stdlib.h>
#include <math.h>

#include "stats.h"
#include "zmalloc.h"

stats *stats_alloc(uint64_t samples) {
    stats *s = zcalloc(sizeof(stats) + sizeof(uint64_t) * samples);
    s->samples = samples;
    s->min     = UINT64_MAX;
    return s;
}

void stats_free(stats *stats) {
    zfree(stats);
}

void stats_reset(stats *stats) {
    stats->limit = 0;
    stats->index = 0;
    stats->min   = UINT64_MAX;
    stats->max   = 0;
}

void stats_rewind(stats *stats) {
    stats->limit = 0;
    stats->index = 0;
}

void stats_record(stats *stats, uint64_t x) {
    stats->data[stats->index++] = x;
    if (x < stats->min) stats->min = x;
    if (x > stats->max) stats->max = x;
    if (stats->limit < stats->samples)  stats->limit++;
    if (stats->index == stats->samples) stats->index = 0;
}

static int stats_compare(const void *a, const void *b) {
    uint64_t *x = (uint64_t *) a;
    uint64_t *y = (uint64_t *) b;
    return *x - *y;
}

long double stats_summarize(stats *stats) {
    qsort(stats->data, stats->limit, sizeof(uint64_t), &stats_compare);
    return stats_mean(stats);
}

long double stats_mean(stats *stats) {
    if (stats->limit == 0) return 0.0;

    uint64_t sum = 0;
    for (uint64_t i = 0; i < stats->limit; i++) {
        sum += stats->data[i];
    }
    return sum / (long double) stats->limit;
}

long double stats_stdev(stats *stats, long double mean) {
    long double sum = 0.0;
    if (stats->limit < 2) return 0.0;
    for (uint64_t i = 0; i < stats->limit; i++) {
        sum += powl(stats->data[i] - mean, 2);
    }
    return sqrtl(sum / (stats->limit - 1));
}

long double stats_within_stdev(stats *stats, long double mean, long double stdev, uint64_t n) {
    long double upper = mean + (stdev * n);
    long double lower = mean - (stdev * n);
    uint64_t sum = 0;

    for (uint64_t i = 0; i < stats->limit; i++) {
        uint64_t x = stats->data[i];
        if (x >= lower && x <= upper) sum++;
    }

    return (sum / (long double) stats->limit) * 100;
}

uint64_t stats_percentile(stats *stats, long double p) {
    uint64_t rank = round((p / 100.0) * stats->limit + 0.5);
    return stats->data[rank - 1];
}

void stats_sample(stats *dst, tinymt64_t *state, uint64_t count, stats *src) {
    for (uint64_t i = 0; i < count; i++) {
        uint64_t n = rand64(state, src->limit);
        stats_record(dst, src->data[n]);
    }
}

uint64_t rand64(tinymt64_t *state, uint64_t n) {
    uint64_t x, max = ~UINT64_C(0);
    max -= max % n;
    do {
        x = tinymt64_generate_uint64(state);
    } while (x >= max);
    return x % n;
}
