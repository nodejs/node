/* 
 * Support for VIA PadLock Advanced Cryptography Engine (ACE)
 * Written by Michal Ludvig <michal@logix.cz>
 *            http://www.logix.cz/michal
 *
 * Big thanks to Andy Polyakov for a help with optimization, 
 * assembler fixes, port to MS Windows and a lot of other 
 * valuable work on this engine!
 */

/* ====================================================================
 * Copyright (c) 1999-2001 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
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
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */


#include <stdio.h>
#include <string.h>

#include <openssl/opensslconf.h>
#include <openssl/crypto.h>
#include <openssl/dso.h>
#include <openssl/engine.h>
#include <openssl/evp.h>
#ifndef OPENSSL_NO_AES
#include <openssl/aes.h>
#endif
#include <openssl/rand.h>
#include <openssl/err.h>

#ifndef OPENSSL_NO_HW
#ifndef OPENSSL_NO_HW_PADLOCK

/* Attempt to have a single source for both 0.9.7 and 0.9.8 :-) */
#if (OPENSSL_VERSION_NUMBER >= 0x00908000L)
#  ifndef OPENSSL_NO_DYNAMIC_ENGINE
#    define DYNAMIC_ENGINE
#  endif
#elif (OPENSSL_VERSION_NUMBER >= 0x00907000L)
#  ifdef ENGINE_DYNAMIC_SUPPORT
#    define DYNAMIC_ENGINE
#  endif
#else
#  error "Only OpenSSL >= 0.9.7 is supported"
#endif

/* VIA PadLock AES is available *ONLY* on some x86 CPUs.
   Not only that it doesn't exist elsewhere, but it
   even can't be compiled on other platforms!
 
   In addition, because of the heavy use of inline assembler,
   compiler choice is limited to GCC and Microsoft C. */
#undef COMPILE_HW_PADLOCK
#if !defined(I386_ONLY) && !defined(OPENSSL_NO_INLINE_ASM)
# if (defined(__GNUC__) && (defined(__i386__) || defined(__i386))) || \
     (defined(_MSC_VER) && defined(_M_IX86))
#  define COMPILE_HW_PADLOCK
# endif
#endif

#ifdef OPENSSL_NO_DYNAMIC_ENGINE
#ifdef COMPILE_HW_PADLOCK
static ENGINE *ENGINE_padlock (void);
#endif

void ENGINE_load_padlock (void)
{
/* On non-x86 CPUs it just returns. */
#ifdef COMPILE_HW_PADLOCK
	ENGINE *toadd = ENGINE_padlock ();
	if (!toadd) return;
	ENGINE_add (toadd);
	ENGINE_free (toadd);
	ERR_clear_error ();
#endif
}

#endif

#ifdef COMPILE_HW_PADLOCK
/* We do these includes here to avoid header problems on platforms that
   do not have the VIA padlock anyway... */
#include <stdlib.h>
#ifdef _WIN32
# include <malloc.h>
# ifndef alloca
#  define alloca _alloca
# endif
#elif defined(__GNUC__)
# ifndef alloca
#  define alloca(s) __builtin_alloca(s)
# endif
#endif

/* Function for ENGINE detection and control */
static int padlock_available(void);
static int padlock_init(ENGINE *e);

/* RNG Stuff */
static RAND_METHOD padlock_rand;

/* Cipher Stuff */
#ifndef OPENSSL_NO_AES
static int padlock_ciphers(ENGINE *e, const EVP_CIPHER **cipher, const int **nids, int nid);
#endif

/* Engine names */
static const char *padlock_id = "padlock";
static char padlock_name[100];

/* Available features */
static int padlock_use_ace = 0;	/* Advanced Cryptography Engine */
static int padlock_use_rng = 0;	/* Random Number Generator */
#ifndef OPENSSL_NO_AES
static int padlock_aes_align_required = 1;
#endif

/* ===== Engine "management" functions ===== */

/* Prepare the ENGINE structure for registration */
static int
padlock_bind_helper(ENGINE *e)
{
	/* Check available features */
	padlock_available();

#if 1	/* disable RNG for now, see commentary in vicinity of RNG code */
	padlock_use_rng=0;
#endif

	/* Generate a nice engine name with available features */
	BIO_snprintf(padlock_name, sizeof(padlock_name),
		"VIA PadLock (%s, %s)", 
		 padlock_use_rng ? "RNG" : "no-RNG",
		 padlock_use_ace ? "ACE" : "no-ACE");

	/* Register everything or return with an error */ 
	if (!ENGINE_set_id(e, padlock_id) ||
	    !ENGINE_set_name(e, padlock_name) ||

	    !ENGINE_set_init_function(e, padlock_init) ||
#ifndef OPENSSL_NO_AES
	    (padlock_use_ace && !ENGINE_set_ciphers (e, padlock_ciphers)) ||
#endif
	    (padlock_use_rng && !ENGINE_set_RAND (e, &padlock_rand))) {
		return 0;
	}

	/* Everything looks good */
	return 1;
}

#ifdef OPENSSL_NO_DYNAMIC_ENGINE

