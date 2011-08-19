#include <openssl/opensslconf.h>

#ifndef OPENSSL_FIPS
#include <stdio.h>

int main(int argc, char **argv)
{
    printf("No FIPS DSA support\n");
    return(0);
}
#else

#include <openssl/bn.h>
#include <openssl/dsa.h>
#include <openssl/fips.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <string.h>
#include <ctype.h>

#include "fips_utl.h"

static void pbn(const char *name, BIGNUM *bn)
	{
	int len, i;
	unsigned char *tmp;
	len = BN_num_bytes(bn);
	tmp = OPENSSL_malloc(len);
	if (!tmp)
		{
		fprintf(stderr, "Memory allocation error\n");
		return;
		}
	BN_bn2bin(bn, tmp);
	printf("%s = ", name);
	for (i = 0; i < len; i++)
		printf("%02X", tmp[i]);
	fputs("\n", stdout);
	OPENSSL_free(tmp);
	return;
	}

static void primes()
    {
    char buf[10240];
    char lbuf[10240];
    char *keyword, *value;

    while(fgets(buf,sizeof buf,stdin) != NULL)
	{
	fputs(buf,stdout);
	if (!parse_line(&keyword, &value, lbuf, buf))
		continue;
	if(!strcmp(keyword,"Prime"))
	    {
	    BIGNUM *pp;

	    pp=BN_new();
	    do_hex2bn(&pp,value);
	    printf("result= %c\n",
		   BN_is_prime_ex(pp,20,NULL,NULL) ? 'P' : 'F');
	    }	    
	}
    }

static void pqg()
    {
    char buf[1024];
    char lbuf[1024];
    char *keyword, *value;
    int nmod=0;

    while(fgets(buf,sizeof buf,stdin) != NULL)
	{
	if (!parse_line(&keyword, &value, lbuf, buf))
		{
		fputs(buf,stdout);
		continue;
		}
	if(!strcmp(keyword,"[mod"))
	    nmod=atoi(value);
	else if(!strcmp(keyword,"N"))
	    {
	    int n=atoi(value);

	    printf("[mod = %d]\n\n",nmod);

	    while(n--)
		{
		unsigned char seed[20];
		DSA *dsa;
		int counter;
		unsigned long h;
		dsa = FIPS_dsa_new();

		if (!DSA_generate_parameters_ex(dsa, nmod,seed,0,&counter,&h,NULL))
			{
			do_print_errors();
			exit(1);
			}
		pbn("P",dsa->p);
		pbn("Q",dsa->q);
		pbn("G",dsa->g);
		pv("Seed",seed,20);
		printf("c = %d\n",counter);
		printf("H = %lx\n",h);
		putc('\n',stdout);
		}
	    }
	else
	    fputs(buf,stdout);
	}
    }

static void pqgver()
    {
    char buf[1024];
    char lbuf[1024];
    char *keyword, *value;
    BIGNUM *p = NULL, *q = NULL, *g = NULL;
    int counter, counter2;
    unsigned long h, h2;
    DSA *dsa=NULL;
    int nmod=0;
    unsigned char seed[1024];

    while(fgets(buf,sizeof buf,stdin) != NULL)
	{
	if (!parse_line(&keyword, &value, lbuf, buf))
		{
		fputs(buf,stdout);
		continue;
		}
	fputs(buf, stdout);
	if(!strcmp(keyword,"[mod"))
	    nmod=atoi(value);
	else if(!strcmp(keyword,"P"))
	    p=hex2bn(value);
	else if(!strcmp(keyword,"Q"))
	    q=hex2bn(value);
	else if(!strcmp(keyword,"G"))
	    g=hex2bn(value);
	else if(!strcmp(keyword,"Seed"))
	    {
	    int slen = hex2bin(value, seed);
	    if (slen != 20)
		{
		fprintf(stderr, "Seed parse length error\n");
		exit (1);
		}
	    }
	else if(!strcmp(keyword,"c"))
	    counter =atoi(buf+4);
	else if(!strcmp(keyword,"H"))
	    {
	    h = atoi(value);
	    if (!p || !q || !g)
		{
		fprintf(stderr, "Parse Error\n");
		exit (1);
		}
	    dsa = FIPS_dsa_new();
	    if (!DSA_generate_parameters_ex(dsa, nmod,seed,20 ,&counter2,&h2,NULL))
			{
			do_print_errors();
			exit(1);
			}
            if (BN_cmp(dsa->p, p) || BN_cmp(dsa->q, q) || BN_cmp(dsa->g, g)
		|| (counter != counter2) || (h != h2))
	    	printf("Result = F\n");
	    else
	    	printf("Result = P\n");
	    BN_free(p);
	    BN_free(q);
	    BN_free(g);
	    p = NULL;
	    q = NULL;
	    g = NULL;
	    FIPS_dsa_free(dsa);
	    dsa = NULL;
	    }
	}
    }

/* Keypair verification routine. NB: this isn't part of the standard FIPS140-2
 * algorithm tests. It is an additional test to perform sanity checks on the
 * output of the KeyPair test.
 */

