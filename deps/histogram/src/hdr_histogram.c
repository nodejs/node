/**
 * hdr_histogram.c
 * Written by Michael Barker and released to the public domain,
 * as explained at http://creativecommons.org/publicdomain/zero/1.0/
 */

#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <inttypes.h>

#include <hdr/hdr_histogram.h>
#include "hdr_tests.h"
#include "hdr_atomic.h"

#ifndef HDR_MALLOC_INCLUDE
#define HDR_MALLOC_INCLUDE "hdr_malloc.h"
#endif

#include HDR_MALLOC_INCLUDE

/* Prefetch hint for upcoming write access */
#if defined(__GNUC__) || defined(__clang__)
#  define HDR_PREFETCH_WRITE(addr) __builtin_prefetch((addr), 1, 3)
#  define HDR_LIKELY(x)   __builtin_expect(!!(x), 1)
#  define HDR_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#  define HDR_PREFETCH_WRITE(addr) ((void)(addr))
#  define HDR_LIKELY(x)   (x)
#  define HDR_UNLIKELY(x) (x)
#endif

/* Runtime-dispatched AVX2 path; rest of TU stays at baseline ISA so the binary
   doesn't silently require AVX2. 64-bit x86 + GCC/Clang only:
     - 32-bit x86: _mm_extract_epi64 unavailable in 32-bit codegen.
     - _MSC_VER: __builtin_cpu_supports's __cpu_model isn't linked under MSVC;
       clang-cl also defines __x86_64__/__clang__ so this exclusion is load-bearing.
     - __INTEL_COMPILER: ICC classic. */
#if !defined(HDR_DISABLE_AVX2) \
    && defined(__x86_64__) \
    && (defined(__GNUC__) || defined(__clang__)) && !defined(__INTEL_COMPILER) && !defined(_MSC_VER)
#  define HDR_HAS_AVX2_DISPATCH 1
#  include <immintrin.h>
#endif

/*  ######   #######  ##     ## ##    ## ########  ######  */
/* ##    ## ##     ## ##     ## ###   ##    ##    ##    ## */
/* ##       ##     ## ##     ## ####  ##    ##    ##       */
/* ##       ##     ## ##     ## ## ## ##    ##     ######  */
/* ##       ##     ## ##     ## ##  ####    ##          ## */
/* ##    ## ##     ## ##     ## ##   ###    ##    ##    ## */
/*  ######   #######   #######  ##    ##    ##     ######  */

static int32_t normalize_index(const struct hdr_histogram* h, int32_t index)
{
    int32_t normalized_index;
    int32_t adjustment = 0;
    if (h->normalizing_index_offset == 0)
    {
        return index;
    }

    normalized_index = index - h->normalizing_index_offset;

    if (normalized_index < 0)
    {
        adjustment = h->counts_len;
    }
    else if (normalized_index >= h->counts_len)
    {
        adjustment = -h->counts_len;
    }

    return normalized_index + adjustment;
}

static int64_t counts_get_direct(const struct hdr_histogram* h, int32_t index)
{
    return h->counts[index];
}

static int64_t counts_get_normalised(const struct hdr_histogram* h, int32_t index)
{
    return counts_get_direct(h, normalize_index(h, index));
}

static void counts_inc_normalised(
    struct hdr_histogram* h, int32_t index, int64_t value)
{
    if (HDR_LIKELY(h->normalizing_index_offset == 0))
    {
        HDR_PREFETCH_WRITE(&h->counts[index]);
        h->counts[index] += value;
    }
    else
    {
        int32_t normalised_index = normalize_index(h, index);
        HDR_PREFETCH_WRITE(&h->counts[normalised_index]);
        h->counts[normalised_index] += value;
    }
    h->total_count += value;
}

static void counts_inc_normalised_atomic(
    struct hdr_histogram* h, int32_t index, int64_t value)
{
    if (HDR_LIKELY(h->normalizing_index_offset == 0))
    {
        HDR_PREFETCH_WRITE(&h->counts[index]);
        hdr_atomic_add_fetch_64(&h->counts[index], value);
    }
    else
    {
        int32_t normalised_index = normalize_index(h, index);
        HDR_PREFETCH_WRITE(&h->counts[normalised_index]);
        hdr_atomic_add_fetch_64(&h->counts[normalised_index], value);
    }
    hdr_atomic_add_fetch_64(&h->total_count, value);
}

static void update_min_max(struct hdr_histogram* h, int64_t value)
{
    if (HDR_UNLIKELY(value > h->max_value))
    {
        h->max_value = value;
    }

    if (HDR_UNLIKELY(value != 0 && value < h->min_value))
    {
        h->min_value = value;
    }
}

static void update_min_max_atomic(struct hdr_histogram* h, int64_t value)
{
    int64_t current_min_value;
    int64_t current_max_value;
    do
    {
        current_min_value = hdr_atomic_load_64(&h->min_value);

        if (0 == value || current_min_value <= value)
        {
            break;
        }
    }
    while (!hdr_atomic_compare_exchange_64(&h->min_value, &current_min_value, value));

    do
    {
        current_max_value = hdr_atomic_load_64(&h->max_value);

        if (value <= current_max_value)
        {
            break;
        }
    }
    while (!hdr_atomic_compare_exchange_64(&h->max_value, &current_max_value, value));
}


/* ##     ## ######## #### ##       #### ######## ##    ## */
/* ##     ##    ##     ##  ##        ##     ##     ##  ##  */
/* ##     ##    ##     ##  ##        ##     ##      ####   */
/* ##     ##    ##     ##  ##        ##     ##       ##    */
/* ##     ##    ##     ##  ##        ##     ##       ##    */
/* ##     ##    ##     ##  ##        ##     ##       ##    */
/*  #######     ##    #### ######## ####    ##       ##    */

static int64_t power(int64_t base, int64_t exp)
{
    int64_t result = 1;
    while(exp)
    {
        result *= base; exp--;
    }
    return result;
}

#if defined(_MSC_VER) && !(defined(__clang__) && (defined(_M_ARM) || defined(_M_ARM64)))
#   if defined(_WIN64)
#       pragma intrinsic(_BitScanReverse64)
#   else
#       pragma intrinsic(_BitScanReverse)
#   endif
#endif

static int32_t count_leading_zeros_64(int64_t value)
{
#if defined(_MSC_VER) && !(defined(__clang__) && (defined(_M_ARM) || defined(_M_ARM64)))
    /* _BitScanReverse writes an unsigned long */
    unsigned long leading_zero = 0;
#if defined(_WIN64)
    _BitScanReverse64(&leading_zero, value);
#else
    uint32_t high = value >> 32;
    if  (_BitScanReverse(&leading_zero, high))
    {
        leading_zero += 32;
    }
    else
    {
        uint32_t low = value & 0x00000000FFFFFFFF;
        _BitScanReverse(&leading_zero, low);
    }
#endif
    return 63 - leading_zero; /* smallest power of 2 containing value */
#else
    return __builtin_clzll(value); /* smallest power of 2 containing value */
#endif
}

