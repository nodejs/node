#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/objects.h>
#include <openssl/comp.h>

static int rle_compress_block(COMP_CTX *ctx, unsigned char *out,
	unsigned int olen, unsigned char *in, unsigned int ilen);
static int rle_expand_block(COMP_CTX *ctx, unsigned char *out,
	unsigned int olen, unsigned char *in, unsigned int ilen);

static COMP_METHOD rle_method={
	NID_rle_compression,
	LN_rle_compression,
	NULL,
	NULL,
	rle_compress_block,
	rle_expand_block,
	NULL,
	NULL,
	};

COMP_METHOD *COMP_rle(void)
	{
	return(&rle_method);
	}

static int rle_compress_block(COMP_CTX *ctx, unsigned char *out,
	     unsigned int olen, unsigned char *in, unsigned int ilen)
	{
	/* int i; */

	if (olen < (ilen+1))
		{
		/* ZZZZZZZZZZZZZZZZZZZZZZ */
		return(-1);
		}

	*(out++)=0;
	memcpy(out,in,ilen);
	return(ilen+1);
	}

static int rle_expand_block(COMP_CTX *ctx, unsigned char *out,
	     unsigned int olen, unsigned char *in, unsigned int ilen)
	{
	int i;

	if (olen < (ilen-1))
		{
		/* ZZZZZZZZZZZZZZZZZZZZZZ */
		return(-1);
		}

	i= *(in++);
	if (i == 0)
		{
		memcpy(out,in,ilen-1);
		}
	return(ilen-1);
	}

