#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "../include/libbase64.h"
#include "codec_supported.h"
#include "moby_dick.h"

static char out[2000];
static size_t outlen;

static bool
assert_enc (int flags, const char *src, const char *dst)
{
	size_t srclen = strlen(src);
	size_t dstlen = strlen(dst);

	base64_encode(src, srclen, out, &outlen, flags);

	if (outlen != dstlen) {
		printf("FAIL: encoding of '%s': length expected %lu, got %lu\n", src,
			(unsigned long)dstlen,
			(unsigned long)outlen
		);
		return true;
	}
	if (strncmp(dst, out, outlen) != 0) {
		out[outlen] = '\0';
		printf("FAIL: encoding of '%s': expected output '%s', got '%s'\n", src, dst, out);
		return true;
	}
	return false;
}

static bool
assert_dec (int flags, const char *src, const char *dst)
{
	size_t srclen = strlen(src);
	size_t dstlen = strlen(dst);

	if (!base64_decode(src, srclen, out, &outlen, flags)) {
		printf("FAIL: decoding of '%s': decoding error\n", src);
		return true;
	}
	if (outlen != dstlen) {
		printf("FAIL: encoding of '%s': "
			"length expected %lu, got %lu\n", src,
			(unsigned long)dstlen,
			(unsigned long)outlen
		);
		return true;
	}
	if (strncmp(dst, out, outlen) != 0) {
		out[outlen] = '\0';
		printf("FAIL: decoding of '%s': expected output '%s', got '%s'\n", src, dst, out);
		return true;
	}
	return false;
}

static int
assert_roundtrip (int flags, const char *src)
{
	char tmp[1500];
	size_t tmplen;
	size_t srclen = strlen(src);

	// Encode the input into global buffer:
	base64_encode(src, srclen, out, &outlen, flags);

	// Decode the global buffer into local temp buffer:
	if (!base64_decode(out, outlen, tmp, &tmplen, flags)) {
		printf("FAIL: decoding of '%s': decoding error\n", out);
		return true;
	}

	// Check that 'src' is identical to 'tmp':
	if (srclen != tmplen) {
		printf("FAIL: roundtrip of '%s': "
			"length expected %lu, got %lu\n", src,
			(unsigned long)srclen,
			(unsigned long)tmplen
		);
		return true;
	}
	if (strncmp(src, tmp, tmplen) != 0) {
		tmp[tmplen] = '\0';
		printf("FAIL: roundtrip of '%s': got '%s'\n", src, tmp);
		return true;
	}

	return false;
}

static int
test_char_table (int flags, bool use_malloc)
{
	bool fail = false;
	char chr[256];
	char enc[400], dec[400];
	size_t enclen, declen;

	// Fill array with all characters 0..255:
	for (int i = 0; i < 256; i++)
		chr[i] = (unsigned char)i;

	// Loop, using each char as a starting position to increase test coverage:
	for (int i = 0; i < 256; i++) {

		size_t chrlen = 256 - i;
		char* src = &chr[i];
		if (use_malloc) {
			src = malloc(chrlen); /* malloc/copy this so valgrind can find out-of-bound access */
			if (src == NULL) {
				printf(
					"FAIL: encoding @ %d: allocation of %lu bytes failed\n",
					i, (unsigned long)chrlen
				);
				fail = true;
				continue;
			}
			memcpy(src, &chr[i], chrlen);
		}

		base64_encode(src, chrlen, enc, &enclen, flags);
		if (use_malloc) {
			free(src);
		}

		if (!base64_decode(enc, enclen, dec, &declen, flags)) {
			printf("FAIL: decoding @ %d: decoding error\n", i);
			fail = true;
			continue;
		}
		if (declen != chrlen) {
			printf("FAIL: roundtrip @ %d: "
				"length expected %lu, got %lu\n", i,
				(unsigned long)chrlen,
				(unsigned long)declen
			);
			fail = true;
			continue;
		}
		if (strncmp(&chr[i], dec, declen) != 0) {
			printf("FAIL: roundtrip @ %d: decoded output not same as input\n", i);
			fail = true;
		}
	}

	return fail;
}

static int
test_streaming (int flags)
{
	bool fail = false;
	char chr[256];
	char ref[400], enc[400];
	size_t reflen;
	struct base64_state state;

	// Fill array with all characters 0..255:
	for (int i = 0; i < 256; i++)
		chr[i] = (unsigned char)i;

	// Create reference base64 encoding:
	base64_encode(chr, 256, ref, &reflen, BASE64_FORCE_PLAIN);

	// Encode the table with various block sizes and compare to reference:
	for (size_t bs = 1; bs < 255; bs++)
	{
		size_t inpos   = 0;
		size_t partlen = 0;
		size_t enclen  = 0;

		base64_stream_encode_init(&state, flags);
		memset(enc, 0, 400);
		for (;;) {
			base64_stream_encode(&state, &chr[inpos], (inpos + bs > 256) ? 256 - inpos : bs, &enc[enclen], &partlen);
			enclen += partlen;
			if (inpos + bs > 256) {
				break;
			}
			inpos += bs;
		}
		base64_stream_encode_final(&state, &enc[enclen], &partlen);
		enclen += partlen;

		if (enclen != reflen) {
			printf("FAIL: stream encoding gave incorrect size: "
				"%lu instead of %lu\n",
				(unsigned long)enclen,
				(unsigned long)reflen
			);
			fail = true;
		}
		if (strncmp(ref, enc, reflen) != 0) {
			printf("FAIL: stream encoding with blocksize %lu failed\n",
				(unsigned long)bs
			);
			fail = true;
		}
	}

	// Decode the reference encoding with various block sizes and
	// compare to input char table:
	for (size_t bs = 1; bs < 255; bs++)
	{
		size_t inpos   = 0;
		size_t partlen = 0;
		size_t enclen  = 0;

		base64_stream_decode_init(&state, flags);
		memset(enc, 0, 400);
		while (base64_stream_decode(&state, &ref[inpos], (inpos + bs > reflen) ? reflen - inpos : bs, &enc[enclen], &partlen)) {
			enclen += partlen;
			inpos += bs;

			// Has the entire buffer been consumed?
			if (inpos >= 400) {
				break;
			}
		}
		if (enclen != 256) {
			printf("FAIL: stream decoding gave incorrect size: "
				"%lu instead of 255\n",
				(unsigned long)enclen
			);
			fail = true;
		}
		if (strncmp(chr, enc, 256) != 0) {
			printf("FAIL: stream decoding with blocksize %lu failed\n",
				(unsigned long)bs
			);
			fail = true;
		}
	}

	return fail;
}