/* Constructor */
static ENGINE *
ENGINE_padlock(void)
{
	ENGINE *eng = ENGINE_new();

	if (!eng) {
		return NULL;
	}

	if (!padlock_bind_helper(eng)) {
		ENGINE_free(eng);
		return NULL;
	}

	return eng;
}

#endif

/* Check availability of the engine */
static int
padlock_init(ENGINE *e)
{
	return (padlock_use_rng || padlock_use_ace);
}

/* This stuff is needed if this ENGINE is being compiled into a self-contained
 * shared-library.
 */
#ifdef DYNAMIC_ENGINE
static int
padlock_bind_fn(ENGINE *e, const char *id)
{
	if (id && (strcmp(id, padlock_id) != 0)) {
		return 0;
	}

	if (!padlock_bind_helper(e))  {
		return 0;
	}

	return 1;
}

IMPLEMENT_DYNAMIC_CHECK_FN()
IMPLEMENT_DYNAMIC_BIND_FN (padlock_bind_fn)
#endif /* DYNAMIC_ENGINE */

/* ===== Here comes the "real" engine ===== */

#ifndef OPENSSL_NO_AES
/* Some AES-related constants */
#define AES_BLOCK_SIZE		16
#define AES_KEY_SIZE_128	16
#define AES_KEY_SIZE_192	24
#define AES_KEY_SIZE_256	32

/* Here we store the status information relevant to the 
   current context. */
/* BIG FAT WARNING:
 * 	Inline assembler in PADLOCK_XCRYPT_ASM()
 * 	depends on the order of items in this structure.
 * 	Don't blindly modify, reorder, etc!
 */
struct padlock_cipher_data
{
	unsigned char iv[AES_BLOCK_SIZE];	/* Initialization vector */
	union {	unsigned int pad[4];
		struct {
			int rounds:4;
			int dgst:1;	/* n/a in C3 */
			int align:1;	/* n/a in C3 */
			int ciphr:1;	/* n/a in C3 */
			unsigned int keygen:1;
			int interm:1;
			unsigned int encdec:1;
			int ksize:2;
		} b;
	} cword;		/* Control word */
	AES_KEY ks;		/* Encryption key */
};

/*
 * Essentially this variable belongs in thread local storage.
 * Having this variable global on the other hand can only cause
 * few bogus key reloads [if any at all on single-CPU system],
 * so we accept the penatly...
 */
static volatile struct padlock_cipher_data *padlock_saved_context;
#endif

/*
 * =======================================================
 * Inline assembler section(s).
 * =======================================================
 * Order of arguments is chosen to facilitate Windows port
 * using __fastcall calling convention. If you wish to add
 * more routines, keep in mind that first __fastcall
 * argument is passed in %ecx and second - in %edx.
 * =======================================================
 */
#if defined(__GNUC__) && __GNUC__>=2
/*
 * As for excessive "push %ebx"/"pop %ebx" found all over.
 * When generating position-independent code GCC won't let
 * us use "b" in assembler templates nor even respect "ebx"
 * in "clobber description." Therefore the trouble...
 */

/* Helper function - check if a CPUID instruction
   is available on this CPU */
static int
padlock_insn_cpuid_available(void)
{
	int result = -1;

	/* We're checking if the bit #21 of EFLAGS 
	   can be toggled. If yes = CPUID is available. */
	asm volatile (
		"pushf\n"
		"popl %%eax\n"
		"xorl $0x200000, %%eax\n"
		"movl %%eax, %%ecx\n"
		"andl $0x200000, %%ecx\n"
		"pushl %%eax\n"
		"popf\n"
		"pushf\n"
		"popl %%eax\n"
		"andl $0x200000, %%eax\n"
		"xorl %%eax, %%ecx\n"
		"movl %%ecx, %0\n"
		: "=r" (result) : : "eax", "ecx");
	
	return (result == 0);
}

/* Load supported features of the CPU to see if
   the PadLock is available. */
static int
padlock_available(void)
{
	char vendor_string[16];
	unsigned int eax, edx;

	/* First check if the CPUID instruction is available at all... */
	if (! padlock_insn_cpuid_available())
		return 0;

	/* Are we running on the Centaur (VIA) CPU? */
	eax = 0x00000000;
	vendor_string[12] = 0;
	asm volatile (
		"pushl	%%ebx\n"
		"cpuid\n"
		"movl	%%ebx,(%%edi)\n"
		"movl	%%edx,4(%%edi)\n"
		"movl	%%ecx,8(%%edi)\n"
		"popl	%%ebx"
		: "+a"(eax) : "D"(vendor_string) : "ecx", "edx");
	if (strcmp(vendor_string, "CentaurHauls") != 0)
		return 0;

	/* Check for Centaur Extended Feature Flags presence */
	eax = 0xC0000000;
	asm volatile ("pushl %%ebx; cpuid; popl	%%ebx"
		: "+a"(eax) : : "ecx", "edx");
	if (eax < 0xC0000001)
		return 0;

	/* Read the Centaur Extended Feature Flags */
	eax = 0xC0000001;
	asm volatile ("pushl %%ebx; cpuid; popl %%ebx"
		: "+a"(eax), "=d"(edx) : : "ecx");

	/* Fill up some flags */
	padlock_use_ace = ((edx & (0x3<<6)) == (0x3<<6));
	padlock_use_rng = ((edx & (0x3<<2)) == (0x3<<2));

	return padlock_use_ace + padlock_use_rng;
}

