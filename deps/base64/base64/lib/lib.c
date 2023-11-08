#include <stdint.h>
#include <stddef.h>
#ifdef _OPENMP
#include <omp.h>
#endif

#include "../include/libbase64.h"
#include "tables/tables.h"
#include "codecs.h"
#include "env.h"

// These static function pointers are initialized once when the library is
// first used, and remain in use for the remaining lifetime of the program.
// The idea being that CPU features don't change at runtime.
static struct codec codec = { NULL, NULL };

void
base64_stream_encode_init (struct base64_state *state, int flags)
{
	// If any of the codec flags are set, redo choice:
	if (codec.enc == NULL || flags & 0xFF) {
		codec_choose(&codec, flags);
	}
	state->eof = 0;
	state->bytes = 0;
	state->carry = 0;
	state->flags = flags;
}

void
base64_stream_encode
	( struct base64_state	*state
	, const char		*src
	, size_t		 srclen
	, char			*out
	, size_t		*outlen
	)
{
	codec.enc(state, src, srclen, out, outlen);
}

void
base64_stream_encode_final
	( struct base64_state	*state
	, char			*out
	, size_t		*outlen
	)
{
	uint8_t *o = (uint8_t *)out;

	if (state->bytes == 1) {
		*o++ = base64_table_enc_6bit[state->carry];
		*o++ = '=';
		*o++ = '=';
		*outlen = 3;
		return;
	}
	if (state->bytes == 2) {
		*o++ = base64_table_enc_6bit[state->carry];
		*o++ = '=';
		*outlen = 2;
		return;
	}
	*outlen = 0;
}

void
base64_stream_decode_init (struct base64_state *state, int flags)
{
	// If any of the codec flags are set, redo choice:
	if (codec.dec == NULL || flags & 0xFFFF) {
		codec_choose(&codec, flags);
	}
	state->eof = 0;
	state->bytes = 0;
	state->carry = 0;
	state->flags = flags;
}

int
base64_stream_decode
	( struct base64_state	*state
	, const char		*src
	, size_t		 srclen
	, char			*out
	, size_t		*outlen
	)
{
	return codec.dec(state, src, srclen, out, outlen);
}

#ifdef _OPENMP

	// Due to the overhead of initializing OpenMP and creating a team of
	// threads, we require the data length to be larger than a threshold:
	#define OMP_THRESHOLD 20000

	// Conditionally include OpenMP-accelerated codec implementations:
	#include "lib_openmp.c"
#endif

void
base64_encode
	( const char	*src
	, size_t	 srclen
	, char		*out
	, size_t	*outlen
	, int		 flags
	)
{
	size_t s;
	size_t t;
	struct base64_state state;

	#ifdef _OPENMP
	if (srclen >= OMP_THRESHOLD) {
		base64_encode_openmp(src, srclen, out, outlen, flags);
		return;
	}
	#endif

	// Init the stream reader:
	base64_stream_encode_init(&state, flags);

	// Feed the whole string to the stream reader:
	base64_stream_encode(&state, src, srclen, out, &s);

	// Finalize the stream by writing trailer if any:
	base64_stream_encode_final(&state, out + s, &t);

	// Final output length is stream length plus tail:
	*outlen = s + t;
}

int
base64_decode
	( const char	*src
	, size_t	 srclen
	, char		*out
	, size_t	*outlen
	, int		 flags
	)
{
	int ret;
	struct base64_state state;

	#ifdef _OPENMP
	if (srclen >= OMP_THRESHOLD) {
		return base64_decode_openmp(src, srclen, out, outlen, flags);
	}
	#endif

	// Init the stream reader:
	base64_stream_decode_init(&state, flags);

	// Feed the whole string to the stream reader:
	ret = base64_stream_decode(&state, src, srclen, out, outlen);

	// If when decoding a whole block, we're still waiting for input then fail:
	if (ret && (state.bytes == 0)) {
		return ret;
	}
	return 0;
}
