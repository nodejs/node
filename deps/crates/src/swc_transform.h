/* C/C++ interface to the SWC TypeScript-to-JavaScript transform.
 *
 * This header is consumed by Node.js build tools (js2c) to transpile
 * TypeScript source files at build time using the same SWC version that
 * ships inside deps/amaro.
 */

#ifndef DEPS_CRATES_SRC_SWC_TRANSFORM_H_
#define DEPS_CRATES_SRC_SWC_TRANSFORM_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Transform modes – keep in sync with swc_ts_fast_strip::Mode */
#define SWC_MODE_STRIP_ONLY 0 /* Erase type annotations only           */
#define SWC_MODE_TRANSFORM 1  /* Full TypeScript-to-JavaScript lowering */

/* Result of a transform operation.
 * Exactly one of `code` / `error` will be non-NULL.
 * The caller MUST call swc_transform_free_result() to release memory. */
typedef struct {
  char* code;      /* output JS code  (NULL on error)   */
  char* error;     /* error message   (NULL on success)  */
  size_t code_len; /* byte length of `code`  (excl. NUL) */
  size_t error_len; /* byte length of `error` (excl. NUL) */
} SwcTransformResult;

/* Transpile a TypeScript source buffer to JavaScript.
 *
 * source      – pointer to UTF-8 encoded TypeScript source.
 * source_len  – byte length of source.
 * filename    – optional filename for diagnostics (may be NULL).
 * filename_len – byte length of filename (ignored when NULL).
 * mode        – SWC_MODE_STRIP_ONLY or SWC_MODE_TRANSFORM.
 */
SwcTransformResult swc_transform(const char* source,
                                 size_t source_len,
                                 const char* filename,
                                 size_t filename_len,
                                 int mode);

/* Free the buffers inside a SwcTransformResult.
 * Must be called exactly once per result returned by swc_transform(). */
void swc_transform_free_result(SwcTransformResult* result);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* DEPS_CRATES_SRC_SWC_TRANSFORM_H_ */