#ifndef OPENSSL_NO_AES
/* Our own htonl()/ntohl() */
static inline void
padlock_bswapl(AES_KEY *ks)
{
	size_t i = sizeof(ks->rd_key)/sizeof(ks->rd_key[0]);
	unsigned int *key = ks->rd_key;

	while (i--) {
		asm volatile ("bswapl %0" : "+r"(*key));
		key++;
	}
}
#endif

/* Force key reload from memory to the CPU microcode.
   Loading EFLAGS from the stack clears EFLAGS[30] 
   which does the trick. */
static inline void
padlock_reload_key(void)
{
	asm volatile ("pushfl; popfl");
}

#ifndef OPENSSL_NO_AES
/*
 * This is heuristic key context tracing. At first one
 * believes that one should use atomic swap instructions,
 * but it's not actually necessary. Point is that if
 * padlock_saved_context was changed by another thread
 * after we've read it and before we compare it with cdata,
 * our key *shall* be reloaded upon thread context switch
 * and we are therefore set in either case...
 */
static inline void
padlock_verify_context(struct padlock_cipher_data *cdata)
{
	asm volatile (
	"pushfl\n"
"	btl	$30,(%%esp)\n"
"	jnc	1f\n"
"	cmpl	%2,%1\n"
"	je	1f\n"
"	popfl\n"
"	subl	$4,%%esp\n"
"1:	addl	$4,%%esp\n"
"	movl	%2,%0"
	:"+m"(padlock_saved_context)
	: "r"(padlock_saved_context), "r"(cdata) : "cc");
}

/* Template for padlock_xcrypt_* modes */
/* BIG FAT WARNING: 
 * 	The offsets used with 'leal' instructions
 * 	describe items of the 'padlock_cipher_data'
 * 	structure.
 */
#define PADLOCK_XCRYPT_ASM(name,rep_xcrypt)	\
static inline void *name(size_t cnt,		\
	struct padlock_cipher_data *cdata,	\
	void *out, const void *inp) 		\
{	void *iv; 				\
	asm volatile ( "pushl	%%ebx\n"	\
		"	leal	16(%0),%%edx\n"	\
		"	leal	32(%0),%%ebx\n"	\
			rep_xcrypt "\n"		\
		"	popl	%%ebx"		\
		: "=a"(iv), "=c"(cnt), "=D"(out), "=S"(inp) \
		: "0"(cdata), "1"(cnt), "2"(out), "3"(inp)  \
		: "edx", "cc", "memory");	\
	return iv;				\
}

/* Generate all functions with appropriate opcodes */
PADLOCK_XCRYPT_ASM(padlock_xcrypt_ecb, ".byte 0xf3,0x0f,0xa7,0xc8")	/* rep xcryptecb */
PADLOCK_XCRYPT_ASM(padlock_xcrypt_cbc, ".byte 0xf3,0x0f,0xa7,0xd0")	/* rep xcryptcbc */
PADLOCK_XCRYPT_ASM(padlock_xcrypt_cfb, ".byte 0xf3,0x0f,0xa7,0xe0")	/* rep xcryptcfb */
PADLOCK_XCRYPT_ASM(padlock_xcrypt_ofb, ".byte 0xf3,0x0f,0xa7,0xe8")	/* rep xcryptofb */
#endif

/* The RNG call itself */
static inline unsigned int
padlock_xstore(void *addr, unsigned int edx_in)
{
	unsigned int eax_out;

	asm volatile (".byte 0x0f,0xa7,0xc0"	/* xstore */
	    : "=a"(eax_out),"=m"(*(unsigned *)addr)
	    : "D"(addr), "d" (edx_in)
	    );

	return eax_out;
}

/* Why not inline 'rep movsd'? I failed to find information on what
 * value in Direction Flag one can expect and consequently have to
 * apply "better-safe-than-sorry" approach and assume "undefined."
 * I could explicitly clear it and restore the original value upon
 * return from padlock_aes_cipher, but it's presumably too much
 * trouble for too little gain...
 *
 * In case you wonder 'rep xcrypt*' instructions above are *not*
 * affected by the Direction Flag and pointers advance toward
 * larger addresses unconditionally.
 */ 
static inline unsigned char *
padlock_memcpy(void *dst,const void *src,size_t n)
{
	long       *d=dst;
	const long *s=src;

	n /= sizeof(*d);
	do { *d++ = *s++; } while (--n);

	return dst;
}

#elif defined(_MSC_VER)
/*
 * Unlike GCC these are real functions. In order to minimize impact
 * on performance we adhere to __fastcall calling convention in
 * order to get two first arguments passed through %ecx and %edx.
 * Which kind of suits very well, as instructions in question use
 * both %ecx and %edx as input:-)
 */
#define REP_XCRYPT(code)		\
	_asm _emit 0xf3			\
	_asm _emit 0x0f _asm _emit 0xa7	\
	_asm _emit code

