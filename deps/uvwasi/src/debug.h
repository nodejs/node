#ifndef __UVWASI_DEBUG_H__
#define __UVWASI_DEBUG_H__

#ifdef UVWASI_DEBUG_LOG
#ifndef __STDC_FORMAT_MACROS
# define __STDC_FORMAT_MACROS
#endif
# include <inttypes.h>
# define UVWASI_DEBUG(...)                                                    \
    do { fprintf(stderr, __VA_ARGS__); } while (0)
#else
# define UVWASI_DEBUG(fmt, ...)
#endif

#endif /* __UVWASI_DEBUG_H__ */