static int32_t get_bucket_index(const struct hdr_histogram* h, int64_t value)
{
    int32_t pow2ceiling = 64 - count_leading_zeros_64(value | h->sub_bucket_mask); /* smallest power of 2 containing value */
    return pow2ceiling - h->unit_magnitude - (h->sub_bucket_half_count_magnitude + 1);
}

static int32_t get_sub_bucket_index(int64_t value, int32_t bucket_index, int32_t unit_magnitude)
{
    return (int32_t)(value >> (bucket_index + unit_magnitude));
}

static int32_t counts_index(const struct hdr_histogram* h, int32_t bucket_index, int32_t sub_bucket_index)
{
    /* Calculate the index for the first entry in the bucket: */
    /* (The following is the equivalent of ((bucket_index + 1) * subBucketHalfCount) ): */
    int32_t bucket_base_index = (bucket_index + 1) << h->sub_bucket_half_count_magnitude;
    /* Calculate the offset in the bucket: */
    int32_t offset_in_bucket = sub_bucket_index - h->sub_bucket_half_count;
    /* The following is the equivalent of ((sub_bucket_index  - subBucketHalfCount) + bucketBaseIndex; */
    return bucket_base_index + offset_in_bucket;
}

static int64_t value_from_index(int32_t bucket_index, int32_t sub_bucket_index, int32_t unit_magnitude)
{
    return ((int64_t) sub_bucket_index) << (bucket_index + unit_magnitude);
}

int32_t counts_index_for(const struct hdr_histogram* h, int64_t value)
{
    int32_t bucket_index     = get_bucket_index(h, value);
    int32_t sub_bucket_index = get_sub_bucket_index(value, bucket_index, h->unit_magnitude);

    return counts_index(h, bucket_index, sub_bucket_index);
}

int64_t hdr_value_at_index(const struct hdr_histogram *h, int32_t index)
{
    int32_t bucket_index = (index >> h->sub_bucket_half_count_magnitude) - 1;
    int32_t sub_bucket_index = (index & (h->sub_bucket_half_count - 1)) + h->sub_bucket_half_count;

    if (bucket_index < 0)
    {
        sub_bucket_index -= h->sub_bucket_half_count;
        bucket_index = 0;
    }

    return value_from_index(bucket_index, sub_bucket_index, h->unit_magnitude);
}

int64_t hdr_size_of_equivalent_value_range(const struct hdr_histogram* h, int64_t value)
{
    int32_t bucket_index     = get_bucket_index(h, value);
    int32_t sub_bucket_index = get_sub_bucket_index(value, bucket_index, h->unit_magnitude);
    int32_t adjusted_bucket  = (sub_bucket_index >= h->sub_bucket_count) ? (bucket_index + 1) : bucket_index;
    return INT64_C(1) << (h->unit_magnitude + adjusted_bucket);
}

static int64_t size_of_equivalent_value_range_given_bucket_indices(
    const struct hdr_histogram *h,
    int32_t bucket_index,
    int32_t sub_bucket_index)
{
    const int32_t adjusted_bucket  = (sub_bucket_index >= h->sub_bucket_count) ? (bucket_index + 1) : bucket_index;
    return INT64_C(1) << (h->unit_magnitude + adjusted_bucket);
}

static int64_t lowest_equivalent_value(const struct hdr_histogram* h, int64_t value)
{
    int32_t bucket_index     = get_bucket_index(h, value);
    int32_t sub_bucket_index = get_sub_bucket_index(value, bucket_index, h->unit_magnitude);
    return value_from_index(bucket_index, sub_bucket_index, h->unit_magnitude);
}

static int64_t lowest_equivalent_value_given_bucket_indices(
    const struct hdr_histogram *h,
    int32_t bucket_index,
    int32_t sub_bucket_index)
{
    return value_from_index(bucket_index, sub_bucket_index, h->unit_magnitude);
}

int64_t hdr_next_non_equivalent_value(const struct hdr_histogram *h, int64_t value)
{
    int64_t low  = lowest_equivalent_value(h, value);
    int64_t size = hdr_size_of_equivalent_value_range(h, value);
    /* saturate: top-bucket low+size overflows int64 (UB) */
    if (low > INT64_MAX - size)
    {
        return INT64_MAX;
    }
    return low + size;
}

static int64_t highest_equivalent_value(const struct hdr_histogram* h, int64_t value)
{
    int64_t low  = lowest_equivalent_value(h, value);
    int64_t size = hdr_size_of_equivalent_value_range(h, value);
    /* clamp: top-bucket low+size-1 overflows int64; keep value <= highest_equivalent_value */
    if (low > INT64_MAX - size)
    {
        return INT64_MAX;
    }
    return low + size - 1;
}

int64_t hdr_median_equivalent_value(const struct hdr_histogram *h, int64_t value)
{
    return lowest_equivalent_value(h, value) + (hdr_size_of_equivalent_value_range(h, value) >> 1);
}

static int64_t non_zero_min(const struct hdr_histogram* h)
{
    if (INT64_MAX == h->min_value)
    {
        return INT64_MAX;
    }

    return lowest_equivalent_value(h, h->min_value);
}

bool hdr_reset_internal_counters_checked(struct hdr_histogram* h)
{
    bool overflow = false;
    int min_non_zero_index = -1;
    int max_index = -1;
    int64_t observed_total_count = 0;
    int i;

    for (i = 0; i < h->counts_len; i++)
    {
        int64_t count_at_index;

        /* logical index: pair the count with hdr_value_at_index below (offset-aware) */
        if ((count_at_index = counts_get_normalised(h, i)) > 0)
        {
            if (count_at_index > INT64_MAX - observed_total_count)
            {
                observed_total_count = INT64_MAX;
                overflow = true;
            }
            else
            {
                observed_total_count += count_at_index;
            }
            max_index = i;
            if (min_non_zero_index == -1 && i != 0)
            {
                min_non_zero_index = i;
            }
        }
    }

    if (max_index == -1)
    {
        h->max_value = 0;
    }
    else
    {
        int64_t max_value = hdr_value_at_index(h, max_index);
        h->max_value = highest_equivalent_value(h, max_value);
    }

    if (min_non_zero_index == -1)
    {
        h->min_value = INT64_MAX;
    }
    else
    {
        h->min_value = hdr_value_at_index(h, min_non_zero_index);
    }

    h->total_count = observed_total_count;
    return !overflow;
}

void hdr_reset_internal_counters(struct hdr_histogram* h)
{
    (void) hdr_reset_internal_counters_checked(h);
}

static int32_t buckets_needed_to_cover_value(int64_t value, int32_t sub_bucket_count, int32_t unit_magnitude)
{
    int64_t smallest_untrackable_value = ((int64_t) sub_bucket_count) << unit_magnitude;
    int32_t buckets_needed = 1;
    while (smallest_untrackable_value <= value)
    {
        if (smallest_untrackable_value > INT64_MAX / 2)
        {
            return buckets_needed + 1;
        }
        smallest_untrackable_value <<= 1;
        buckets_needed++;
    }

    return buckets_needed;
}