/* BIG FAT WARNING: 
 * 	The offsets used with 'lea' instructions
 * 	describe items of the 'padlock_cipher_data'
 * 	structure.
 */
#define PADLOCK_XCRYPT_ASM(name,code)	\
static void * __fastcall 		\
	name (size_t cnt, void *cdata,	\
	void *outp, const void *inp)	\
{	_asm	mov	eax,edx		\
	_asm	lea	edx,[eax+16]	\
	_asm	lea	ebx,[eax+32]	\
	_asm	mov	edi,outp	\
	_asm	mov	esi,inp		\
	REP_XCRYPT(code)		\
}

PADLOCK_XCRYPT_ASM(padlock_xcrypt_ecb,0xc8)
PADLOCK_XCRYPT_ASM(padlock_xcrypt_cbc,0xd0)
PADLOCK_XCRYPT_ASM(padlock_xcrypt_cfb,0xe0)
PADLOCK_XCRYPT_ASM(padlock_xcrypt_ofb,0xe8)

static int __fastcall
padlock_xstore(void *outp,unsigned int code)
{	_asm	mov	edi,ecx
	_asm _emit 0x0f _asm _emit 0xa7 _asm _emit 0xc0
}

static void __fastcall
padlock_reload_key(void)
{	_asm pushfd _asm popfd		}

static void __fastcall
padlock_verify_context(void *cdata)
{	_asm	{
		pushfd
		bt	DWORD PTR[esp],30
		jnc	skip
		cmp	ecx,padlock_saved_context
		je	skip
		popfd
		sub	esp,4
	skip:	add	esp,4
		mov	padlock_saved_context,ecx
		}
}

static int
padlock_available(void)
{	_asm	{
		pushfd
		pop	eax
		mov	ecx,eax
		xor	eax,1<<21
		push	eax
		popfd
		pushfd
		pop	eax
		xor	eax,ecx
		bt	eax,21
		jnc	noluck
		mov	eax,0
		cpuid
		xor	eax,eax
		cmp	ebx,'tneC'
		jne	noluck
		cmp	edx,'Hrua'
		jne	noluck
		cmp	ecx,'slua'
		jne	noluck
		mov	eax,0xC0000000
		cpuid
		mov	edx,eax
		xor	eax,eax
		cmp	edx,0xC0000001
		jb	noluck
		mov	eax,0xC0000001
		cpuid
		xor	eax,eax
		bt	edx,6
		jnc	skip_a
		bt	edx,7
		jnc	skip_a
		mov	padlock_use_ace,1
		inc	eax
	skip_a:	bt	edx,2
		jnc	skip_r
		bt	edx,3
		jnc	skip_r
		mov	padlock_use_rng,1
		inc	eax
	skip_r:
	noluck:
		}
}

static void __fastcall
padlock_bswapl(void *key)
{	_asm	{
		pushfd
		cld
		mov	esi,ecx
		mov	edi,ecx
		mov	ecx,60
	up:	lodsd
		bswap	eax
		stosd
		loop	up
		popfd
		}
}

/* MS actually specifies status of Direction Flag and compiler even
 * manages to compile following as 'rep movsd' all by itself...
 */
#define padlock_memcpy(o,i,n) ((unsigned char *)memcpy((o),(i),(n)&~3U))
#endif

/* ===== AES encryption/decryption ===== */
#ifndef OPENSSL_NO_AES

#if defined(NID_aes_128_cfb128) && ! defined (NID_aes_128_cfb)
#define NID_aes_128_cfb	NID_aes_128_cfb128
#endif

#if defined(NID_aes_128_ofb128) && ! defined (NID_aes_128_ofb)
#define NID_aes_128_ofb	NID_aes_128_ofb128
#endif

#if defined(NID_aes_192_cfb128) && ! defined (NID_aes_192_cfb)
#define NID_aes_192_cfb	NID_aes_192_cfb128
#endif

#if defined(NID_aes_192_ofb128) && ! defined (NID_aes_192_ofb)
#define NID_aes_192_ofb	NID_aes_192_ofb128
#endif

#if defined(NID_aes_256_cfb128) && ! defined (NID_aes_256_cfb)
#define NID_aes_256_cfb	NID_aes_256_cfb128
#endif

#if defined(NID_aes_256_ofb128) && ! defined (NID_aes_256_ofb)
#define NID_aes_256_ofb	NID_aes_256_ofb128
#endif

/* List of supported ciphers. */
static int padlock_cipher_nids[] = {
	NID_aes_128_ecb,
	NID_aes_128_cbc,
	NID_aes_128_cfb,
	NID_aes_128_ofb,

	NID_aes_192_ecb,
	NID_aes_192_cbc,
	NID_aes_192_cfb,
	NID_aes_192_ofb,

	NID_aes_256_ecb,
	NID_aes_256_cbc,
	NID_aes_256_cfb,
	NID_aes_256_ofb,
};
static int padlock_cipher_nids_num = (sizeof(padlock_cipher_nids)/
				      sizeof(padlock_cipher_nids[0]));

/* Function prototypes ... */
static int padlock_aes_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
				const unsigned char *iv, int enc);
static int padlock_aes_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
			      const unsigned char *in, size_t nbytes);

