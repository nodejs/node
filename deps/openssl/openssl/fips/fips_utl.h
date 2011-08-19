/* ====================================================================
 * Copyright (c) 2007 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

void do_print_errors(void);
int hex2bin(const char *in, unsigned char *out);
unsigned char *hex2bin_m(const char *in, long *plen);
int do_hex2bn(BIGNUM **pr, const char *in);
int do_bn_print(FILE *out, BIGNUM *bn);
int do_bn_print_name(FILE *out, const char *name, BIGNUM *bn);
int parse_line(char **pkw, char **pval, char *linebuf, char *olinebuf);
BIGNUM *hex2bn(const char *in);
int bin2hex(const unsigned char *in,int len,char *out);
void pv(const char *tag,const unsigned char *val,int len);
int tidy_line(char *linebuf, char *olinebuf);
int bint2bin(const char *in, int len, unsigned char *out);
int bin2bint(const unsigned char *in,int len,char *out);
void PrintValue(char *tag, unsigned char *val, int len);
void OutputValue(char *tag, unsigned char *val, int len, FILE *rfp,int bitmode);

void do_print_errors(void)
	{
	const char *file, *data;
	int line, flags;
	unsigned long l;
	while ((l = ERR_get_error_line_data(&file, &line, &data, &flags)))
		{
		fprintf(stderr, "ERROR:%lx:lib=%d,func=%d,reason=%d"
				":file=%s:line=%d:%s\n",
			l, ERR_GET_LIB(l), ERR_GET_FUNC(l), ERR_GET_REASON(l),
			file, line, flags & ERR_TXT_STRING ? data : "");
		}
	}

int hex2bin(const char *in, unsigned char *out)
    {
    int n1, n2;
    unsigned char ch;

    for (n1=0,n2=0 ; in[n1] && in[n1] != '\n' ; )
	{ /* first byte */
	if ((in[n1] >= '0') && (in[n1] <= '9'))
	    ch = in[n1++] - '0';
	else if ((in[n1] >= 'A') && (in[n1] <= 'F'))
	    ch = in[n1++] - 'A' + 10;
	else if ((in[n1] >= 'a') && (in[n1] <= 'f'))
	    ch = in[n1++] - 'a' + 10;
	else
	    return -1;
	if(!in[n1])
	    {
	    out[n2++]=ch;
	    break;
	    }
	out[n2] = ch << 4;
	/* second byte */
	if ((in[n1] >= '0') && (in[n1] <= '9'))
	    ch = in[n1++] - '0';
	else if ((in[n1] >= 'A') && (in[n1] <= 'F'))
	    ch = in[n1++] - 'A' + 10;
	else if ((in[n1] >= 'a') && (in[n1] <= 'f'))
	    ch = in[n1++] - 'a' + 10;
	else
	    return -1;
	out[n2++] |= ch;
	}
    return n2;
    }

unsigned char *hex2bin_m(const char *in, long *plen)
	{
	unsigned char *p;
	p = OPENSSL_malloc((strlen(in) + 1)/2);
	*plen = hex2bin(in, p);
	return p;
	}

int do_hex2bn(BIGNUM **pr, const char *in)
	{
	unsigned char *p;
	long plen;
	int r = 0;
	p = hex2bin_m(in, &plen);
	if (!p)
		return 0;
	if (!*pr)
		*pr = BN_new();
	if (!*pr)
		return 0;
	if (BN_bin2bn(p, plen, *pr))
		r = 1;
	OPENSSL_free(p);
	return r;
	}

int do_bn_print(FILE *out, BIGNUM *bn)
	{
	int len, i;
	unsigned char *tmp;
	len = BN_num_bytes(bn);
	if (len == 0)
		{
		fputs("00", out);
		return 1;
		}

	tmp = OPENSSL_malloc(len);
	if (!tmp)
		{
		fprintf(stderr, "Memory allocation error\n");
		return 0;
		}
	BN_bn2bin(bn, tmp);
	for (i = 0; i < len; i++)
		fprintf(out, "%02x", tmp[i]);
	OPENSSL_free(tmp);
	return 1;
	}

int do_bn_print_name(FILE *out, const char *name, BIGNUM *bn)
	{
	int r;
	fprintf(out, "%s = ", name);
	r = do_bn_print(out, bn);
	if (!r)
		return 0;
	fputs("\n", out);
	return 1;
	}