/* ##     ## ######## ##     ##  #######  ########  ##    ## */
/* ###   ### ##       ###   ### ##     ## ##     ##  ##  ##  */
/* #### #### ##       #### #### ##     ## ##     ##   ####   */
/* ## ### ## ######   ## ### ## ##     ## ########     ##    */
/* ##     ## ##       ##     ## ##     ## ##   ##      ##    */
/* ##     ## ##       ##     ## ##     ## ##    ##     ##    */
/* ##     ## ######## ##     ##  #######  ##     ##    ##    */

int hdr_calculate_bucket_config(
    int64_t lowest_discernible_value,
    int64_t highest_trackable_value,
    int significant_figures,
    struct hdr_histogram_bucket_config* cfg)
{
    int32_t sub_bucket_count_magnitude;
    int64_t largest_value_with_single_unit_resolution;

    /* define cfg on every reject path so a two-step-init caller that mishandles
       the EINVAL return never reads uninitialized fields */
    memset(cfg, 0, sizeof(*cfg));

    if (lowest_discernible_value < 1 ||
            significant_figures < 1 || 5 < significant_figures ||
            /* division form: lowest*2 near INT64_MAX overflows int64 (UB) */
            lowest_discernible_value > highest_trackable_value / 2)
    {
        return EINVAL;
    }

    cfg->lowest_discernible_value = lowest_discernible_value;
    cfg->significant_figures = significant_figures;
    cfg->highest_trackable_value = highest_trackable_value;

    largest_value_with_single_unit_resolution = 2 * power(10, significant_figures);
    sub_bucket_count_magnitude = (int32_t) ceil(log((double)largest_value_with_single_unit_resolution) / log(2));
    cfg->sub_bucket_half_count_magnitude = ((sub_bucket_count_magnitude > 1) ? sub_bucket_count_magnitude : 1) - 1;

    double unit_magnitude = log((double)lowest_discernible_value) / log(2);
    if (INT32_MAX < unit_magnitude)
    {
        return EINVAL;
    }

    cfg->unit_magnitude = (int32_t) unit_magnitude;
    cfg->sub_bucket_count      = (int32_t) pow(2, (cfg->sub_bucket_half_count_magnitude + 1));
    cfg->sub_bucket_half_count = cfg->sub_bucket_count / 2;

    /* reject before shifting: sub_bucket_mask shift past bit 61 is signed-shift UB */
    if (cfg->unit_magnitude + cfg->sub_bucket_half_count_magnitude > 61)
    {
        return EINVAL;
    }

    cfg->sub_bucket_mask       = ((int64_t) cfg->sub_bucket_count - 1) << cfg->unit_magnitude;

    cfg->bucket_count = buckets_needed_to_cover_value(highest_trackable_value, cfg->sub_bucket_count, (int32_t)cfg->unit_magnitude);
    cfg->counts_len = (cfg->bucket_count + 1) * (cfg->sub_bucket_count / 2);

    return 0;
}

void hdr_init_preallocated(struct hdr_histogram* h, struct hdr_histogram_bucket_config* cfg)
{
    h->lowest_discernible_value        = cfg->lowest_discernible_value;
    h->highest_trackable_value         = cfg->highest_trackable_value;
    h->unit_magnitude                  = (int32_t)cfg->unit_magnitude;
    h->significant_figures             = (int32_t)cfg->significant_figures;
    h->sub_bucket_half_count_magnitude = cfg->sub_bucket_half_count_magnitude;
    h->sub_bucket_half_count           = cfg->sub_bucket_half_count;
    h->sub_bucket_mask                 = cfg->sub_bucket_mask;
    h->sub_bucket_count                = cfg->sub_bucket_count;
    h->min_value                       = INT64_MAX;
    h->max_value                       = 0;
    h->normalizing_index_offset        = 0;
    h->conversion_ratio                = 1.0;
    h->bucket_count                    = cfg->bucket_count;
    h->counts_len                      = cfg->counts_len;
    h->total_count                     = 0;
}

int hdr_init(
    int64_t lowest_discernible_value,
    int64_t highest_trackable_value,
    int significant_figures,
    struct hdr_histogram** result)
{
    int64_t* counts;
    struct hdr_histogram_bucket_config cfg;
    struct hdr_histogram* histogram;

    int r = hdr_calculate_bucket_config(lowest_discernible_value, highest_trackable_value, significant_figures, &cfg);
    if (r)
    {
        return r;
    }

    counts = (int64_t*) hdr_calloc((size_t) cfg.counts_len, sizeof(int64_t));
    if (!counts)
    {
        return ENOMEM;
    }

    histogram = (struct hdr_histogram*) hdr_calloc(1, sizeof(struct hdr_histogram));
    if (!histogram)
    {
        hdr_free(counts);
        return ENOMEM;
    }

    histogram->counts = counts;

    hdr_init_preallocated(histogram, &cfg);
    *result = histogram;

    return 0;
}

void hdr_close(struct hdr_histogram* h)
{
    if (h) {
	hdr_free(h->counts);
	hdr_free(h);
    }
}

int hdr_alloc(int64_t highest_trackable_value, int significant_figures, struct hdr_histogram** result)
{
    return hdr_init(1, highest_trackable_value, significant_figures, result);
}

/* reset a histogram to zero. */
void hdr_reset(struct hdr_histogram *h)
{
     h->total_count=0;
     h->min_value = INT64_MAX;
     h->max_value = 0;
     memset(h->counts, 0, (sizeof(int64_t) * h->counts_len));
}

size_t hdr_get_memory_size(struct hdr_histogram *h)
{
    return sizeof(struct hdr_histogram) + h->counts_len * sizeof(int64_t);
}

/* ##     ## ########  ########     ###    ######## ########  ######  */
/* ##     ## ##     ## ##     ##   ## ##      ##    ##       ##    ## */
/* ##     ## ##     ## ##     ##  ##   ##     ##    ##       ##       */
/* ##     ## ########  ##     ## ##     ##    ##    ######    ######  */
/* ##     ## ##        ##     ## #########    ##    ##             ## */
/* ##     ## ##        ##     ## ##     ##    ##    ##       ##    ## */
/*  #######  ##        ########  ##     ##    ##    ########  ######  */


/* Shared record body. The count-sign check lives in hdr_record_values()/_atomic()
   below, keeping the single-value hot path (count == 1, never negative) free of it. */
static bool record_value_counted(struct hdr_histogram* h, int64_t value, int64_t count)
{
    int32_t counts_index;

    if (value < 0 || h->highest_trackable_value < value)
    {
        return false;
    }

    counts_index = counts_index_for(h, value);
    if ((uint32_t)counts_index >= (uint32_t)h->counts_len)
    {
        return false;
    }

    counts_inc_normalised(h, counts_index, count);
    update_min_max(h, value);

    return true;
}

static bool record_value_counted_atomic(struct hdr_histogram* h, int64_t value, int64_t count)
{
    int32_t counts_index;

    if (value < 0 || h->highest_trackable_value < value)
    {
        return false;
    }

    counts_index = counts_index_for(h, value);
    if ((uint32_t)counts_index >= (uint32_t)h->counts_len)
    {
        return false;
    }

    counts_inc_normalised_atomic(h, counts_index, count);
    update_min_max_atomic(h, value);

    return true;
}