#define NEAREST_ALIGNED(ptr) ( (unsigned char *)(ptr) +		\
	( (0x10 - ((size_t)(ptr) & 0x0F)) & 0x0F )	)
#define ALIGNED_CIPHER_DATA(ctx) ((struct padlock_cipher_data *)\
	NEAREST_ALIGNED(ctx->cipher_data))

#define EVP_CIPHER_block_size_ECB	AES_BLOCK_SIZE
#define EVP_CIPHER_block_size_CBC	AES_BLOCK_SIZE
#define EVP_CIPHER_block_size_OFB	1
#define EVP_CIPHER_block_size_CFB	1

/* Declaring so many ciphers by hand would be a pain.
   Instead introduce a bit of preprocessor magic :-) */
#define	DECLARE_AES_EVP(ksize,lmode,umode)	\
static const EVP_CIPHER padlock_aes_##ksize##_##lmode = {	\
	NID_aes_##ksize##_##lmode,		\
	EVP_CIPHER_block_size_##umode,	\
	AES_KEY_SIZE_##ksize,		\
	AES_BLOCK_SIZE,			\
	0 | EVP_CIPH_##umode##_MODE,	\
	padlock_aes_init_key,		\
	padlock_aes_cipher,		\
	NULL,				\
	sizeof(struct padlock_cipher_data) + 16,	\
	EVP_CIPHER_set_asn1_iv,		\
	EVP_CIPHER_get_asn1_iv,		\
	NULL,				\
	NULL				\
}

DECLARE_AES_EVP(128,ecb,ECB);
DECLARE_AES_EVP(128,cbc,CBC);
DECLARE_AES_EVP(128,cfb,CFB);
DECLARE_AES_EVP(128,ofb,OFB);

DECLARE_AES_EVP(192,ecb,ECB);
DECLARE_AES_EVP(192,cbc,CBC);
DECLARE_AES_EVP(192,cfb,CFB);
DECLARE_AES_EVP(192,ofb,OFB);

DECLARE_AES_EVP(256,ecb,ECB);
DECLARE_AES_EVP(256,cbc,CBC);
DECLARE_AES_EVP(256,cfb,CFB);
DECLARE_AES_EVP(256,ofb,OFB);

static int
padlock_ciphers (ENGINE *e, const EVP_CIPHER **cipher, const int **nids, int nid)
{
	/* No specific cipher => return a list of supported nids ... */
	if (!cipher) {
		*nids = padlock_cipher_nids;
		return padlock_cipher_nids_num;
	}

	/* ... or the requested "cipher" otherwise */
	switch (nid) {
	  case NID_aes_128_ecb:
	    *cipher = &padlock_aes_128_ecb;
	    break;
	  case NID_aes_128_cbc:
	    *cipher = &padlock_aes_128_cbc;
	    break;
	  case NID_aes_128_cfb:
	    *cipher = &padlock_aes_128_cfb;
	    break;
	  case NID_aes_128_ofb:
	    *cipher = &padlock_aes_128_ofb;
	    break;

	  case NID_aes_192_ecb:
	    *cipher = &padlock_aes_192_ecb;
	    break;
	  case NID_aes_192_cbc:
	    *cipher = &padlock_aes_192_cbc;
	    break;
	  case NID_aes_192_cfb:
	    *cipher = &padlock_aes_192_cfb;
	    break;
	  case NID_aes_192_ofb:
	    *cipher = &padlock_aes_192_ofb;
	    break;

	  case NID_aes_256_ecb:
	    *cipher = &padlock_aes_256_ecb;
	    break;
	  case NID_aes_256_cbc:
	    *cipher = &padlock_aes_256_cbc;
	    break;
	  case NID_aes_256_cfb:
	    *cipher = &padlock_aes_256_cfb;
	    break;
	  case NID_aes_256_ofb:
	    *cipher = &padlock_aes_256_ofb;
	    break;

	  default:
	    /* Sorry, we don't support this NID */
	    *cipher = NULL;
	    return 0;
	}

	return 1;
}

