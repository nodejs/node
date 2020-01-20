#ifndef __UVWASI_UV_MAPPING_H__
#define __UVWASI_UV_MAPPING_H__

#include "uv.h"
#include "wasi_types.h"

#define NANOS_PER_SEC 1000000000

uvwasi_errno_t uvwasi__translate_uv_error(int err);
int uvwasi__translate_to_uv_signal(uvwasi_signal_t sig);
uvwasi_timestamp_t uvwasi__timespec_to_timestamp(const uv_timespec_t* ts);
uvwasi_filetype_t uvwasi__stat_to_filetype(const uv_stat_t* stat);
void uvwasi__stat_to_filestat(const uv_stat_t* stat, uvwasi_filestat_t* fs);
uvwasi_errno_t uvwasi__get_filetype_by_fd(uv_file fd, uvwasi_filetype_t* type);

#endif /* __UVWASI_UV_MAPPING_H__ */