bool hdr_record_value(struct hdr_histogram* h, int64_t value)
{
    return record_value_counted(h, value, 1);
}

bool hdr_record_value_atomic(struct hdr_histogram* h, int64_t value)
{
    return record_value_counted_atomic(h, value, 1);
}

bool hdr_record_value_capped(struct hdr_histogram* h, int64_t value)
{
    int64_t capped = (value > h->highest_trackable_value) ? h->highest_trackable_value : value;
    return hdr_record_value(h, capped < 0 ? 0 : capped);
}

bool hdr_record_value_capped_atomic(struct hdr_histogram* h, int64_t value)
{
    int64_t capped = (value > h->highest_trackable_value) ? h->highest_trackable_value : value;
    return hdr_record_value_atomic(h, capped < 0 ? 0 : capped);
}

bool hdr_record_values(struct hdr_histogram* h, int64_t value, int64_t count)
{
    if (count < 0)  /* non-negative counts; scan assumes a monotonic prefix */
    {
        return false;
    }
    return record_value_counted(h, value, count);
}

bool hdr_record_values_atomic(struct hdr_histogram* h, int64_t value, int64_t count)
{
    if (count < 0)  /* see hdr_record_values */
    {
        return false;
    }
    return record_value_counted_atomic(h, value, count);
}

bool hdr_record_corrected_value(struct hdr_histogram* h, int64_t value, int64_t expected_interval)
{
    return hdr_record_corrected_values(h, value, 1, expected_interval);
}

bool hdr_record_corrected_value_atomic(struct hdr_histogram* h, int64_t value, int64_t expected_interval)
{
    return hdr_record_corrected_values_atomic(h, value, 1, expected_interval);
}

bool hdr_record_corrected_values(struct hdr_histogram* h, int64_t value, int64_t count, int64_t expected_interval)
{
    int64_t missing_value;

    if (!hdr_record_values(h, value, count))
    {
        return false;
    }

    if (expected_interval <= 0 || value <= expected_interval)
    {
        return true;
    }

    missing_value = value - expected_interval;
    for (; missing_value >= expected_interval; missing_value -= expected_interval)
    {
        if (!hdr_record_values(h, missing_value, count))
        {
            return false;
        }
    }

    return true;
}

bool hdr_record_corrected_values_atomic(struct hdr_histogram* h, int64_t value, int64_t count, int64_t expected_interval)
{
    int64_t missing_value;

    if (!hdr_record_values_atomic(h, value, count))
    {
        return false;
    }

    if (expected_interval <= 0 || value <= expected_interval)
    {
        return true;
    }

    missing_value = value - expected_interval;
    for (; missing_value >= expected_interval; missing_value -= expected_interval)
    {
        if (!hdr_record_values_atomic(h, missing_value, count))
        {
            return false;
        }
    }

    return true;
}

int64_t hdr_add(struct hdr_histogram* h, const struct hdr_histogram* from)
{
    struct hdr_iter iter;
    int64_t dropped = 0;
    hdr_iter_recorded_init(&iter, from);

    while (hdr_iter_next(&iter))
    {
        int64_t value = iter.value;
        int64_t count = iter.count;

        if (!hdr_record_values(h, value, count))
        {
            dropped += count;
        }
    }

    return dropped;
}

int64_t hdr_add_while_correcting_for_coordinated_omission(
        struct hdr_histogram* h, struct hdr_histogram* from, int64_t expected_interval)
{
    struct hdr_iter iter;
    int64_t dropped = 0;
    hdr_iter_recorded_init(&iter, from);

    while (hdr_iter_next(&iter))
    {
        int64_t value = iter.value;
        int64_t count = iter.count;

        if (!hdr_record_corrected_values(h, value, count, expected_interval))
        {
            dropped += count;
        }
    }

    return dropped;
}



/* ##     ##    ###    ##       ##     ## ########  ######  */
/* ##     ##   ## ##   ##       ##     ## ##       ##    ## */
/* ##     ##  ##   ##  ##       ##     ## ##       ##       */
/* ##     ## ##     ## ##       ##     ## ######    ######  */
/*  ##   ##  ######### ##       ##     ## ##             ## */
/*   ## ##   ##     ## ##       ##     ## ##       ##    ## */
/*    ###    ##     ## ########  #######  ########  ######  */


int64_t hdr_max(const struct hdr_histogram* h)
{
    if (0 == h->max_value)
    {
        return 0;
    }

    return highest_equivalent_value(h, h->max_value);
}

int64_t hdr_total_count(const struct hdr_histogram* h)
{
    /* atomic load: safe to call while other threads use the *_atomic record functions */
    return h != NULL ? hdr_atomic_load_64((int64_t*) &h->total_count) : 0;
}

int64_t hdr_min(const struct hdr_histogram* h)
{
    if (0 < hdr_count_at_index(h, 0))
    {
        return 0;
    }

    return non_zero_min(h);
}

static int64_t get_value_from_idx_up_to_count_scalar(
    const struct hdr_histogram* h, int64_t count_at_percentile)
{
    /* Block-summed scan: sum BLK counts, test the running total once per block,
       and do the exact per-element walk only for the crossing block. offset != 0
       (decoded/rotated) reads via the offset-aware accessor. */
    enum { BLK = 4 };
    const int64_t* counts = h->counts;
    const int32_t n = h->counts_len;
    int32_t idx = 0;
    int64_t running = 0;

    if (HDR_UNLIKELY(h->normalizing_index_offset != 0))
    {
        for (idx = 0; idx < n; idx++)
        {
            running += counts_get_normalised(h, idx);
            if (running >= count_at_percentile)
                return hdr_value_at_index(h, idx);
        }
        return 0;
    }

    {
        const int32_t blk_limit = n - (n % BLK);
        for (; idx < blk_limit; idx += BLK)
        {
            /* unsigned block sum: cannot overflow under valid state (matches AVX2 path) */
            uint64_t block_sum_u = 0;
            int32_t j;
            for (j = 0; j < BLK; j++)
                block_sum_u += (uint64_t)counts[idx + j];
            if (HDR_UNLIKELY((uint64_t)running + block_sum_u >= (uint64_t)count_at_percentile))
            {
#if defined(__aarch64__) && defined(__clang__) && !defined(__APPLE__)
                /* Keep crossing-block prefix sums out of the block-sum loop. */
#pragma clang loop unroll(disable)
#endif
                for (j = 0; j < BLK; j++)
                {
                    running += counts[idx + j];
                    if (running >= count_at_percentile)
                        return hdr_value_at_index(h, idx + j);
                }
            }
            else
            {
                running += (int64_t)block_sum_u;
            }
        }
    }

    for (; idx < n; idx++)
    {
        running += counts[idx];
        if (running >= count_at_percentile)
            return hdr_value_at_index(h, idx);
    }

    return 0;
}

