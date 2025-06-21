#ifndef __UVWASI_CLOCKS_H__
#define __UVWASI_CLOCKS_H__

#include "wasi_types.h"

uvwasi_errno_t uvwasi__clock_gettime_realtime(uvwasi_timestamp_t* time);
uvwasi_errno_t uvwasi__clock_gettime_process_cputime(uvwasi_timestamp_t* time);
uvwasi_errno_t uvwasi__clock_gettime_thread_cputime(uvwasi_timestamp_t* time);

uvwasi_errno_t uvwasi__clock_getres_process_cputime(uvwasi_timestamp_t* time);
uvwasi_errno_t uvwasi__clock_getres_thread_cputime(uvwasi_timestamp_t* time);

#endif /* __UVWASI_CLOCKS_H__ */
