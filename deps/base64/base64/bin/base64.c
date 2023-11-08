#define _XOPEN_SOURCE		// IOV_MAX

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <limits.h>
#include <sys/uio.h>
#include "../include/libbase64.h"

// Size of the buffer for the "raw" (not base64-encoded) data in bytes.
#define BUFFER_RAW_SIZE (1024 * 1024)

// Size of the buffer for the base64-encoded data in bytes. The base64-encoded
// data is 4/3 the size of the input, with some margin to be sure.
#define BUFFER_ENC_SIZE (BUFFER_RAW_SIZE * 4 / 3 + 16)

// Global config structure.
struct config {

	// Name by which the program was called on the command line.
	const char *name;

	// Name of the input file for logging purposes.
	const char *file;

	// Input file handle.
	FILE *fp;

	// Wrap width in characters, for encoding only.
	size_t wrap;

	// Whether to run in decode mode.
	bool decode;

	// Whether to just print the help text and exit.
	bool print_help;
};

// Input/output buffer structure.
struct buffer {

	// Runtime-allocated buffer for raw (unencoded) data.
	char *raw;

	// Runtime-allocated buffer for base64-encoded data.
	char *enc;
};

static bool
buffer_alloc (const struct config *config, struct buffer *buf)
{
	if ((buf->raw = malloc(BUFFER_RAW_SIZE)) == NULL ||
	    (buf->enc = malloc(BUFFER_ENC_SIZE)) == NULL) {
		free(buf->raw);
		fprintf(stderr, "%s: malloc: %s\n",
		        config->name, strerror(errno));
		return false;
	}

	return true;
}

static void
buffer_free (struct buffer *buf)
{
	free(buf->raw);
	free(buf->enc);
}

static bool
writev_retry (const struct config *config, struct iovec *iov, size_t nvec)
{
	// Writing nothing always succeeds.
	if (nvec == 0) {
		return true;
	}

	while (true) {
		ssize_t nwrite;

		// Try to write the vectors to stdout.
		if ((nwrite = writev(1, iov, nvec)) < 0) {

			// Retry on EINTR.
			if (errno == EINTR) {
				continue;
			}

			// Quit on other errors.
			fprintf(stderr, "%s: writev: %s\n",
				config->name, strerror(errno));
			return false;
		}

		// The return value of `writev' is the number of bytes written.
		// To check for success, we traverse the list and remove all
		// written vectors. The call succeeded if the list is empty.
		while (true) {

			// Retry if this vector is not or partially written.
			if (iov->iov_len > (size_t) nwrite) {
				char *base = iov->iov_base;

				iov->iov_base = (size_t) nwrite + base;
				iov->iov_len -= (size_t) nwrite;
				break;
			}

			// Move to the next vector.
			nwrite -= iov->iov_len;
			iov++;

			// Return successfully if all vectors were written.
			if (--nvec == 0) {
				return true;
			}
		}
	}
}

static inline bool
iov_append (const struct config *config, struct iovec *iov,
            size_t *nvec, char *base, const size_t len)
{
	// Add the buffer to the IO vector array.
	iov[*nvec].iov_base = base;
	iov[*nvec].iov_len  = len;

	// Increment the array index. Flush the array if it is full.
	if (++(*nvec) == IOV_MAX) {
		if (writev_retry(config, iov, IOV_MAX) == false) {
			return false;
		}
		*nvec = 0;
	}

	return true;
}

static bool
write_stdout (const struct config *config, const char *buf, size_t len)
{
	while (len > 0) {
		ssize_t nwrite;

		// Try to write the buffer to stdout.
		if ((nwrite = write(1, buf, len)) < 0) {

			// Retry on EINTR.
			if (errno == EINTR) {
				continue;
			}

			// Quit on other errors.
			fprintf(stderr, "%s: write: %s\n",
			        config->name, strerror(errno));
			return false;
		}

		// Update the buffer position.
		buf += (size_t) nwrite;
		len -= (size_t) nwrite;
	}

	return true;
}

static bool
write_wrapped (const struct config *config, char *buf, size_t len)
{
	static size_t col = 0;

	// Special case: if buf is NULL, print final trailing newline.
	if (buf == NULL) {
		if (config->wrap > 0 && col > 0) {
			return write_stdout(config, "\n", 1);
		}
		return true;
	}

	// If no wrap width is given, write the entire buffer.
	if (config->wrap == 0) {
		return write_stdout(config, buf, len);
	}

	// Statically allocated IO vector buffer.
	static struct iovec iov[IOV_MAX];
	size_t nvec = 0;

	while (len > 0) {

		// Number of characters to fill the current line.
		size_t nwrite = config->wrap - col;

		// Do not write more data than is available.
		if (nwrite > len) {
			nwrite = len;
		}

		// Append the data to the IO vector array.
		if (iov_append(config, iov, &nvec, buf, nwrite) == false) {
			return false;
		}

		// Advance the buffer.
		len -= nwrite;
		buf += nwrite;
		col += nwrite;

		// If the line is full, append a newline.
		if (col == config->wrap) {
			if (iov_append(config, iov, &nvec, "\n", 1) == false) {
				return false;
			}
			col = 0;
		}
	}

	// Write the remaining vectors.
	if (writev_retry(config, iov, nvec) == false) {
		return false;
	}

	return true;
}