#ifdef HDR_HAS_AVX2_DISPATCH
__attribute__((target("avx2")))
static int64_t get_value_from_idx_up_to_count_avx2(
    const struct hdr_histogram* h, int64_t count_at_percentile)
{
    int64_t running = 0;
    int32_t idx = 0;
    /* 16 int64 (4x256-bit) per iteration: amortize the horizontal reduction +
       extract + target-cross branch over 16 elements instead of 4. */
    const int32_t limit = h->counts_len & ~15;

    for (; idx < limit; idx += 16) {
        /* prefetch 512 B ahead to hide L2/L3 latency; clamp in-bounds — a
           past-end pointer is UB even for a hint. */
        int32_t pf = idx + 4 * 16;
        _mm_prefetch((const char*)&h->counts[pf < h->counts_len ? pf : h->counts_len - 1], _MM_HINT_T0);
        __m256i a = _mm256_loadu_si256((const __m256i*)&h->counts[idx]);
        __m256i b = _mm256_loadu_si256((const __m256i*)&h->counts[idx + 4]);
        __m256i c = _mm256_loadu_si256((const __m256i*)&h->counts[idx + 8]);
        __m256i d = _mm256_loadu_si256((const __m256i*)&h->counts[idx + 12]);
        __m256i vsum = _mm256_add_epi64(_mm256_add_epi64(a, b), _mm256_add_epi64(c, d));
        __m128i lo = _mm256_castsi256_si128(vsum);
        __m128i hi = _mm256_extracti128_si256(vsum, 1);
        __m128i s = _mm_add_epi64(lo, hi);
        /* Reduce with unsigned arithmetic to avoid signed-overflow UB. */
        int64_t chunk = (int64_t)((uint64_t)_mm_extract_epi64(s, 0)
                                + (uint64_t)_mm_extract_epi64(s, 1));

        /* counts[] are non-negative (the record path rejects count < 0), so the
           prefix sum is monotonic: block-skip is exact and only the crossing block
           is walked. */
        int64_t next = (int64_t)((uint64_t)running + (uint64_t)chunk);
        if (HDR_UNLIKELY(next >= count_at_percentile)) {
            for (int32_t j = idx; j < idx + 16; j++) {
                running = (int64_t)((uint64_t)running + (uint64_t)h->counts[j]);
                if (running >= count_at_percentile)
                    return hdr_value_at_index(h, j);
            }
        }
        else {
            running = next;
        }
    }
    for (; idx < h->counts_len; idx++) {
        running = (int64_t)((uint64_t)running + (uint64_t)h->counts[idx]);
        if (running >= count_at_percentile)
            return hdr_value_at_index(h, idx);
    }
    return 0;
}
#endif

static int64_t get_value_from_idx_up_to_count(const struct hdr_histogram* h, int64_t count_at_percentile)
{
    count_at_percentile = count_at_percentile > 0 ? count_at_percentile : 1;
#ifdef HDR_HAS_AVX2_DISPATCH
    /* AVX2 reads counts[] directly; offset != 0 (rotated) must use the scalar scan */
    if (h->normalizing_index_offset == 0 && __builtin_cpu_supports("avx2"))
        return get_value_from_idx_up_to_count_avx2(h, count_at_percentile);
#endif
    return get_value_from_idx_up_to_count_scalar(h, count_at_percentile);
}


int64_t hdr_value_at_percentile(const struct hdr_histogram* h, double percentile)
{
    double requested_percentile = percentile < 100.0 ? percentile : 100.0;
    int64_t count_at_percentile =
        (int64_t) (((requested_percentile / 100) * h->total_count) + 0.5);
    int64_t value_from_idx = get_value_from_idx_up_to_count(h, count_at_percentile);
    if (percentile == 0.0)
    {
        return lowest_equivalent_value(h, value_from_idx);
    }
    return highest_equivalent_value(h, value_from_idx);
}

int hdr_value_at_percentiles(const struct hdr_histogram *h, const double *percentiles, int64_t *values, size_t length)
{
    if (NULL == percentiles || NULL == values)
    {
        return EINVAL;
    }

    struct hdr_iter iter;
    const int64_t total_count = h->total_count;
    // to avoid allocations we use the values array for intermediate computation
    // i.e. to store the expected cumulative count at each percentile
    for (size_t i = 0; i < length; i++)
    {
        const double requested_percentile = percentiles[i] < 100.0 ? percentiles[i] : 100.0;
        const int64_t count_at_percentile =
        (int64_t) (((requested_percentile / 100) * total_count) + 0.5);
        values[i] = count_at_percentile > 1 ? count_at_percentile : 1;
    }

    uint64_t total = 0; /* unsigned: no signed-overflow UB when a hostile block sum is added at once */
    size_t at_pos = 0;

    if (HDR_LIKELY(h->normalizing_index_offset == 0))
    {
        /* Skip whole blocks that cannot reach the next target. counts[] are
           non-negative (the record path rejects count < 0), so the prefix sum is
           monotonic and this block-skip is exact for any valid histogram. */
        enum { BATCH_SCAN_BLOCK = 8 };
        const int64_t* counts = h->counts;
        const int32_t len = h->counts_len;
        int32_t idx = 0;
        for (; idx + BATCH_SCAN_BLOCK <= len && at_pos < length; idx += BATCH_SCAN_BLOCK)
        {
            /* unsigned sum keeps the accumulation UB-free even at the int64 boundary */
            const uint64_t s =
                (uint64_t)counts[idx]     + (uint64_t)counts[idx + 1] +
                (uint64_t)counts[idx + 2] + (uint64_t)counts[idx + 3] +
                (uint64_t)counts[idx + 4] + (uint64_t)counts[idx + 5] +
                (uint64_t)counts[idx + 6] + (uint64_t)counts[idx + 7];
            if ((int64_t)(total + s) >= values[at_pos])
            {
                int32_t j;
                for (j = idx; j < idx + BATCH_SCAN_BLOCK; j++)
                {
                    total += (uint64_t)counts[j];
                    while (at_pos < length && (int64_t)total >= values[at_pos])
                    {
                        values[at_pos] = highest_equivalent_value(h, hdr_value_at_index(h, j));
                        at_pos++;
                    }
                }
            }
            else
            {
                total += s;
            }
        }
        /* Tail: fewer than BATCH_SCAN_BLOCK counters remain. */
        for (; idx < len && at_pos < length; idx++)
        {
            total += (uint64_t)counts[idx];
            while (at_pos < length && (int64_t)total >= values[at_pos])
            {
                values[at_pos] = highest_equivalent_value(h, hdr_value_at_index(h, idx));
                at_pos++;
            }
        }
    }
    else
    {
        /* offset-aware fallback (normalizing_index_offset != 0): iterator
           dereferences counts through the normalized index */
        hdr_iter_init(&iter, h);
        while (hdr_iter_next(&iter) && at_pos < length)
        {
            total += (uint64_t)iter.count;
            while (at_pos < length && (int64_t)total >= values[at_pos])
            {
                values[at_pos] = highest_equivalent_value(h, iter.value);
                at_pos++;
            }
        }
    }
    return 0;
}