int parse_line(char **pkw, char **pval, char *linebuf, char *olinebuf)
	{
	char *keyword, *value, *p, *q;
	strcpy(linebuf, olinebuf);
	keyword = linebuf;
	/* Skip leading space */
	while (isspace((unsigned char)*keyword))
		keyword++;

	/* Look for = sign */
	p = strchr(linebuf, '=');

	/* If no '=' exit */
	if (!p)
		return 0;

	q = p - 1;

	/* Remove trailing space */
	while (isspace((unsigned char)*q))
		*q-- = 0;

	*p = 0;
	value = p + 1;

	/* Remove leading space from value */
	while (isspace((unsigned char)*value))
		value++;

	/* Remove trailing space from value */
	p = value + strlen(value) - 1;

	while (*p == '\n' || isspace((unsigned char)*p))
		*p-- = 0;

	*pkw = keyword;
	*pval = value;
	return 1;
	}

BIGNUM *hex2bn(const char *in)
    {
    BIGNUM *p=NULL;

    if (!do_hex2bn(&p, in))
	return NULL;

    return p;
    }

int bin2hex(const unsigned char *in,int len,char *out)
    {
    int n1, n2;
    unsigned char ch;

    for (n1=0,n2=0 ; n1 < len ; ++n1)
	{
	ch=in[n1] >> 4;
	if (ch <= 0x09)
	    out[n2++]=ch+'0';
	else
	    out[n2++]=ch-10+'a';
	ch=in[n1] & 0x0f;
	if(ch <= 0x09)
	    out[n2++]=ch+'0';
	else
	    out[n2++]=ch-10+'a';
	}
    out[n2]='\0';
    return n2;
    }

void pv(const char *tag,const unsigned char *val,int len)
    {
    char obuf[2048];

    bin2hex(val,len,obuf);
    printf("%s = %s\n",tag,obuf);
    }

/* To avoid extensive changes to test program at this stage just convert
 * the input line into an acceptable form. Keyword lines converted to form
 * "keyword = value\n" no matter what white space present, all other lines
 * just have leading and trailing space removed.
 */

int tidy_line(char *linebuf, char *olinebuf)
	{
	char *keyword, *value, *p, *q;
	strcpy(linebuf, olinebuf);
	keyword = linebuf;
	/* Skip leading space */
	while (isspace((unsigned char)*keyword))
		keyword++;
	/* Look for = sign */
	p = strchr(linebuf, '=');

	/* If no '=' just chop leading, trailing ws */
	if (!p)
		{
		p = keyword + strlen(keyword) - 1;
		while (*p == '\n' || isspace((unsigned char)*p))
			*p-- = 0;
		strcpy(olinebuf, keyword);
		strcat(olinebuf, "\n");
		return 1;
		}

	q = p - 1;

	/* Remove trailing space */
	while (isspace((unsigned char)*q))
		*q-- = 0;

	*p = 0;
	value = p + 1;

	/* Remove leading space from value */
	while (isspace((unsigned char)*value))
		value++;

	/* Remove trailing space from value */
	p = value + strlen(value) - 1;

	while (*p == '\n' || isspace((unsigned char)*p))
		*p-- = 0;

	strcpy(olinebuf, keyword);
	strcat(olinebuf, " = ");
	strcat(olinebuf, value);
	strcat(olinebuf, "\n");

	return 1;
	}

/* NB: this return the number of _bits_ read */
int bint2bin(const char *in, int len, unsigned char *out)
    {
    int n;

    memset(out,0,len);
    for(n=0 ; n < len ; ++n)
	if(in[n] == '1')
	    out[n/8]|=(0x80 >> (n%8));
    return len;
    }

int bin2bint(const unsigned char *in,int len,char *out)
    {
    int n;

    for(n=0 ; n < len ; ++n)
	out[n]=(in[n/8]&(0x80 >> (n%8))) ? '1' : '0';
    return n;
    }

/*-----------------------------------------------*/

void PrintValue(char *tag, unsigned char *val, int len)
{
#if VERBOSE
  char obuf[2048];
  int olen;
  olen = bin2hex(val, len, obuf);
  printf("%s = %.*s\n", tag, olen, obuf);
#endif
}

void OutputValue(char *tag, unsigned char *val, int len, FILE *rfp,int bitmode)
    {
    char obuf[2048];
    int olen;

    if(bitmode)
	olen=bin2bint(val,len,obuf);
    else
	olen=bin2hex(val,len,obuf);

    fprintf(rfp, "%s = %.*s\n", tag, olen, obuf);
#if VERBOSE
    printf("%s = %.*s\n", tag, olen, obuf);
#endif
    }

