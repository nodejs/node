#ifndef __UVWASI_PATH_RESOLVER_H__
#define __UVWASI_PATH_RESOLVER_H__

#include "fd_table.h"
#include "uvwasi.h"

uvwasi_errno_t uvwasi__normalize_path(const char* path,
                                      uvwasi_size_t path_len,
                                      char* normalized_path,
                                      uvwasi_size_t normalized_len);

uvwasi_errno_t uvwasi__resolve_path(const uvwasi_t* uvwasi,
                                    const struct uvwasi_fd_wrap_t* fd,
                                    const char* path,
                                    uvwasi_size_t path_len,
                                    char** resolved_path,
                                    uvwasi_lookupflags_t flags);

#endif /* __UVWASI_PATH_RESOLVER_H__ */