double hdr_mean(const struct hdr_histogram* h)
{
    struct hdr_iter iter;
    double total = 0;
    int64_t count = 0;
    int64_t total_count = h->total_count;

    hdr_iter_init(&iter, h);

    while (hdr_iter_next(&iter) && count < total_count)
    {
        if (0 != iter.count)
        {
            count += iter.count;
            /* sum in double: count*median can overflow int64 (UB) for large values */
            total += (double) iter.count * (double) hdr_median_equivalent_value(h, iter.value);
        }
    }

    return total / total_count;
}

double hdr_stddev(const struct hdr_histogram* h)
{
    double mean = hdr_mean(h);
    double geometric_dev_total = 0.0;

    struct hdr_iter iter;
    hdr_iter_init(&iter, h);

    while (hdr_iter_next(&iter))
    {
        if (0 != iter.count)
        {
            double dev = (hdr_median_equivalent_value(h, iter.value) * 1.0) - mean;
            geometric_dev_total += (dev * dev) * iter.count;
        }
    }

    return sqrt(geometric_dev_total / h->total_count);
}

bool hdr_values_are_equivalent(const struct hdr_histogram* h, int64_t a, int64_t b)
{
    return lowest_equivalent_value(h, a) == lowest_equivalent_value(h, b);
}

int64_t hdr_lowest_equivalent_value(const struct hdr_histogram* h, int64_t value)
{
    return lowest_equivalent_value(h, value);
}

int64_t hdr_count_at_value(const struct hdr_histogram* h, int64_t value)
{
    int32_t counts_index;

    if (value < 0) { return 0; }
    /* value past the array's top half-bucket maps outside counts[] (OOB); count 0 */
    counts_index = counts_index_for(h, value);
    if ((uint32_t)counts_index >= (uint32_t)h->counts_len)
    {
        return 0;
    }

    return counts_get_normalised(h, counts_index);
}

int64_t hdr_count_at_index(const struct hdr_histogram* h, int32_t index)
{
    /* reject index outside counts[] (OOB read); unsigned compare also catches negatives */
    if ((uint32_t)index >= (uint32_t)h->counts_len)
    {
        return 0;
    }
    return counts_get_normalised(h, index);
}


/* #### ######## ######## ########     ###    ########  #######  ########   ######  */
/*  ##     ##    ##       ##     ##   ## ##      ##    ##     ## ##     ## ##    ## */
/*  ##     ##    ##       ##     ##  ##   ##     ##    ##     ## ##     ## ##       */
/*  ##     ##    ######   ########  ##     ##    ##    ##     ## ########   ######  */
/*  ##     ##    ##       ##   ##   #########    ##    ##     ## ##   ##         ## */
/*  ##     ##    ##       ##    ##  ##     ##    ##    ##     ## ##    ##  ##    ## */
/* ####    ##    ######## ##     ## ##     ##    ##     #######  ##     ##  ######  */


static bool has_buckets(struct hdr_iter* iter)
{
    return iter->counts_index < iter->h->counts_len;
}

static bool has_next(struct hdr_iter* iter)
{
    return iter->cumulative_count < iter->total_count;
}

static bool move_next(struct hdr_iter* iter)
{
    iter->counts_index++;

    if (!has_buckets(iter))
    {
        return false;
    }

    iter->count = counts_get_normalised(iter->h, iter->counts_index);
    iter->cumulative_count += iter->count;
    const int64_t value = hdr_value_at_index(iter->h, iter->counts_index);
    const int32_t bucket_index = get_bucket_index(iter->h, value);
    const int32_t sub_bucket_index = get_sub_bucket_index(value, bucket_index, iter->h->unit_magnitude);
    const int64_t leq = lowest_equivalent_value_given_bucket_indices(iter->h, bucket_index, sub_bucket_index);
    const int64_t size_of_equivalent_value_range = size_of_equivalent_value_range_given_bucket_indices(
        iter->h, bucket_index, sub_bucket_index);
    iter->lowest_equivalent_value = leq;
    iter->value = value;
    /* saturate: top-bucket leq+size overflows int64 (UB) */
    iter->highest_equivalent_value =
        (leq > INT64_MAX - size_of_equivalent_value_range)
            ? INT64_MAX
            : leq + size_of_equivalent_value_range - 1;
    iter->median_equivalent_value = leq + (size_of_equivalent_value_range >> 1);

    return true;
}

static int64_t peek_next_value_from_index(struct hdr_iter* iter)
{
    const int32_t index = iter->counts_index + 1;
    int32_t bucket_index = (int32_t) ((uint32_t) index >> iter->h->sub_bucket_half_count_magnitude) - 1;
    int32_t sub_bucket_index = (index & (iter->h->sub_bucket_half_count - 1)) + iter->h->sub_bucket_half_count;
    int32_t shift;
    if (bucket_index < 0)
    {
        sub_bucket_index -= iter->h->sub_bucket_half_count;
        bucket_index = 0;
    }
    shift = bucket_index + iter->h->unit_magnitude;
    /* one past the top bucket shifts into the sign bit for a near-INT64_MAX range; saturate */
    if (shift >= 63 || (uint64_t) sub_bucket_index > ((uint64_t) INT64_MAX >> shift))
    {
        return INT64_MAX;
    }
    return value_from_index(bucket_index, sub_bucket_index, iter->h->unit_magnitude);
}

static bool next_value_greater_than_reporting_level_upper_bound(
    struct hdr_iter *iter, int64_t reporting_level_upper_bound)
{
    if (iter->counts_index >= iter->h->counts_len)
    {
        return false;
    }

    return peek_next_value_from_index(iter) > reporting_level_upper_bound;
}

static bool basic_iter_next(struct hdr_iter *iter)
{
    if (!has_next(iter) || iter->counts_index >= iter->h->counts_len)
    {
        return false;
    }

    move_next(iter);

    return true;
}

static void update_iterated_values(struct hdr_iter* iter, int64_t new_value_iterated_to)
{
    iter->value_iterated_from = iter->value_iterated_to;
    iter->value_iterated_to = new_value_iterated_to;
}

static bool all_values_iter_next(struct hdr_iter* iter)
{
    bool result = move_next(iter);

    if (result)
    {
        update_iterated_values(iter, iter->value);
    }

    return result;
}

void hdr_iter_init(struct hdr_iter* iter, const struct hdr_histogram* h)
{
    iter->h = h;

    iter->counts_index = -1;
    iter->total_count = h->total_count;
    iter->count = 0;
    iter->cumulative_count = 0;
    iter->value = 0;
    iter->highest_equivalent_value = 0;
    iter->value_iterated_from = 0;
    iter->value_iterated_to = 0;

    iter->_next_fp = all_values_iter_next;
}

bool hdr_iter_next(struct hdr_iter* iter)
{
    return iter->_next_fp(iter);
}