/* Prepare the encryption key for PadLock usage */
static int
padlock_aes_init_key (EVP_CIPHER_CTX *ctx, const unsigned char *key,
		      const unsigned char *iv, int enc)
{
	struct padlock_cipher_data *cdata;
	int key_len = EVP_CIPHER_CTX_key_length(ctx) * 8;

	if (key==NULL) return 0;	/* ERROR */

	cdata = ALIGNED_CIPHER_DATA(ctx);
	memset(cdata, 0, sizeof(struct padlock_cipher_data));

	/* Prepare Control word. */
	if (EVP_CIPHER_CTX_mode(ctx) == EVP_CIPH_OFB_MODE)
		cdata->cword.b.encdec = 0;
	else
		cdata->cword.b.encdec = (ctx->encrypt == 0);
	cdata->cword.b.rounds = 10 + (key_len - 128) / 32;
	cdata->cword.b.ksize = (key_len - 128) / 64;

	switch(key_len) {
		case 128:
			/* PadLock can generate an extended key for
			   AES128 in hardware */
			memcpy(cdata->ks.rd_key, key, AES_KEY_SIZE_128);
			cdata->cword.b.keygen = 0;
			break;

		case 192:
		case 256:
			/* Generate an extended AES key in software.
			   Needed for AES192/AES256 */
			/* Well, the above applies to Stepping 8 CPUs
			   and is listed as hardware errata. They most
			   likely will fix it at some point and then
			   a check for stepping would be due here. */
			if (EVP_CIPHER_CTX_mode(ctx) == EVP_CIPH_CFB_MODE ||
			    EVP_CIPHER_CTX_mode(ctx) == EVP_CIPH_OFB_MODE ||
			    enc)
				AES_set_encrypt_key(key, key_len, &cdata->ks);
			else
				AES_set_decrypt_key(key, key_len, &cdata->ks);
#ifndef AES_ASM
			/* OpenSSL C functions use byte-swapped extended key. */
			padlock_bswapl(&cdata->ks);
#endif
			cdata->cword.b.keygen = 1;
			break;

		default:
			/* ERROR */
			return 0;
	}

	/*
	 * This is done to cover for cases when user reuses the
	 * context for new key. The catch is that if we don't do
	 * this, padlock_eas_cipher might proceed with old key...
	 */
	padlock_reload_key ();

	return 1;
}

/* 
 * Simplified version of padlock_aes_cipher() used when
 * 1) both input and output buffers are at aligned addresses.
 * or when
 * 2) running on a newer CPU that doesn't require aligned buffers.
 */
static int
padlock_aes_cipher_omnivorous(EVP_CIPHER_CTX *ctx, unsigned char *out_arg,
		const unsigned char *in_arg, size_t nbytes)
{
	struct padlock_cipher_data *cdata;
	void  *iv;

	cdata = ALIGNED_CIPHER_DATA(ctx);
	padlock_verify_context(cdata);

	switch (EVP_CIPHER_CTX_mode(ctx)) {
	case EVP_CIPH_ECB_MODE:
		padlock_xcrypt_ecb(nbytes/AES_BLOCK_SIZE, cdata, out_arg, in_arg);
		break;

	case EVP_CIPH_CBC_MODE:
		memcpy(cdata->iv, ctx->iv, AES_BLOCK_SIZE);
		iv = padlock_xcrypt_cbc(nbytes/AES_BLOCK_SIZE, cdata, out_arg, in_arg);
		memcpy(ctx->iv, iv, AES_BLOCK_SIZE);
		break;

	case EVP_CIPH_CFB_MODE:
		memcpy(cdata->iv, ctx->iv, AES_BLOCK_SIZE);
		iv = padlock_xcrypt_cfb(nbytes/AES_BLOCK_SIZE, cdata, out_arg, in_arg);
		memcpy(ctx->iv, iv, AES_BLOCK_SIZE);
		break;

	case EVP_CIPH_OFB_MODE:
		memcpy(cdata->iv, ctx->iv, AES_BLOCK_SIZE);
		padlock_xcrypt_ofb(nbytes/AES_BLOCK_SIZE, cdata, out_arg, in_arg);
		memcpy(ctx->iv, cdata->iv, AES_BLOCK_SIZE);
		break;

	default:
		return 0;
	}

	memset(cdata->iv, 0, AES_BLOCK_SIZE);

	return 1;
}

#ifndef  PADLOCK_CHUNK
# define PADLOCK_CHUNK	512	/* Must be a power of 2 larger than 16 */
#endif
#if PADLOCK_CHUNK<16 || PADLOCK_CHUNK&(PADLOCK_CHUNK-1)
# error "insane PADLOCK_CHUNK..."
#endif

/* Re-align the arguments to 16-Bytes boundaries and run the 
   encryption function itself. This function is not AES-specific. */
