/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/*
 * gcc -Wall -lproc -o extract_dmod extract_dmod.c
 *
 * use this tool to extract the mdb_v8.so from a core file for use with older
 * mdb or platforms with an out of date v8.so
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <strings.h>
#include <errno.h>
#include <libproc.h>
#include <sys/sysmacros.h>

const char *g_sym = "mdb_v8.so_start";

int
main(int argc, const char *argv[])
{
	int ifd, ofd, perr, toread, ret;
	struct ps_prochandle *p;
	GElf_Sym sym;
	prsyminfo_t si;
	char buf[1024];

	if (argc != 3) {
		fprintf(stderr, "pdump: <infile> <outfile>\n");
		return (1);
	}

	ifd = open(argv[1], O_RDONLY);
	if (ifd < 0) {
		fprintf(stderr, "failed to open: %s: %s\n",
		    argv[1], strerror(errno));
		return (1);
	}
	ofd = open(argv[2], O_RDWR | O_CREAT | O_TRUNC);
	if (ofd < 0) {
		fprintf(stderr, "failed to open: %s: %s\n",
		    argv[1], strerror(errno));
		return (1);
	}

	p = Pfgrab_core(ifd, NULL, &perr); 
	if (p == NULL) {
		fprintf(stderr, "failed to grab core file\n");
		return (1);
	}

	if (Pxlookup_by_name(p, NULL, "a.out", g_sym, &sym, &si) != 0) {
		fprintf(stderr, "failed to lookup symobl %s\n", g_sym);
		return (0);
	}

	while (sym.st_size > 0) {
		toread = MIN(sym.st_size, sizeof (buf));
		ret = Pread(p, buf, toread, sym.st_value);
		if (ret < 0) {
			fprintf(stderr, "failed to Pread...\n");
			return (1);
		}
		if (ret != 0)
			ret = write(ofd, buf, ret);
		if (ret < 0) {
			fprintf(stderr, "failed to write to output file %s\n",
			    strerror(errno));
		}
		sym.st_size -= ret;
		sym.st_value += ret;
	}

	return (0);
}