/* ########  ######## ########   ######  ######## ##    ## ######## #### ##       ########  ######  */
/* ##     ## ##       ##     ## ##    ## ##       ###   ##    ##     ##  ##       ##       ##    ## */
/* ##     ## ##       ##     ## ##       ##       ####  ##    ##     ##  ##       ##       ##       */
/* ########  ######   ########  ##       ######   ## ## ##    ##     ##  ##       ######    ######  */
/* ##        ##       ##   ##   ##       ##       ##  ####    ##     ##  ##       ##             ## */
/* ##        ##       ##    ##  ##    ## ##       ##   ###    ##     ##  ##       ##       ##    ## */
/* ##        ######## ##     ##  ######  ######## ##    ##    ##    #### ######## ########  ######  */

static bool percentile_iter_next(struct hdr_iter* iter)
{
    int64_t temp, half_distance, percentile_reporting_ticks;

    struct hdr_iter_percentiles* percentiles = &iter->specifics.percentiles;

    if (!has_next(iter))
    {
        if (percentiles->seen_last_value)
        {
            return false;
        }

        percentiles->seen_last_value = true;
        percentiles->percentile = 100.0;

        return true;
    }

    if (iter->counts_index == -1 && !basic_iter_next(iter))
    {
        return false;
    }

    do
    {
        double current_percentile = (100.0 * (double) iter->cumulative_count) / iter->h->total_count;
        if (iter->count != 0 &&
                percentiles->percentile_to_iterate_to <= current_percentile)
        {
            update_iterated_values(iter, highest_equivalent_value(iter->h, iter->value));

            percentiles->percentile = percentiles->percentile_to_iterate_to;
            temp = (int64_t)(log(100 / (100.0 - (percentiles->percentile_to_iterate_to))) / log(2)) + 1;
            half_distance = (int64_t) pow(2, (double) temp);
            percentile_reporting_ticks = percentiles->ticks_per_half_distance * half_distance;
            percentiles->percentile_to_iterate_to += 100.0 / percentile_reporting_ticks;

            return true;
        }
    }
    while (basic_iter_next(iter));

    return true;
}

void hdr_iter_percentile_init(struct hdr_iter* iter, const struct hdr_histogram* h, int32_t ticks_per_half_distance)
{
    iter->h = h;

    hdr_iter_init(iter, h);

    iter->specifics.percentiles.seen_last_value          = false;
    iter->specifics.percentiles.ticks_per_half_distance  = ticks_per_half_distance;
    iter->specifics.percentiles.percentile_to_iterate_to = 0.0;
    iter->specifics.percentiles.percentile               = 0.0;

    iter->_next_fp = percentile_iter_next;
}

static void format_line_string(char* str, size_t len, int significant_figures, format_type format)
{
#if defined(_MSC_VER)
#define snprintf _snprintf
#pragma warning(push)
#pragma warning(disable: 4996)
#endif
    const char* format_str = "%s%d%s";

    switch (format)
    {
        case CSV:
            snprintf(str, len, format_str, "%.", significant_figures, "f,%f,%d,%.2f\n");
            break;
        case CLASSIC:
            snprintf(str, len, format_str, "%12.", significant_figures, "f %12f %12d %12.2f\n");
            break;
        default:
            snprintf(str, len, format_str, "%12.", significant_figures, "f %12f %12d %12.2f\n");
    }
#if defined(_MSC_VER)
#undef snprintf
#pragma warning(pop)
#endif
}


/* ########  ########  ######   #######  ########  ########  ######## ########   */
/* ##     ## ##       ##    ## ##     ## ##     ## ##     ## ##       ##     ##  */
/* ##     ## ##       ##       ##     ## ##     ## ##     ## ##       ##     ##  */
/* ########  ######   ##       ##     ## ########  ##     ## ######   ##     ##  */
/* ##   ##   ##       ##       ##     ## ##   ##   ##     ## ##       ##     ##  */
/* ##    ##  ##       ##    ## ##     ## ##    ##  ##     ## ##       ##     ##  */
/* ##     ## ########  ######   #######  ##     ## ########  ######## ########   */


static bool recorded_iter_next(struct hdr_iter* iter)
{
    while (basic_iter_next(iter))
    {
        if (iter->count != 0)
        {
            update_iterated_values(iter, iter->value);

            iter->specifics.recorded.count_added_in_this_iteration_step = iter->count;
            return true;
        }
    }

    return false;
}

void hdr_iter_recorded_init(struct hdr_iter* iter, const struct hdr_histogram* h)
{
    hdr_iter_init(iter, h);

    iter->specifics.recorded.count_added_in_this_iteration_step = 0;

    iter->_next_fp = recorded_iter_next;
}

/* ##       #### ##    ## ########    ###    ########  */
/* ##        ##  ###   ## ##         ## ##   ##     ## */
/* ##        ##  ####  ## ##        ##   ##  ##     ## */
/* ##        ##  ## ## ## ######   ##     ## ########  */
/* ##        ##  ##  #### ##       ######### ##   ##   */
/* ##        ##  ##   ### ##       ##     ## ##    ##  */
/* ######## #### ##    ## ######## ##     ## ##     ## */


static bool iter_linear_next(struct hdr_iter* iter)
{
    struct hdr_iter_linear* linear = &iter->specifics.linear;

    linear->count_added_in_this_iteration_step = 0;

    if (has_next(iter) ||
        next_value_greater_than_reporting_level_upper_bound(
            iter, linear->next_value_reporting_level_lowest_equivalent))
    {
        do
        {
            if (iter->value >= linear->next_value_reporting_level_lowest_equivalent)
            {
                update_iterated_values(iter, linear->next_value_reporting_level);

                /* Emit the saturated final level once before entering the terminal state. */
                if (linear->next_value_reporting_level == INT64_MAX)
                {
                    linear->next_value_reporting_level_lowest_equivalent = INT64_MAX;
                }
                else if (linear->value_units_per_bucket <= 0 ||
                    linear->next_value_reporting_level > INT64_MAX - linear->value_units_per_bucket)
                {
                    /* step <= 0 first: never-advances (infinite loop) and guards the subtraction; second clause is the overflow guard */
                    linear->next_value_reporting_level = INT64_MAX;
                    linear->next_value_reporting_level_lowest_equivalent =
                        lowest_equivalent_value(iter->h, INT64_MAX);
                }
                else
                {
                    linear->next_value_reporting_level += linear->value_units_per_bucket;
                    linear->next_value_reporting_level_lowest_equivalent =
                        lowest_equivalent_value(iter->h, linear->next_value_reporting_level);
                }

                return true;
            }

            if (!move_next(iter))
            {
                return true;
            }

            linear->count_added_in_this_iteration_step += iter->count;
        }
        while (true);
    }

    return false;
}


void hdr_iter_linear_init(struct hdr_iter* iter, const struct hdr_histogram* h, int64_t value_units_per_bucket)
{
    hdr_iter_init(iter, h);

    iter->specifics.linear.count_added_in_this_iteration_step = 0;
    iter->specifics.linear.value_units_per_bucket = value_units_per_bucket;
    if (value_units_per_bucket <= 0)
    {
        /* non-positive step never advances; a negative one also reaches
           negative left-shift UB in lowest_equivalent_value below. Pin to the
           terminating state (matches the advance-path guard). */
        iter->specifics.linear.next_value_reporting_level = INT64_MAX;
        iter->specifics.linear.next_value_reporting_level_lowest_equivalent = INT64_MAX;
    }
    else
    {
        iter->specifics.linear.next_value_reporting_level = value_units_per_bucket;
        iter->specifics.linear.next_value_reporting_level_lowest_equivalent = lowest_equivalent_value(h, value_units_per_bucket);
    }

    iter->_next_fp = iter_linear_next;
}

