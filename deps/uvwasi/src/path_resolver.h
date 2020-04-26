#ifndef __UVWASI_PATH_RESOLVER_H__
#define __UVWASI_PATH_RESOLVER_H__

#include "uvwasi.h"

/* TODO(cjihrig): PATH_MAX_BYTES shouldn't be stack allocated. On Windows, paths
   can be 32k long, and this PATH_MAX_BYTES is an artificial limitation. */
#ifdef _WIN32
/* MAX_PATH is in characters, not bytes. Make sure we have enough headroom. */
# define PATH_MAX_BYTES (MAX_PATH * 4)
#else
# include <limits.h>
# define PATH_MAX_BYTES (PATH_MAX)
#endif

uvwasi_errno_t uvwasi__normalize_path(const char* path,
                                      size_t path_len,
                                      char* normalized_path,
                                      size_t normalized_len);

uvwasi_errno_t uvwasi__resolve_path(const uvwasi_t* uvwasi,
                                    const struct uvwasi_fd_wrap_t* fd,
                                    const char* path,
                                    size_t path_len,
                                    char* resolved_path,
                                    uvwasi_lookupflags_t flags);

#endif /* __UVWASI_PATH_RESOLVER_H__ */