static int dss_paramcheck(int nmod, BIGNUM *p, BIGNUM *q, BIGNUM *g,
							BN_CTX *ctx)
    {
    BIGNUM *rem = NULL;
    if (BN_num_bits(p) != nmod)
	return 0;
    if (BN_num_bits(q) != 160)
	return 0;
    if (BN_is_prime_ex(p, BN_prime_checks, ctx, NULL) != 1)
	return 0;
    if (BN_is_prime_ex(q, BN_prime_checks, ctx, NULL) != 1)
	return 0;
    rem = BN_new();
    if (!BN_mod(rem, p, q, ctx) || !BN_is_one(rem)
    	|| (BN_cmp(g, BN_value_one()) <= 0)
	|| !BN_mod_exp(rem, g, q, p, ctx) || !BN_is_one(rem))
	{
	BN_free(rem);
	return 0;
	}
    /* Todo: check g */
    BN_free(rem);
    return 1;
    }

static void keyver()
    {
    char buf[1024];
    char lbuf[1024];
    char *keyword, *value;
    BIGNUM *p = NULL, *q = NULL, *g = NULL, *X = NULL, *Y = NULL;
    BIGNUM *Y2;
    BN_CTX *ctx = NULL;
    int nmod=0, paramcheck = 0;

    ctx = BN_CTX_new();
    Y2 = BN_new();

    while(fgets(buf,sizeof buf,stdin) != NULL)
	{
	if (!parse_line(&keyword, &value, lbuf, buf))
		{
		fputs(buf,stdout);
		continue;
		}
	if(!strcmp(keyword,"[mod"))
	    {
	    if (p)
		BN_free(p);
	    p = NULL;
	    if (q)
		BN_free(q);
	    q = NULL;
	    if (g)
		BN_free(g);
	    g = NULL;
	    paramcheck = 0;
	    nmod=atoi(value);
	    }
	else if(!strcmp(keyword,"P"))
	    p=hex2bn(value);
	else if(!strcmp(keyword,"Q"))
	    q=hex2bn(value);
	else if(!strcmp(keyword,"G"))
	    g=hex2bn(value);
	else if(!strcmp(keyword,"X"))
	    X=hex2bn(value);
	else if(!strcmp(keyword,"Y"))
	    {
	    Y=hex2bn(value);
	    if (!p || !q || !g || !X || !Y)
		{
		fprintf(stderr, "Parse Error\n");
		exit (1);
		}
	    pbn("P",p);
	    pbn("Q",q);
	    pbn("G",g);
	    pbn("X",X);
	    pbn("Y",Y);
	    if (!paramcheck)
		{
		if (dss_paramcheck(nmod, p, q, g, ctx))
			paramcheck = 1;
		else
			paramcheck = -1;
		}
	    if (paramcheck != 1)
	   	printf("Result = F\n");
	    else
		{
		if (!BN_mod_exp(Y2, g, X, p, ctx) || BN_cmp(Y2, Y))
	    		printf("Result = F\n");
	        else
	    		printf("Result = P\n");
		}
	    BN_free(X);
	    BN_free(Y);
	    X = NULL;
	    Y = NULL;
	    }
	}
	if (p)
	    BN_free(p);
	if (q)
	    BN_free(q);
	if (g)
	    BN_free(g);
	if (Y2)
	    BN_free(Y2);
    }

static void keypair()
    {
    char buf[1024];
    char lbuf[1024];
    char *keyword, *value;
    int nmod=0;

    while(fgets(buf,sizeof buf,stdin) != NULL)
	{
	if (!parse_line(&keyword, &value, lbuf, buf))
		{
		fputs(buf,stdout);
		continue;
		}
	if(!strcmp(keyword,"[mod"))
	    nmod=atoi(value);
	else if(!strcmp(keyword,"N"))
	    {
	    DSA *dsa;
	    int n=atoi(value);

	    printf("[mod = %d]\n\n",nmod);
	    dsa = FIPS_dsa_new();
	    if (!DSA_generate_parameters_ex(dsa, nmod,NULL,0,NULL,NULL,NULL))
		{
		do_print_errors();
		exit(1);
		}
	    pbn("P",dsa->p);
	    pbn("Q",dsa->q);
	    pbn("G",dsa->g);
	    putc('\n',stdout);

	    while(n--)
		{
		if (!DSA_generate_key(dsa))
			{
			do_print_errors();
			exit(1);
			}

		pbn("X",dsa->priv_key);
		pbn("Y",dsa->pub_key);
		putc('\n',stdout);
		}
	    }
	}
    }