static int
test_invalid_dec_input (int flags)
{
	// Subset of invalid characters to cover all ranges
	static const char invalid_set[] = { '\0', -1, '!', '-', ';', '_', '|' };
	static const char* invalid_strings[] = {
		"Zm9vYg=",
		"Zm9vYg",
		"Zm9vY",
		"Zm9vYmF=Zm9v"
	};

	bool fail = false;
	char chr[256];
	char enc[400], dec[400];
	size_t enclen, declen;

	// Fill array with all characters 0..255:
	for (int i = 0; i < 256; i++)
		chr[i] = (unsigned char)i;

	// Create reference base64 encoding:
	base64_encode(chr, 256, enc, &enclen, BASE64_FORCE_PLAIN);

	// Test invalid strings returns error.
	for (size_t i = 0U; i < sizeof(invalid_strings) / sizeof(invalid_strings[0]); ++i) {
		if (base64_decode(invalid_strings[i], strlen(invalid_strings[i]), dec, &declen, flags)) {
			printf("FAIL: decoding invalid input \"%s\": no decoding error\n", invalid_strings[i]);
			fail = true;
		}
	}

	// Loop, corrupting each char to increase test coverage:
	for (size_t c = 0U; c < sizeof(invalid_set); ++c) {
		for (size_t i = 0U; i < enclen; i++) {
			char backup = enc[i];

			enc[i] = invalid_set[c];

			if (base64_decode(enc, enclen, dec, &declen, flags)) {
				printf("FAIL: decoding invalid input @ %d: no decoding error\n", (int)i);
				fail = true;
				enc[i] = backup;
				continue;
			}
			enc[i] = backup;
		}
	}

	// Loop, corrupting two chars to increase test coverage:
	for (size_t c = 0U; c < sizeof(invalid_set); ++c) {
		for (size_t i = 0U; i < enclen - 2U; i++) {
			char backup  = enc[i+0];
			char backup2 = enc[i+2];

			enc[i+0] = invalid_set[c];
			enc[i+2] = invalid_set[c];

			if (base64_decode(enc, enclen, dec, &declen, flags)) {
				printf("FAIL: decoding invalid input @ %d: no decoding error\n", (int)i);
				fail = true;
				enc[i+0] = backup;
				enc[i+2] = backup2;
				continue;
			}
			enc[i+0] = backup;
			enc[i+2] = backup2;
		}
	}

	return fail;
}

static int
test_one_codec (const char *codec, int flags)
{
	bool fail = false;

	printf("Codec %s:\n", codec);

	// Skip if this codec is not supported:
	if (!codec_supported(flags)) {
		puts("  skipping");
		return false;
	}

	// Test vectors:
	struct {
		const char *in;
		const char *out;
	} vec[] = {

		// These are the test vectors from RFC4648:
		{ "",		""         },
		{ "f",		"Zg=="     },
		{ "fo",		"Zm8="     },
		{ "foo",	"Zm9v"     },
		{ "foob",	"Zm9vYg==" },
		{ "fooba",	"Zm9vYmE=" },
		{ "foobar",	"Zm9vYmFy" },

		// The first paragraph from Moby Dick,
		// to test the SIMD codecs with larger blocksize:
		{ moby_dick_plain, moby_dick_base64 },
	};

	for (size_t i = 0; i < sizeof(vec) / sizeof(vec[0]); i++) {

		// Encode plain string, check against output:
		fail |= assert_enc(flags, vec[i].in, vec[i].out);

		// Decode the output string, check if we get the input:
		fail |= assert_dec(flags, vec[i].out, vec[i].in);

		// Do a roundtrip on the inputs and the outputs:
		fail |= assert_roundtrip(flags, vec[i].in);
		fail |= assert_roundtrip(flags, vec[i].out);
	}

	fail |= test_char_table(flags, false); /* test with unaligned input buffer */
	fail |= test_char_table(flags, true); /* test for out-of-bound input read */
	fail |= test_streaming(flags);
	fail |= test_invalid_dec_input(flags);

	if (!fail)
		puts("  all tests passed.");

	return fail;
}

int
main ()
{
	bool fail = false;

	// Loop over all codecs:
	for (size_t i = 0; codecs[i]; i++) {

		// Flags to invoke this codec:
		int codec_flags = (1 << i);

		// Test this codec, merge the results:
		fail |= test_one_codec(codecs[i], codec_flags);
	}

	return (fail) ? 1 : 0;
}
