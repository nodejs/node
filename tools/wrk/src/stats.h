#ifndef STATS_H
#define STATS_H

#include <stdbool.h>
#include "tinymt64.h"

#define MAX(X, Y) ((X) > (Y) ? (X) : (Y))
#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))

typedef struct {
    uint32_t connect;
    uint32_t read;
    uint32_t write;
    uint32_t status;
    uint32_t timeout;
} errors;

typedef struct {
    uint64_t samples;
    uint64_t index;
    uint64_t limit;
    uint64_t min;
    uint64_t max;
    uint64_t data[];
} stats;

stats *stats_alloc(uint64_t);
void stats_free(stats *);
void stats_reset(stats *);
void stats_rewind(stats *);

void stats_record(stats *, uint64_t);

long double stats_summarize(stats *);
long double stats_mean(stats *);
long double stats_stdev(stats *stats, long double);
long double stats_within_stdev(stats *, long double, long double, uint64_t);
uint64_t stats_percentile(stats *, long double);

void stats_sample(stats *, tinymt64_t *, uint64_t, stats *);
uint64_t rand64(tinymt64_t *, uint64_t);

#endif /* STATS_H */