static int
padlock_aes_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out_arg,
		   const unsigned char *in_arg, size_t nbytes)
{
	struct padlock_cipher_data *cdata;
	const  void *inp;
	unsigned char  *out;
	void  *iv;
	int    inp_misaligned, out_misaligned, realign_in_loop;
	size_t chunk, allocated=0;

	/* ctx->num is maintained in byte-oriented modes,
	   such as CFB and OFB... */
	if ((chunk = ctx->num)) { /* borrow chunk variable */
		unsigned char *ivp=ctx->iv;

		switch (EVP_CIPHER_CTX_mode(ctx)) {
		case EVP_CIPH_CFB_MODE:
			if (chunk >= AES_BLOCK_SIZE)
				return 0; /* bogus value */

			if (ctx->encrypt)
				while (chunk<AES_BLOCK_SIZE && nbytes!=0) {
					ivp[chunk] = *(out_arg++) = *(in_arg++) ^ ivp[chunk];
					chunk++, nbytes--;
				}
			else	while (chunk<AES_BLOCK_SIZE && nbytes!=0) {
					unsigned char c = *(in_arg++);
					*(out_arg++) = c ^ ivp[chunk];
					ivp[chunk++] = c, nbytes--;
				}

			ctx->num = chunk%AES_BLOCK_SIZE;
			break;
		case EVP_CIPH_OFB_MODE:
			if (chunk >= AES_BLOCK_SIZE)
				return 0; /* bogus value */

			while (chunk<AES_BLOCK_SIZE && nbytes!=0) {
				*(out_arg++) = *(in_arg++) ^ ivp[chunk];
				chunk++, nbytes--;
			}

			ctx->num = chunk%AES_BLOCK_SIZE;
			break;
		}
	}

	if (nbytes == 0)
		return 1;
#if 0
	if (nbytes % AES_BLOCK_SIZE)
		return 0; /* are we expected to do tail processing? */
#else
	/* nbytes is always multiple of AES_BLOCK_SIZE in ECB and CBC
	   modes and arbitrary value in byte-oriented modes, such as
	   CFB and OFB... */
#endif

	/* VIA promises CPUs that won't require alignment in the future.
	   For now padlock_aes_align_required is initialized to 1 and
	   the condition is never met... */
	/* C7 core is capable to manage unaligned input in non-ECB[!]
	   mode, but performance penalties appear to be approximately
	   same as for software alignment below or ~3x. They promise to
	   improve it in the future, but for now we can just as well
	   pretend that it can only handle aligned input... */
	if (!padlock_aes_align_required && (nbytes%AES_BLOCK_SIZE)==0)
		return padlock_aes_cipher_omnivorous(ctx, out_arg, in_arg, nbytes);

	inp_misaligned = (((size_t)in_arg) & 0x0F);
	out_misaligned = (((size_t)out_arg) & 0x0F);

	/* Note that even if output is aligned and input not,
	 * I still prefer to loop instead of copy the whole
	 * input and then encrypt in one stroke. This is done
	 * in order to improve L1 cache utilization... */
	realign_in_loop = out_misaligned|inp_misaligned;

	if (!realign_in_loop && (nbytes%AES_BLOCK_SIZE)==0)
		return padlock_aes_cipher_omnivorous(ctx, out_arg, in_arg, nbytes);

	/* this takes one "if" out of the loops */
	chunk  = nbytes;
	chunk %= PADLOCK_CHUNK;
	if (chunk==0) chunk = PADLOCK_CHUNK;

	if (out_misaligned) {
		/* optmize for small input */
		allocated = (chunk<nbytes?PADLOCK_CHUNK:nbytes);
		out = alloca(0x10 + allocated);
		out = NEAREST_ALIGNED(out);
	}
	else
		out = out_arg;

	cdata = ALIGNED_CIPHER_DATA(ctx);
	padlock_verify_context(cdata);

	switch (EVP_CIPHER_CTX_mode(ctx)) {
	case EVP_CIPH_ECB_MODE:
		do	{
			if (inp_misaligned)
				inp = padlock_memcpy(out, in_arg, chunk);
			else
				inp = in_arg;
			in_arg += chunk;

			padlock_xcrypt_ecb(chunk/AES_BLOCK_SIZE, cdata, out, inp);

			if (out_misaligned)
				out_arg = padlock_memcpy(out_arg, out, chunk) + chunk;
			else
				out     = out_arg+=chunk;

			nbytes -= chunk;
			chunk   = PADLOCK_CHUNK;
		} while (nbytes);
		break;

	case EVP_CIPH_CBC_MODE:
		memcpy(cdata->iv, ctx->iv, AES_BLOCK_SIZE);
		goto cbc_shortcut;
		do	{
			if (iv != cdata->iv)
				memcpy(cdata->iv, iv, AES_BLOCK_SIZE);
			chunk = PADLOCK_CHUNK;
		cbc_shortcut: /* optimize for small input */
			if (inp_misaligned)
				inp = padlock_memcpy(out, in_arg, chunk);
			else
				inp = in_arg;
			in_arg += chunk;

			iv = padlock_xcrypt_cbc(chunk/AES_BLOCK_SIZE, cdata, out, inp);

			if (out_misaligned)
				out_arg = padlock_memcpy(out_arg, out, chunk) + chunk;
			else
				out     = out_arg+=chunk;

		} while (nbytes -= chunk);
		memcpy(ctx->iv, iv, AES_BLOCK_SIZE);
		break;

	case EVP_CIPH_CFB_MODE:
		memcpy (iv = cdata->iv, ctx->iv, AES_BLOCK_SIZE);
		chunk &= ~(AES_BLOCK_SIZE-1);
		if (chunk)	goto cfb_shortcut;
		else		goto cfb_skiploop;
		do	{
			if (iv != cdata->iv)
				memcpy(cdata->iv, iv, AES_BLOCK_SIZE);
			chunk = PADLOCK_CHUNK;
		cfb_shortcut: /* optimize for small input */
			if (inp_misaligned)
				inp = padlock_memcpy(out, in_arg, chunk);
			else
				inp = in_arg;
			in_arg += chunk;

			iv = padlock_xcrypt_cfb(chunk/AES_BLOCK_SIZE, cdata, out, inp);

			if (out_misaligned)
				out_arg = padlock_memcpy(out_arg, out, chunk) + chunk;
			else
				out     = out_arg+=chunk;

			nbytes -= chunk;
		} while (nbytes >= AES_BLOCK_SIZE);

		cfb_skiploop:
		if (nbytes) {
			unsigned char *ivp = cdata->iv;

			if (iv != ivp) {
				memcpy(ivp, iv, AES_BLOCK_SIZE);
				iv = ivp;
			}
			ctx->num = nbytes;
			if (cdata->cword.b.encdec) {
				cdata->cword.b.encdec=0;
				padlock_reload_key();
				padlock_xcrypt_ecb(1,cdata,ivp,ivp);
				cdata->cword.b.encdec=1;
				padlock_reload_key();
				while(nbytes) {
					unsigned char c = *(in_arg++);
					*(out_arg++) = c ^ *ivp;
					*(ivp++) = c, nbytes--;
				}
			}
			else {	padlock_reload_key();
				padlock_xcrypt_ecb(1,cdata,ivp,ivp);
				padlock_reload_key();
				while (nbytes) {
					*ivp = *(out_arg++) = *(in_arg++) ^ *ivp;
					ivp++, nbytes--;
				}
			}
		}

		memcpy(ctx->iv, iv, AES_BLOCK_SIZE);
		break;

	case EVP_CIPH_OFB_MODE:
		memcpy(cdata->iv, ctx->iv, AES_BLOCK_SIZE);
		chunk &= ~(AES_BLOCK_SIZE-1);
		if (chunk) do	{
			if (inp_misaligned)
				inp = padlock_memcpy(out, in_arg, chunk);
			else
				inp = in_arg;
			in_arg += chunk;

			padlock_xcrypt_ofb(chunk/AES_BLOCK_SIZE, cdata, out, inp);

			if (out_misaligned)
				out_arg = padlock_memcpy(out_arg, out, chunk) + chunk;
			else
				out     = out_arg+=chunk;

			nbytes -= chunk;
			chunk   = PADLOCK_CHUNK;
		} while (nbytes >= AES_BLOCK_SIZE);

		if (nbytes) {
			unsigned char *ivp = cdata->iv;

			ctx->num = nbytes;
			padlock_reload_key();	/* empirically found */
			padlock_xcrypt_ecb(1,cdata,ivp,ivp);
			padlock_reload_key();	/* empirically found */
			while (nbytes) {
				*(out_arg++) = *(in_arg++) ^ *ivp;
				ivp++, nbytes--;
			}
		}

		memcpy(ctx->iv, cdata->iv, AES_BLOCK_SIZE);
		break;

	default:
		return 0;
	}

	/* Clean the realign buffer if it was used */
	if (out_misaligned) {
		volatile unsigned long *p=(void *)out;
		size_t   n = allocated/sizeof(*p);
		while (n--) *p++=0;
	}

	memset(cdata->iv, 0, AES_BLOCK_SIZE);

	return 1;
}

