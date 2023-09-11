#include <stddef.h>	// size_t
#include <stdio.h>	// fopen()
#include <string.h>	// strlen()
#include <getopt.h>
#include "../include/libbase64.h"

#define BUFSIZE 1024 * 1024

static char buf[BUFSIZE];
static char out[(BUFSIZE * 5) / 3];	// Technically 4/3 of input, but take some margin
size_t nread;
size_t nout;

static int
enc (FILE *fp)
{
	int ret = 1;
	struct base64_state state;

	base64_stream_encode_init(&state, 0);

	while ((nread = fread(buf, 1, BUFSIZE, fp)) > 0) {
		base64_stream_encode(&state, buf, nread, out, &nout);
		if (nout) {
			fwrite(out, nout, 1, stdout);
		}
		if (feof(fp)) {
			break;
		}
	}
	if (ferror(fp)) {
		fprintf(stderr, "read error\n");
		ret = 0;
		goto out;
	}
	base64_stream_encode_final(&state, out, &nout);

	if (nout) {
		fwrite(out, nout, 1, stdout);
	}
out:	fclose(fp);
	fclose(stdout);
	return ret;
}

static int
dec (FILE *fp)
{
	int ret = 1;
	struct base64_state state;

	base64_stream_decode_init(&state, 0);

	while ((nread = fread(buf, 1, BUFSIZE, fp)) > 0) {
		if (!base64_stream_decode(&state, buf, nread, out, &nout)) {
			fprintf(stderr, "decoding error\n");
			ret = 0;
			goto out;
		}
		if (nout) {
			fwrite(out, nout, 1, stdout);
		}
		if (feof(fp)) {
			break;
		}
	}
	if (ferror(fp)) {
		fprintf(stderr, "read error\n");
		ret = 0;
	}
out:	fclose(fp);
	fclose(stdout);
	return ret;
}

int
main (int argc, char **argv)
{
	char *file;
	FILE *fp;
	int decode = 0;

	// Parse options:
	for (;;)
	{
		int c;
		int opt_index = 0;
		static struct option opt_long[] = {
			{ "decode", 0, 0, 'd' },
			{ 0, 0, 0, 0 }
		};
		if ((c = getopt_long(argc, argv, "d", opt_long, &opt_index)) == -1) {
			break;
		}
		switch (c)
		{
			case 'd':
				decode = 1;
				break;
		}
	}

	// No options left on command line? Read from stdin:
	if (optind >= argc) {
		fp = stdin;
	}

	// One option left on command line? Treat it as a file:
	else if (optind + 1 == argc) {
		file = argv[optind];
		if (strcmp(file, "-") == 0) {
			fp = stdin;
		}
		else if ((fp = fopen(file, "rb")) == NULL) {
			printf("cannot open %s\n", file);
			return 1;
		}
	}

	// More than one option left on command line? Syntax error:
	else {
		printf("Usage: %s <file>\n", argv[0]);
		return 1;
	}

	// Invert return codes to create shell return code:
	return (decode) ? !dec(fp) : !enc(fp);
}
