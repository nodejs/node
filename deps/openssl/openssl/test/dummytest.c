#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <openssl/e_os2.h>
#include <openssl/buffer.h>
#include <openssl/crypto.h>

int main(int argc, char *argv[])
	{
	char *p, *q = 0, *program;

	p = strrchr(argv[0], '/');
	if (!p) p = strrchr(argv[0], '\\');
#ifdef OPENSSL_SYS_VMS
	if (!p) p = strrchr(argv[0], ']');
	if (p) q = strrchr(p, '>');
	if (q) p = q;
	if (!p) p = strrchr(argv[0], ':');
	q = 0;
#endif
	if (p) p++;
	if (!p) p = argv[0];
	if (p) q = strchr(p, '.');
	if (p && !q) q = p + strlen(p);

	if (!p)
		program = BUF_strdup("(unknown)");
	else
		{
		program = OPENSSL_malloc((q - p) + 1);
		strncpy(program, p, q - p);
		program[q - p] = '\0';
		}

	for(p = program; *p; p++)
		if (islower((unsigned char)(*p)))
			*p = toupper((unsigned char)(*p));

	q = strstr(program, "TEST");
	if (q > p && q[-1] == '_') q--;
	*q = '\0';

	printf("No %s support\n", program);

	OPENSSL_free(program);
	return(0);
	}
