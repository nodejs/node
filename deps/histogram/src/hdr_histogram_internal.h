/**
 * hdr_histogram_internal.h
 * Non-public helpers shared across library translation units (not installed,
 * not part of the public API). Distinct from hdr_tests.h, which is for helpers
 * used only by the test suite.
 */
#ifndef HDR_HISTOGRAM_INTERNAL_H
#define HDR_HISTOGRAM_INTERNAL_H

#include <hdr/hdr_histogram.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Map a recorded value to its counts[] index. Assumes value >= 0 (callers guard);
   defined in hdr_histogram.c and used by the log codec and the packed variant. */
int32_t counts_index_for(const struct hdr_histogram* h, int64_t value);

#ifdef __cplusplus
}
#endif

#endif
