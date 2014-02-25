/*
** Character types.
** Donated to the public domain.
*/

#ifndef _LJ_CHAR_H
#define _LJ_CHAR_H

#include "lj_def.h"

#define LJ_CHAR_CNTRL	0x01
#define LJ_CHAR_SPACE	0x02
#define LJ_CHAR_PUNCT	0x04
#define LJ_CHAR_DIGIT	0x08
#define LJ_CHAR_XDIGIT	0x10
#define LJ_CHAR_UPPER	0x20
#define LJ_CHAR_LOWER	0x40
#define LJ_CHAR_IDENT	0x80
#define LJ_CHAR_ALPHA	(LJ_CHAR_LOWER|LJ_CHAR_UPPER)
#define LJ_CHAR_ALNUM	(LJ_CHAR_ALPHA|LJ_CHAR_DIGIT)
#define LJ_CHAR_GRAPH	(LJ_CHAR_ALNUM|LJ_CHAR_PUNCT)

/* Only pass -1 or 0..255 to these macros. Never pass a signed char! */
#define lj_char_isa(c, t)	((lj_char_bits+1)[(c)] & t)
#define lj_char_iscntrl(c)	lj_char_isa((c), LJ_CHAR_CNTRL)
#define lj_char_isspace(c)	lj_char_isa((c), LJ_CHAR_SPACE)
#define lj_char_ispunct(c)	lj_char_isa((c), LJ_CHAR_PUNCT)
#define lj_char_isdigit(c)	lj_char_isa((c), LJ_CHAR_DIGIT)
#define lj_char_isxdigit(c)	lj_char_isa((c), LJ_CHAR_XDIGIT)
#define lj_char_isupper(c)	lj_char_isa((c), LJ_CHAR_UPPER)
#define lj_char_islower(c)	lj_char_isa((c), LJ_CHAR_LOWER)
#define lj_char_isident(c)	lj_char_isa((c), LJ_CHAR_IDENT)
#define lj_char_isalpha(c)	lj_char_isa((c), LJ_CHAR_ALPHA)
#define lj_char_isalnum(c)	lj_char_isa((c), LJ_CHAR_ALNUM)
#define lj_char_isgraph(c)	lj_char_isa((c), LJ_CHAR_GRAPH)

#define lj_char_toupper(c)	((c) - (lj_char_islower(c) >> 1))
#define lj_char_tolower(c)	((c) + lj_char_isupper(c))

LJ_DATA const uint8_t lj_char_bits[257];

#endif
