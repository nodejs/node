#ifndef __UVWASI_DEBUG_H__
#define __UVWASI_DEBUG_H__

#ifdef UVWASI_DEBUG_LOG
# define __STDC_FORMAT_MACROS
# include <inttypes.h>
# define DEBUG(fmt, ...)                                                      \
    do { fprintf(stderr, fmt, __VA_ARGS__); } while (0)
#else
# define DEBUG(fmt, ...)
#endif

#endif /* __UVWASI_DEBUG_H__ */