#endif /* OPENSSL_NO_AES */

/* ===== Random Number Generator ===== */
/*
 * This code is not engaged. The reason is that it does not comply
 * with recommendations for VIA RNG usage for secure applications
 * (posted at http://www.via.com.tw/en/viac3/c3.jsp) nor does it
 * provide meaningful error control...
 */
/* Wrapper that provides an interface between the API and 
   the raw PadLock RNG */
static int
padlock_rand_bytes(unsigned char *output, int count)
{
	unsigned int eax, buf;

	while (count >= 8) {
		eax = padlock_xstore(output, 0);
		if (!(eax&(1<<6)))	return 0; /* RNG disabled */
		/* this ---vv--- covers DC bias, Raw Bits and String Filter */
		if (eax&(0x1F<<10))	return 0;
		if ((eax&0x1F)==0)	continue; /* no data, retry... */
		if ((eax&0x1F)!=8)	return 0; /* fatal failure...  */
		output += 8;
		count  -= 8;
	}
	while (count > 0) {
		eax = padlock_xstore(&buf, 3);
		if (!(eax&(1<<6)))	return 0; /* RNG disabled */
		/* this ---vv--- covers DC bias, Raw Bits and String Filter */
		if (eax&(0x1F<<10))	return 0;
		if ((eax&0x1F)==0)	continue; /* no data, retry... */
		if ((eax&0x1F)!=1)	return 0; /* fatal failure...  */
		*output++ = (unsigned char)buf;
		count--;
	}
	*(volatile unsigned int *)&buf=0;

	return 1;
}

/* Dummy but necessary function */
static int
padlock_rand_status(void)
{
	return 1;
}

/* Prepare structure for registration */
static RAND_METHOD padlock_rand = {
	NULL,			/* seed */
	padlock_rand_bytes,	/* bytes */
	NULL,			/* cleanup */
	NULL,			/* add */
	padlock_rand_bytes,	/* pseudorand */
	padlock_rand_status,	/* rand status */
};

#else  /* !COMPILE_HW_PADLOCK */
#ifndef OPENSSL_NO_DYNAMIC_ENGINE
OPENSSL_EXPORT
int bind_engine(ENGINE *e, const char *id, const dynamic_fns *fns);
OPENSSL_EXPORT
int bind_engine(ENGINE *e, const char *id, const dynamic_fns *fns) { return 0; }
IMPLEMENT_DYNAMIC_CHECK_FN()
#endif
#endif /* COMPILE_HW_PADLOCK */

#endif /* !OPENSSL_NO_HW_PADLOCK */
#endif /* !OPENSSL_NO_HW */
