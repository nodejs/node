/* Copyright (c) 2013-2014 Yoran Heling

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#if defined(STRLEN)
static void xmlbench(const char *buf, size_t bufsize) {
	size_t l = strlen(buf);
	assert(l == bufsize);
}


#elif defined(YXML)
#include <yxml.h>
static void xmlbench(const char *buf, size_t bufsize) {
	yxml_t x[1];
	char *stack = malloc(4096);
	yxml_init(x, stack, 4096);
	yxml_ret_t r;
	do {
		r = yxml_parse(x, *buf);
		buf++;
	} while(*buf && r >= 0);
	if(r >= 0)
		r = yxml_eof(x);
	/*printf("t%03lu l%03u b%03lu: %c %d", x->total, x->line, x->byte, *buf, r);*/
	assert(!*buf && r == YXML_OK);
}


#elif defined(EXPAT)
#include <expat.h>
static void xmlbench(const char *buf, size_t bufsize) {
	XML_Parser p = XML_ParserCreate(NULL);
	int r = XML_Parse(p, buf, bufsize, 1);
	assert(r != XML_STATUS_ERROR);
	XML_ParserFree(p);
}


#elif defined(LIBXML2)
#include <libxml/xmlreader.h>
static void xmlbench(const char *buf, size_t bufsize) {
	xmlTextReaderPtr p = xmlReaderForMemory(buf, bufsize, NULL, NULL, 0);
	while(xmlTextReaderRead(p) == 1)
		;
	xmlFreeTextReader(p);
}


#elif defined(MXML)
#include <mxml.h>
static void sax_cb(mxml_node_t *node, mxml_sax_event_t event, void *data) {
	static int i;
	/* Only retain the root node, to make sure that we don't get NULL. */
	if(event == MXML_SAX_ELEMENT_OPEN && !i) {
		mxmlRetain(node);
		i=1;
	}
}
static void xmlbench(const char *buf, size_t bufsize) {
	mxml_node_t *n = mxmlSAXLoadString(NULL, buf, MXML_NO_CALLBACK, sax_cb, NULL);
	assert(n);
}


#else
#error No idea what to bench
#endif


int main(int argc, char **argv) {
	struct stat st;
	ssize_t r;

	assert(argc == 2);
	r = stat(argv[1], &st);
	assert(!r);

	char *buf = malloc(st.st_size+1);
	assert(buf);

	int fd = open(argv[1], O_RDONLY);
	assert(fd > 0);

	ssize_t rd = read(fd, buf, st.st_size);
	assert(rd == st.st_size);
	buf[st.st_size] = 0;
	close(fd);

	xmlbench(buf, st.st_size);
	return 0;
}

/* vim: set noet sw=4 ts=4: */