static bool
encode (const struct config *config, struct buffer *buf)
{
	size_t nread, nout;
	struct base64_state state;

	// Initialize the encoder's state structure.
	base64_stream_encode_init(&state, 0);

	// Read raw data into the buffer.
	while ((nread = fread(buf->raw, 1, BUFFER_RAW_SIZE, config->fp)) > 0) {

		// Encode the raw input into the encoded buffer.
		base64_stream_encode(&state, buf->raw, nread, buf->enc, &nout);

		// Append the encoded data to the output stream.
		if (write_wrapped(config, buf->enc, nout) == false) {
			return false;
		}
	}

	// Check for stream errors.
	if (ferror(config->fp)) {
		fprintf(stderr, "%s: %s: read error\n",
		        config->name, config->file);
		return false;
	}

	// Finalize the encoding by adding proper stream terminators.
	base64_stream_encode_final(&state, buf->enc, &nout);

	// Append this tail to the output stream.
	if (write_wrapped(config, buf->enc, nout) == false) {
		return false;
	}

	// Print optional trailing newline.
	if (write_wrapped(config, NULL, 0) == false) {
		return false;
	}

	return true;
}

static int
decode (const struct config *config, struct buffer *buf)
{
	size_t nread, nout;
	struct base64_state state;

	// Initialize the decoder's state structure.
	base64_stream_decode_init(&state, 0);

	// Read encoded data into the buffer. Use the smallest buffer size to
	// be on the safe side: the decoded output will fit the raw buffer.
	while ((nread = fread(buf->enc, 1, BUFFER_RAW_SIZE, config->fp)) > 0) {

		// Decode the input into the raw buffer.
		if (base64_stream_decode(&state, buf->enc, nread,
		                         buf->raw, &nout) == 0) {
			fprintf(stderr, "%s: %s: decoding error\n",
			        config->name, config->file);
			return false;
		}

		// Append the raw data to the output stream.
		if (write_stdout(config, buf->raw, nout) == false) {
			return false;
		}
	}

	// Check for stream errors.
	if (ferror(config->fp)) {
		fprintf(stderr, "%s: %s: read error\n",
		       config->name, config->file);
		return false;
	}

	return true;
}

static void
usage (FILE *fp, const struct config *config)
{
	const char *usage =
		"Usage: %s [OPTION]... [FILE]\n"
		"If no FILE is given or is specified as '-', "
		"read from standard input.\n"
		"Options:\n"
		"  -d, --decode     Decode a base64 stream.\n"
		"  -h, --help       Print this help text.\n"
		"  -w, --wrap=COLS  Wrap encoded lines at this column. "
		"Default 76, 0 to disable.\n";

	fprintf(fp, usage, config->name);
}

static bool
get_wrap (struct config *config, const char *str)
{
	char *eptr;

	// Reject empty strings.
	if (*str == '\0') {
		return false;
	}

	// Convert the input string to a signed long.
	const long wrap = strtol(str, &eptr, 10);

	// Reject negative numbers.
	if (wrap < 0) {
		return false;
	}

	// Reject strings containing non-digits.
	if (*eptr != '\0') {
		return false;
	}

	config->wrap = (size_t) wrap;
	return true;
}

static bool
parse_opts (int argc, char **argv, struct config *config)
{
	int c;
	static const struct option opts[] = {
		{ "decode", no_argument,       NULL, 'd' },
		{ "help",   no_argument,       NULL, 'h' },
		{ "wrap",   required_argument, NULL, 'w' },
		{ NULL }
	};

	// Remember the program's name.
	config->name = *argv;

	// Parse command line options.
	while ((c = getopt_long(argc, argv, ":dhw:", opts, NULL)) != -1) {
		switch (c) {
		case 'd':
			config->decode = true;
			break;

		case 'h':
			config->print_help = true;
			return true;

		case 'w':
			if (get_wrap(config, optarg) == false) {
				fprintf(stderr,
				        "%s: invalid wrap value '%s'\n",
				        config->name, optarg);
				return false;
			}
			break;

		case ':':
			fprintf(stderr, "%s: missing argument for '%c'\n",
			        config->name, optopt);
			return false;

		default:
			fprintf(stderr, "%s: unknown option '%c'\n",
			        config->name, optopt);
			return false;
		}
	}

	// Return successfully if no filename was given.
	if (optind >= argc) {
		return true;
	}

	// Return unsuccessfully if more than one filename was given.
	if (optind + 1 < argc) {
		fprintf(stderr, "%s: too many files\n", config->name);
		return false;
	}

	// For compatibility with GNU Coreutils base64, treat a filename of '-'
	// as standard input.
	if (strcmp(argv[optind], "-") == 0) {
		return true;
	}

	// Save the name of the file.
	config->file = argv[optind];

	// Open the file.
	if ((config->fp = fopen(config->file, "rb")) == NULL) {
		fprintf(stderr, "%s: %s: %s\n",
		        config->name, config->file, strerror(errno));
		return false;
	}

	return true;
}

int
main (int argc, char **argv)
{
	// Default program config.
	struct config config = {
		.file       = "stdin",
		.fp         = stdin,
		.wrap       = 76,
		.decode     = false,
		.print_help = false,
	};
	struct buffer buf;

	// Parse options from the command line.
	if (parse_opts(argc, argv, &config) == false) {
		usage(stderr, &config);
		return 1;
	}

	// Return early if the user just wanted the help text.
	if (config.print_help) {
		usage(stdout, &config);
		return 0;
	}

	// Allocate buffers.
	if (buffer_alloc(&config, &buf) == false) {
		return 1;
	}

	// Encode or decode the input based on the user's choice.
	const bool ret = config.decode
		? decode(&config, &buf)
		: encode(&config, &buf);

	// Free the buffers.
	buffer_free(&buf);

	// Close the input file.
	fclose(config.fp);

	// Close the output stream.
	fclose(stdout);

	// That's all, folks.
	return ret ? 0 : 1;
}
