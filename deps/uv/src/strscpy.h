#ifndef UV_STRSCPY_H_
#define UV_STRSCPY_H_

/* Include uv.h for its definitions of size_t and ssize_t.
 * size_t can be obtained directly from <stddef.h> but ssize_t requires
 * some hoop jumping on Windows that I didn't want to duplicate here.
 */
#include "uv.h"

/* Copies up to |n-1| bytes from |d| to |s| and always zero-terminates
 * the result, except when |n==0|. Returns the number of bytes copied
 * or UV_E2BIG if |d| is too small.
 *
 * See https://www.kernel.org/doc/htmldocs/kernel-api/API-strscpy.html
 */
ssize_t uv__strscpy(char* d, const char* s, size_t n);

#endif  /* UV_STRSCPY_H_ */
