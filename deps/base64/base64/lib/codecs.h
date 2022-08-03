#include <stdint.h>
#include <stddef.h>

#include "../include/libbase64.h"
#include "config.h"

// Function parameters for encoding functions:
#define BASE64_ENC_PARAMS			\
	( struct base64_state	*state		\
	, const char		*src		\
	, size_t		 srclen		\
	, char			*out		\
	, size_t		*outlen		\
	)

// Function parameters for decoding functions:
#define BASE64_DEC_PARAMS			\
	( struct base64_state	*state		\
	, const char		*src		\
	, size_t		 srclen		\
	, char			*out		\
	, size_t		*outlen		\
	)

// Function signature for encoding functions:
#define BASE64_ENC_FUNCTION(arch)		\
	void					\
	base64_stream_encode_ ## arch		\
	BASE64_ENC_PARAMS

// Function signature for decoding functions:
#define BASE64_DEC_FUNCTION(arch)		\
	int					\
	base64_stream_decode_ ## arch		\
	BASE64_DEC_PARAMS

// Cast away unused variable, silence compiler:
#define UNUSED(x)		((void)(x))

// Stub function when encoder arch unsupported:
#define BASE64_ENC_STUB				\
	UNUSED(state);				\
	UNUSED(src);				\
	UNUSED(srclen);				\
	UNUSED(out);				\
						\
	*outlen = 0;

// Stub function when decoder arch unsupported:
#define BASE64_DEC_STUB				\
	UNUSED(state);				\
	UNUSED(src);				\
	UNUSED(srclen);				\
	UNUSED(out);				\
	UNUSED(outlen);				\
						\
	return -1;

struct codec
{
	void (* enc) BASE64_ENC_PARAMS;
	int  (* dec) BASE64_DEC_PARAMS;
};

extern void codec_choose (struct codec *, int flags);
