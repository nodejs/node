// For clock_gettime(2):
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif

// For CLOCK_REALTIME on FreeBSD:
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE   600
#endif

#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#ifdef __MACH__
#include <mach/mach_time.h>
#endif

#include "../include/libbase64.h"
#include "codec_supported.h"

#define KB	1024
#define MB	(1024 * KB)

#define RANDOMDEV  "/dev/urandom"

struct buffers {
	char *reg;
	char *enc;
	size_t regsz;
	size_t encsz;
};

// Define buffer sizes to test with:
static struct bufsize {
	char	*label;
	size_t	 len;
	int	 repeat;
	int	 batch;
}
sizes[] = {
	{ "10 MB",	MB * 10,	10,	1	},
	{ "1 MB",	MB * 1,		10,	10	},
	{ "100 KB",	KB * 100,	10,	100	},
	{ "10 KB",	KB * 10,	100,	100	},
	{ "1 KB",	KB * 1,		100,	1000	},
};

static inline float
bytes_to_mb (size_t bytes)
{
	return bytes / (float) MB;
}

static bool
get_random_data (struct buffers *b, char **errmsg)
{
	int fd;
	ssize_t nread;
	size_t total_read = 0;

	// Open random device for semi-random data:
	if ((fd = open(RANDOMDEV, O_RDONLY)) < 0) {
		*errmsg = "Cannot open " RANDOMDEV;
		return false;
	}

	printf("Filling buffer with %.1f MB of random data...\n", bytes_to_mb(b->regsz));

	while (total_read < b->regsz) {
		if ((nread = read(fd, b->reg + total_read, b->regsz - total_read)) < 0) {
			*errmsg = "Read error";
			close(fd);
			return false;
		}
		total_read += nread;
	}
	close(fd);
	return true;
}

#ifdef __MACH__
typedef uint64_t base64_timespec;
static void
base64_gettime (base64_timespec * o_time)
{
	*o_time = mach_absolute_time();
}

static float
timediff_sec (base64_timespec *start, base64_timespec *end)
{
	uint64_t diff = *end - *start;
	mach_timebase_info_data_t tb = { 0, 0 };
	mach_timebase_info(&tb);

	return (float)((diff * tb.numer) / tb.denom) / 1e9f;
}
#else
typedef struct timespec base64_timespec;
static void
base64_gettime (base64_timespec * o_time)
{
	clock_gettime(CLOCK_REALTIME, o_time);
}

static float
timediff_sec (base64_timespec *start, base64_timespec *end)
{
	return (end->tv_sec - start->tv_sec) + ((float)(end->tv_nsec - start->tv_nsec)) / 1e9f;
}
#endif

static void
codec_bench_enc (struct buffers *b, const struct bufsize *bs, const char *name, unsigned int flags)
{
	float timediff, fastest = -1.0f;
	base64_timespec start, end;

	// Reset buffer size:
	b->regsz = bs->len;

	// Repeat benchmark a number of times for a fair test:
	for (int i = bs->repeat; i; i--) {

		// Timing loop, use batches to increase timer resolution:
		base64_gettime(&start);
		for (int j = bs->batch; j; j--)
			base64_encode(b->reg, b->regsz, b->enc, &b->encsz, flags);
		base64_gettime(&end);

		// Calculate average time of batch:
		timediff = timediff_sec(&start, &end) / bs->batch;

		// Update fastest time seen:
		if (fastest < 0.0f || timediff < fastest)
			fastest = timediff;
	}

	printf("%s\tencode\t%.02f MB/sec\n", name, bytes_to_mb(b->regsz) / fastest);
}

static void
codec_bench_dec (struct buffers *b, const struct bufsize *bs, const char *name, unsigned int flags)
{
	float timediff, fastest = -1.0f;
	base64_timespec start, end;

	// Reset buffer size:
	b->encsz = bs->len;

	// Repeat benchmark a number of times for a fair test:
	for (int i = bs->repeat; i; i--) {

		// Timing loop, use batches to increase timer resolution:
		base64_gettime(&start);
		for (int j = bs->batch; j; j--)
			base64_decode(b->enc, b->encsz, b->reg, &b->regsz, flags);
		base64_gettime(&end);

		// Calculate average time of batch:
		timediff = timediff_sec(&start, &end) / bs->batch;

		// Update fastest time seen:
		if (fastest < 0.0f || timediff < fastest)
			fastest = timediff;
	}

	printf("%s\tdecode\t%.02f MB/sec\n", name, bytes_to_mb(b->encsz) / fastest);
}

static void
codec_bench (struct buffers *b, const struct bufsize *bs, const char *name, unsigned int flags)
{
	codec_bench_enc(b, bs, name, flags);
	codec_bench_dec(b, bs, name, flags);
}

int
main ()
{
	int ret = 0;
	char *errmsg = NULL;
	struct buffers b;

	// Set buffer sizes to largest buffer length:
	b.regsz = sizes[0].len;
	b.encsz = sizes[0].len * 5 / 3;

	// Allocate space for megabytes of random data:
	if ((b.reg = malloc(b.regsz)) == NULL) {
		errmsg = "Out of memory";
		ret = 1;
		goto err0;
	}

	// Allocate space for encoded output:
	if ((b.enc = malloc(b.encsz)) == NULL) {
		errmsg = "Out of memory";
		ret = 1;
		goto err1;
	}

	// Fill buffer with random data:
	if (get_random_data(&b, &errmsg) == false) {
		ret = 1;
		goto err2;
	}

	// Loop over all buffer sizes:
	for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
		printf("Testing with buffer size %s, fastest of %d * %d\n",
			sizes[i].label, sizes[i].repeat, sizes[i].batch);

		// Loop over all codecs:
		for (size_t j = 0; codecs[j]; j++)
			if (codec_supported(1 << j))
				codec_bench(&b, &sizes[i], codecs[j], 1 << j);
	};

	// Free memory:
err2:	free(b.enc);
err1:	free(b.reg);
err0:	if (errmsg)
		fputs(errmsg, stderr);

	return ret;
}