static void siggen()
    {
    char buf[1024];
    char lbuf[1024];
    char *keyword, *value;
    int nmod=0;
    DSA *dsa=NULL;

    while(fgets(buf,sizeof buf,stdin) != NULL)
	{
	if (!parse_line(&keyword, &value, lbuf, buf))
		{
		fputs(buf,stdout);
		continue;
		}
	if(!strcmp(keyword,"[mod"))
	    {
	    nmod=atoi(value);
	    printf("[mod = %d]\n\n",nmod);
	    if (dsa)
		FIPS_dsa_free(dsa);
	    dsa = FIPS_dsa_new();
	    if (!DSA_generate_parameters_ex(dsa, nmod,NULL,0,NULL,NULL,NULL))
		{
		do_print_errors();
		exit(1);
		}
	    pbn("P",dsa->p);
	    pbn("Q",dsa->q);
	    pbn("G",dsa->g);
	    putc('\n',stdout);
	    }
	else if(!strcmp(keyword,"Msg"))
	    {
	    unsigned char msg[1024];
	    unsigned char sbuf[60];
	    unsigned int slen;
	    int n;
	    EVP_PKEY pk;
	    EVP_MD_CTX mctx;
	    DSA_SIG *sig;
	    EVP_MD_CTX_init(&mctx);

	    n=hex2bin(value,msg);
	    pv("Msg",msg,n);

	    if (!DSA_generate_key(dsa))
		{
		do_print_errors();
		exit(1);
		}
	    pk.type = EVP_PKEY_DSA;
	    pk.pkey.dsa = dsa;
	    pbn("Y",dsa->pub_key);

	    EVP_SignInit_ex(&mctx, EVP_dss1(), NULL);
	    EVP_SignUpdate(&mctx, msg, n);
	    EVP_SignFinal(&mctx, sbuf, &slen, &pk);

	    sig = DSA_SIG_new();
	    FIPS_dsa_sig_decode(sig, sbuf, slen);

	    pbn("R",sig->r);
	    pbn("S",sig->s);
	    putc('\n',stdout);
	    DSA_SIG_free(sig);
	    EVP_MD_CTX_cleanup(&mctx);
	    }
	}
	if (dsa)
		FIPS_dsa_free(dsa);
    }

static void sigver()
    {
    DSA *dsa=NULL;
    char buf[1024];
    char lbuf[1024];
    unsigned char msg[1024];
    char *keyword, *value;
    int nmod=0, n=0;
    DSA_SIG sg, *sig = &sg;

    sig->r = NULL;
    sig->s = NULL;

    while(fgets(buf,sizeof buf,stdin) != NULL)
	{
	if (!parse_line(&keyword, &value, lbuf, buf))
		{
		fputs(buf,stdout);
		continue;
		}
	if(!strcmp(keyword,"[mod"))
	    {
	    nmod=atoi(value);
	    if(dsa)
		FIPS_dsa_free(dsa);
	    dsa=FIPS_dsa_new();
	    }
	else if(!strcmp(keyword,"P"))
	    dsa->p=hex2bn(value);
	else if(!strcmp(keyword,"Q"))
	    dsa->q=hex2bn(value);
	else if(!strcmp(keyword,"G"))
	    {
	    dsa->g=hex2bn(value);

	    printf("[mod = %d]\n\n",nmod);
	    pbn("P",dsa->p);
	    pbn("Q",dsa->q);
	    pbn("G",dsa->g);
	    putc('\n',stdout);
	    }
	else if(!strcmp(keyword,"Msg"))
	    {
	    n=hex2bin(value,msg);
	    pv("Msg",msg,n);
	    }
	else if(!strcmp(keyword,"Y"))
	    dsa->pub_key=hex2bn(value);
	else if(!strcmp(keyword,"R"))
	    sig->r=hex2bn(value);
	else if(!strcmp(keyword,"S"))
	    {
	    EVP_MD_CTX mctx;
	    EVP_PKEY pk;
	    unsigned char sigbuf[60];
	    unsigned int slen;
	    int r;
	    EVP_MD_CTX_init(&mctx);
	    pk.type = EVP_PKEY_DSA;
	    pk.pkey.dsa = dsa;
	    sig->s=hex2bn(value);
	
	    pbn("Y",dsa->pub_key);
	    pbn("R",sig->r);
	    pbn("S",sig->s);

	    slen = FIPS_dsa_sig_encode(sigbuf, sig);
	    EVP_VerifyInit_ex(&mctx, EVP_dss1(), NULL);
	    EVP_VerifyUpdate(&mctx, msg, n);
	    r = EVP_VerifyFinal(&mctx, sigbuf, slen, &pk);
	    EVP_MD_CTX_cleanup(&mctx);
	
	    printf("Result = %c\n", r == 1 ? 'P' : 'F');
	    putc('\n',stdout);
	    }
	}
    }

int main(int argc,char **argv)
    {
    if(argc != 2)
	{
	fprintf(stderr,"%s [prime|pqg|pqgver|keypair|siggen|sigver]\n",argv[0]);
	exit(1);
	}
    if(!FIPS_mode_set(1))
	{
	do_print_errors();
	exit(1);
	}
    if(!strcmp(argv[1],"prime"))
	primes();
    else if(!strcmp(argv[1],"pqg"))
	pqg();
    else if(!strcmp(argv[1],"pqgver"))
	pqgver();
    else if(!strcmp(argv[1],"keypair"))
	keypair();
    else if(!strcmp(argv[1],"keyver"))
	keyver();
    else if(!strcmp(argv[1],"siggen"))
	siggen();
    else if(!strcmp(argv[1],"sigver"))
	sigver();
    else
	{
	fprintf(stderr,"Don't know how to %s.\n",argv[1]);
	exit(1);
	}

    return 0;
    }

#endif
