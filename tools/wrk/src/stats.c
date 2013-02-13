// Copyright (C) 2012 - Will Glozer.  All rights reserved.

#include <inttypes.h>
#include <stdlib.h>
#include <math.h>

#include "stats.h"
#include "zmalloc.h"

stats *stats_alloc(uint64_t samples) {
    stats *stats = zcalloc(sizeof(stats) + sizeof(uint64_t) * samples);
    stats->samples = samples;
    return stats;
}

void stats_free(stats *stats) {
    zfree(stats);
}

void stats_record(stats *stats, uint64_t x) {
    stats->data[stats->index++] = x;
    if (stats->limit < stats->samples)  stats->limit++;
    if (stats->index == stats->samples) stats->index = 0;
}

uint64_t stats_min(stats *stats) {
    uint64_t min = 0;
    for (uint64_t i = 0; i < stats->limit; i++) {
        uint64_t x = stats->data[i];
        if (x < min || min == 0) min = x;
    }
    return min;
}

uint64_t stats_max(stats *stats) {
    uint64_t max = 0;
    for (uint64_t i = 0; i < stats->limit; i++) {
        uint64_t x = stats->data[i];
        if (x > max || max == 0) max = x;
    }
    return max;
}

long double stats_mean(stats *stats) {
    uint64_t sum = 0;
    if (stats->limit == 0) return 0.0;
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
