#ifndef __TEST_UTIL_H__
#define __TEST_UTIL_H__

#include "../jsmn.c"

static int vtokeq(const char *s, jsmntok_t *t, int numtok, va_list ap) {
	if (numtok > 0) {
		int i, start, end, size;
		int type;
		char *value;

		size = -1;
		value = NULL;
		for (i = 0; i < numtok; i++) {
			type = va_arg(ap, int);
			if (type == JSMN_STRING) {
				value = va_arg(ap, char *);
				size = va_arg(ap, int);
				start = end = -1;
			} else if (type == JSMN_PRIMITIVE) {
				value = va_arg(ap, char *);
				start = end = size = -1;
			} else {
				start = va_arg(ap, int);
				end = va_arg(ap, int);
				size = va_arg(ap, int);
				value = NULL;
			}
			if (t[i].type != type) {
				printf("token %d type is %d, not %d\n", i, t[i].type, type);
				return 0;
			}
			if (start != -1 && end != -1) {
				if (t[i].start != start) {
					printf("token %d start is %d, not %d\n", i, t[i].start, start);
					return 0;
				}
				if (t[i].end != end ) {
					printf("token %d end is %d, not %d\n", i, t[i].end, end);
					return 0;
				}
			}
			if (size != -1 && t[i].size != size) {
				printf("token %d size is %d, not %d\n", i, t[i].size, size);
				return 0;
			}

			if (s != NULL && value != NULL) {
				const char *p = s + t[i].start;
				if (strlen(value) != t[i].end - t[i].start ||
						strncmp(p, value, t[i].end - t[i].start) != 0) {
					printf("token %d value is %.*s, not %s\n", i, t[i].end-t[i].start,
							s+t[i].start, value);
					return 0;
				}
			}
		}
	}
	return 1;
}

static int tokeq(const char *s, jsmntok_t *tokens, int numtok, ...) {
	int ok;
	va_list args;
	va_start(args, numtok);
	ok = vtokeq(s, tokens, numtok, args); 
	va_end(args);
	return ok;
}

static int parse(const char *s, int status, int numtok, ...) {
	int r;
	int ok = 1;
	va_list args;
	jsmn_parser p;
	jsmntok_t *t = malloc(numtok * sizeof(jsmntok_t));

	jsmn_init(&p);
	r = jsmn_parse(&p, s, strlen(s), t, numtok);
	if (r != status) {
		printf("status is %d, not %d\n", r, status);
		return 0;
	}

	if (status >= 0) {
		va_start(args, numtok);
		ok = vtokeq(s, t, numtok, args); 
		va_end(args);
	}
	free(t);
	return ok;
}

#endif /* __TEST_UTIL_H__ */