void hdr_iter_linear_set_value_units_per_bucket(struct hdr_iter* iter, int64_t value_units_per_bucket)
{
    /* specifics is a union: writing it on any other iterator would corrupt it */
    if (iter->_next_fp == iter_linear_next)
    {
        iter->specifics.linear.value_units_per_bucket = value_units_per_bucket;
    }
}

/* ##        #######   ######      ###    ########  #### ######## ##     ## ##     ## ####  ######  */
/* ##       ##     ## ##    ##    ## ##   ##     ##  ##     ##    ##     ## ###   ###  ##  ##    ## */
/* ##       ##     ## ##         ##   ##  ##     ##  ##     ##    ##     ## #### ####  ##  ##       */
/* ##       ##     ## ##   #### ##     ## ########   ##     ##    ######### ## ### ##  ##  ##       */
/* ##       ##     ## ##    ##  ######### ##   ##    ##     ##    ##     ## ##     ##  ##  ##       */
/* ##       ##     ## ##    ##  ##     ## ##    ##   ##     ##    ##     ## ##     ##  ##  ##    ## */
/* ########  #######   ######   ##     ## ##     ## ####    ##    ##     ## ##     ## ####  ######  */

static bool log_iter_next(struct hdr_iter *iter)
{
    struct hdr_iter_log* logarithmic = &iter->specifics.log;

    logarithmic->count_added_in_this_iteration_step = 0;

    if (has_next(iter) ||
        next_value_greater_than_reporting_level_upper_bound(
            iter, logarithmic->next_value_reporting_level_lowest_equivalent))
    {
        do
        {
            if (iter->value >= logarithmic->next_value_reporting_level_lowest_equivalent)
            {
                update_iterated_values(iter, logarithmic->next_value_reporting_level);

                /* Emit the saturated final level once before entering the terminal state. */
                {
                    int64_t base = (int64_t) logarithmic->log_base;
                    if (logarithmic->next_value_reporting_level == INT64_MAX)
                    {
                        logarithmic->next_value_reporting_level_lowest_equivalent = INT64_MAX;
                    }
                    else if (base <= 1 || logarithmic->next_value_reporting_level <= 0 || logarithmic->next_value_reporting_level > INT64_MAX / base)
                    {
                        /* base <= 1 first: never-advances (infinite loop) and short-circuits /base so base==0 can't divide-by-zero; level <= 0 never advances (0*=base loops) and *=base on a negative is overflow UB; last clause is the positive-overflow guard */
                        logarithmic->next_value_reporting_level = INT64_MAX;
                        logarithmic->next_value_reporting_level_lowest_equivalent =
                            lowest_equivalent_value(iter->h, INT64_MAX);
                    }
                    else
                    {
                        logarithmic->next_value_reporting_level *= base;
                        logarithmic->next_value_reporting_level_lowest_equivalent =
                            lowest_equivalent_value(iter->h, logarithmic->next_value_reporting_level);
                    }
                }

                return true;
            }

            if (!move_next(iter))
            {
                return true;
            }

            logarithmic->count_added_in_this_iteration_step += iter->count;
        }
        while (true);
    }

    return false;
}

void hdr_iter_log_init(
        struct hdr_iter* iter,
        const struct hdr_histogram* h,
        int64_t value_units_first_bucket,
        double log_base)
{
    hdr_iter_init(iter, h);
    iter->specifics.log.count_added_in_this_iteration_step = 0;
    iter->specifics.log.log_base = log_base;
    iter->specifics.log.next_value_reporting_level = value_units_first_bucket;
    if (value_units_first_bucket <= 0 || !isfinite(log_base) || log_base <= 1.0 ||
        log_base >= (double) INT64_MAX)
    {
        /* non-positive first bucket or base <= 1 never advances; a negative
           first bucket also reaches negative left-shift UB in
           lowest_equivalent_value below. A non-finite (NaN/Inf) or out-of-int64-
           range base would hit float-cast-overflow UB at the (int64_t) log_base
           cast in log_iter_next. Pin to the terminating state (matches the
           advance-path guard) so that cast is never reached for a bad base. */
        iter->specifics.log.next_value_reporting_level = INT64_MAX;
        iter->specifics.log.next_value_reporting_level_lowest_equivalent = INT64_MAX;
    }
    else
    {
        iter->specifics.log.next_value_reporting_level_lowest_equivalent = lowest_equivalent_value(h, value_units_first_bucket);
    }

    iter->_next_fp = log_iter_next;
}

/* Printing. */

static const char* format_head_string(format_type format)
{
    switch (format)
    {
        case CSV:
            return "%s,%s,%s,%s\n";
        case CLASSIC:
        default:
            return "%12s %12s %12s %12s\n\n";
    }
}

static const char CLASSIC_FOOTER[] =
    "#[Mean    = %12.3f, StdDeviation   = %12.3f]\n"
    "#[Max     = %12.3f, Total count    = %12" PRIu64 "]\n"
    "#[Buckets = %12d, SubBuckets     = %12d]\n";

int hdr_percentiles_print(
        struct hdr_histogram* h, FILE* stream, int32_t ticks_per_half_distance,
        double value_scale, format_type format)
{
    char line_format[25];
    const char* head_format;
    int rc = 0;
    struct hdr_iter iter;
    struct hdr_iter_percentiles * percentiles;

    format_line_string(line_format, 25, h->significant_figures, format);
    head_format = format_head_string(format);

    hdr_iter_percentile_init(&iter, h, ticks_per_half_distance);

    if (fprintf(
            stream, head_format,
            "Value", "Percentile", "TotalCount", "1/(1-Percentile)") < 0)
    {
        rc = EIO;
        goto cleanup;
    }

    percentiles = &iter.specifics.percentiles;
    while (hdr_iter_next(&iter))
    {
        double  value               = iter.highest_equivalent_value / value_scale;
        double  percentile          = percentiles->percentile / 100.0;
        int64_t total_count         = iter.cumulative_count;
        double  inverted_percentile = (1.0 / (1.0 - percentile));

        if (fprintf(
                stream, line_format, value, percentile, total_count, inverted_percentile) < 0)
        {
            rc = EIO;
            goto cleanup;
        }
    }

    if (CLASSIC == format)
    {
        double mean   = hdr_mean(h)   / value_scale;
        double stddev = hdr_stddev(h) / value_scale;
        double max    = hdr_max(h)    / value_scale;

        if (fprintf(
                stream, CLASSIC_FOOTER,  mean, stddev, max,
                h->total_count, h->bucket_count, h->sub_bucket_count) < 0)
        {
            rc = EIO;
            goto cleanup;
        }
    }

    cleanup:
    return rc;
}
