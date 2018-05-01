/* transforms is a part of ABI, but not API.

   It means that there are some functions that are supposed to be in "common"
   library, but header itself is not placed into include/brotli. This way,
   aforementioned functions will be available only to brotli internals.
 */

#ifndef BROTLI_COMMON_TRANSFORM_H_
#define BROTLI_COMMON_TRANSFORM_H_

#include <brotli/port.h>
#include <brotli/types.h>

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

enum BrotliWordTransformType {
  BROTLI_TRANSFORM_IDENTITY = 0,
  BROTLI_TRANSFORM_OMIT_LAST_1 = 1,
  BROTLI_TRANSFORM_OMIT_LAST_2 = 2,
  BROTLI_TRANSFORM_OMIT_LAST_3 = 3,
  BROTLI_TRANSFORM_OMIT_LAST_4 = 4,
  BROTLI_TRANSFORM_OMIT_LAST_5 = 5,
  BROTLI_TRANSFORM_OMIT_LAST_6 = 6,
  BROTLI_TRANSFORM_OMIT_LAST_7 = 7,
  BROTLI_TRANSFORM_OMIT_LAST_8 = 8,
  BROTLI_TRANSFORM_OMIT_LAST_9 = 9,
  BROTLI_TRANSFORM_UPPERCASE_FIRST = 10,
  BROTLI_TRANSFORM_UPPERCASE_ALL = 11,
  BROTLI_TRANSFORM_OMIT_FIRST_1 = 12,
  BROTLI_TRANSFORM_OMIT_FIRST_2 = 13,
  BROTLI_TRANSFORM_OMIT_FIRST_3 = 14,
  BROTLI_TRANSFORM_OMIT_FIRST_4 = 15,
  BROTLI_TRANSFORM_OMIT_FIRST_5 = 16,
  BROTLI_TRANSFORM_OMIT_FIRST_6 = 17,
  BROTLI_TRANSFORM_OMIT_FIRST_7 = 18,
  BROTLI_TRANSFORM_OMIT_FIRST_8 = 19,
  BROTLI_TRANSFORM_OMIT_FIRST_9 = 20,
  BROTLI_NUM_TRANSFORM_TYPES  /* Counts transforms, not a transform itself. */
};

#define BROTLI_TRANSFORMS_MAX_CUT_OFF BROTLI_TRANSFORM_OMIT_LAST_9

typedef struct BrotliTransforms {
  uint16_t prefix_suffix_size;
  /* Last character must be null, so prefix_suffix_size must be at least 1. */
  const uint8_t* prefix_suffix;
  const uint16_t* prefix_suffix_map;
  uint32_t num_transforms;
  /* Each entry is a [prefix_id, transform, suffix_id] triplet. */
  const uint8_t* transforms;
  /* Indices of transforms like ["", BROTLI_TRANSFORM_OMIT_LAST_#, ""].
     0-th element corresponds to ["", BROTLI_TRANSFORM_IDENTITY, ""].
     -1, if cut-off transform does not exist. */
  int16_t cutOffTransforms[BROTLI_TRANSFORMS_MAX_CUT_OFF + 1];
} BrotliTransforms;

/* T is BrotliTransforms*; result is uint8_t. */
#define BROTLI_TRANSFORM_PREFIX_ID(T, I) ((T)->transforms[((I) * 3) + 0])
#define BROTLI_TRANSFORM_TYPE(T, I)      ((T)->transforms[((I) * 3) + 1])
#define BROTLI_TRANSFORM_SUFFIX_ID(T, I) ((T)->transforms[((I) * 3) + 2])

/* T is BrotliTransforms*; result is const uint8_t*. */
#define BROTLI_TRANSFORM_PREFIX(T, I) (&(T)->prefix_suffix[ \
    (T)->prefix_suffix_map[BROTLI_TRANSFORM_PREFIX_ID(T, I)]])
#define BROTLI_TRANSFORM_SUFFIX(T, I) (&(T)->prefix_suffix[ \
    (T)->prefix_suffix_map[BROTLI_TRANSFORM_SUFFIX_ID(T, I)]])

BROTLI_COMMON_API const BrotliTransforms* BrotliGetTransforms(void);

BROTLI_COMMON_API int BrotliTransformDictionaryWord(
    uint8_t* dst, const uint8_t* word, int len,
    const BrotliTransforms* transforms, int transform_idx);

#if defined(__cplusplus) || defined(c_plusplus)
}  /* extern "C" */
#endif

#endif  /* BROTLI_COMMON_TRANSFORM_H_ */
