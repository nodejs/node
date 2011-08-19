#include "openssl/bn.h"
#include "openssl/sha.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

/* Copyright (C) 2008 Ben Laurie (ben@links.org) */

/*
 * Implement J-PAKE, as described in
 * http://grouper.ieee.org/groups/1363/Research/contributions/hao-ryan-2008.pdf
 * 
 * With hints from http://www.cl.cam.ac.uk/~fh240/software/JPAKE2.java.
 */

static void showbn(const char *name, const BIGNUM *bn)
    {
    fputs(name, stdout);
    fputs(" = ", stdout);
    BN_print_fp(stdout, bn);
    putc('\n', stdout);
    }

typedef struct
    {
    BN_CTX *ctx;  // Perhaps not the best place for this?
    BIGNUM *p;
    BIGNUM *q;
    BIGNUM *g;
    } JPakeParameters;

static void JPakeParametersInit(JPakeParameters *params)
    {
    params->ctx = BN_CTX_new();

    // For now use p, q, g from Java sample code. Later, generate them.
    params->p = NULL;
    BN_hex2bn(&params->p, "fd7f53811d75122952df4a9c2eece4e7f611b7523cef4400c31e3f80b6512669455d402251fb593d8d58fabfc5f5ba30f6cb9b556cd7813b801d346ff26660b76b9950a5a49f9fe8047b1022c24fbba9d7feb7c61bf83b57e7c6a8a6150f04fb83f6d3c51ec3023554135a169132f675f3ae2b61d72aeff22203199dd14801c7");
    params->q = NULL;
    BN_hex2bn(&params->q, "9760508f15230bccb292b982a2eb840bf0581cf5");
    params->g = NULL;
    BN_hex2bn(&params->g, "f7e1a085d69b3ddecbbcab5c36b857b97994afbbfa3aea82f9574c0b3d0782675159578ebad4594fe67107108180b449167123e84c281613b7cf09328cc8a6e13c167a8b547c8d28e0a3ae1e2bb3a675916ea37f0bfa213562f1fb627a01243bcca4f1bea8519089a883dfe15ae59f06928b665e807b552564014c3bfecf492a");

    showbn("p", params->p);
    showbn("q", params->q);
    showbn("g", params->g);
    }

typedef struct
    {
    BIGNUM *gr;  // g^r (r random)
    BIGNUM *b;   // b = r - x*h, h=hash(g, g^r, g^x, name)
    } JPakeZKP;

typedef struct
    {
    BIGNUM *gx;       // g^x
    JPakeZKP zkpx;    // ZKP(x)
    } JPakeStep1;

typedef struct
    {
    BIGNUM *X;        // g^(xa + xc + xd) * xb * s
    JPakeZKP zkpxbs;  // ZKP(xb * s)
    } JPakeStep2;

typedef struct
    {
    const char *name;  // Must be unique
    int base;          // 1 for Alice, 3 for Bob. Only used for printing stuff.
    JPakeStep1 s1c;    // Alice's g^x3, ZKP(x3) or Bob's g^x1, ZKP(x1)
    JPakeStep1 s1d;    // Alice's g^x4, ZKP(x4) or Bob's g^x2, ZKP(x2)
    JPakeStep2 s2;     // Alice's A, ZKP(x2 * s) or Bob's B, ZKP(x4 * s)
    } JPakeUserPublic;

/*
 * The user structure. In the definition, (xa, xb, xc, xd) are Alice's
 * (x1, x2, x3, x4) or Bob's (x3, x4, x1, x2). If you see what I mean.
 */
typedef struct
    {
    JPakeUserPublic p;
    BIGNUM *secret;    // The shared secret
    BIGNUM *key;       // The calculated (shared) key
    BIGNUM *xa;        // Alice's x1 or Bob's x3
    BIGNUM *xb;        // Alice's x2 or Bob's x4
    } JPakeUser;

// Generate each party's random numbers. xa is in [0, q), xb is in [1, q).
static void genrand(JPakeUser *user, const JPakeParameters *params)
    {
    BIGNUM *qm1;

    // xa in [0, q)
    user->xa = BN_new();
    BN_rand_range(user->xa, params->q);

    // q-1
    qm1 = BN_new();
    BN_copy(qm1, params->q);
    BN_sub_word(qm1, 1);

    // ... and xb in [0, q-1)
    user->xb = BN_new();
    BN_rand_range(user->xb, qm1);
    // [1, q)
    BN_add_word(user->xb, 1);

    // cleanup
    BN_free(qm1);

    // Show
    printf("x%d", user->p.base);
    showbn("", user->xa);
    printf("x%d", user->p.base+1);
    showbn("", user->xb);
    }

static void hashlength(SHA_CTX *sha, size_t l)
    {
    unsigned char b[2];

    assert(l <= 0xffff);
    b[0] = l >> 8;
    b[1] = l&0xff;
    SHA1_Update(sha, b, 2);
    }

static void hashstring(SHA_CTX *sha, const char *string)
    {
    size_t l = strlen(string);

    hashlength(sha, l);
    SHA1_Update(sha, string, l);
    }

static void hashbn(SHA_CTX *sha, const BIGNUM *bn)
    {
    size_t l = BN_num_bytes(bn);
    unsigned char *bin = alloca(l);

    hashlength(sha, l);
    BN_bn2bin(bn, bin);
    SHA1_Update(sha, bin, l);
    }

// h=hash(g, g^r, g^x, name)
static void zkpHash(BIGNUM *h, const JPakeZKP *zkp, const BIGNUM *gx,
		    const JPakeUserPublic *from, const JPakeParameters *params)
    {
    unsigned char md[SHA_DIGEST_LENGTH];
    SHA_CTX sha;

    // XXX: hash should not allow moving of the boundaries - Java code
    // is flawed in this respect. Length encoding seems simplest.
    SHA1_Init(&sha);
    hashbn(&sha, params->g);
    hashbn(&sha, zkp->gr);
    hashbn(&sha, gx);
    hashstring(&sha, from->name);
    SHA1_Final(md, &sha);
    BN_bin2bn(md, SHA_DIGEST_LENGTH, h);
    }

// Prove knowledge of x
// Note that we don't send g^x because, as it happens, we've always
// sent it elsewhere. Also note that because of that, we could avoid
// calculating it here, but we don't, for clarity...
static void CreateZKP(JPakeZKP *zkp, const BIGNUM *x, const JPakeUser *us,
		      const BIGNUM *zkpg, const JPakeParameters *params,
		      int n, const char *suffix)
    {
    BIGNUM *r = BN_new();
    BIGNUM *gx = BN_new();
    BIGNUM *h = BN_new();
    BIGNUM *t = BN_new();

    // r in [0,q)
    // XXX: Java chooses r in [0, 2^160) - i.e. distribution not uniform
    BN_rand_range(r, params->q);
    // g^r
    zkp->gr = BN_new();
    BN_mod_exp(zkp->gr, zkpg, r, params->p, params->ctx);
    // g^x
    BN_mod_exp(gx, zkpg, x, params->p, params->ctx);

    // h=hash...
    zkpHash(h, zkp, gx, &us->p, params);
    
    // b = r - x*h
    BN_mod_mul(t, x, h, params->q, params->ctx);
    zkp->b = BN_new();
    BN_mod_sub(zkp->b, r, t, params->q, params->ctx);

    // show
    printf("  ZKP(x%d%s)\n", n, suffix);
    showbn("   zkpg", zkpg);
    showbn("    g^x", gx);
    showbn("    g^r", zkp->gr);
    showbn("      b", zkp->b);

    // cleanup
    BN_free(t);
    BN_free(h);
    BN_free(gx);
    BN_free(r);
    }

static int VerifyZKP(const JPakeZKP *zkp, BIGNUM *gx,
		     const JPakeUserPublic *them, const BIGNUM *zkpg,
		     const JPakeParameters *params, int n, const char *suffix)
    {
    BIGNUM *h = BN_new();
    BIGNUM *t1 = BN_new();
    BIGNUM *t2 = BN_new();
    BIGNUM *t3 = BN_new();
    int ret = 0;

    zkpHash(h, zkp, gx, them, params);

    // t1 = g^b
    BN_mod_exp(t1, zkpg, zkp->b, params->p, params->ctx);
    // t2 = (g^x)^h = g^{hx}
    BN_mod_exp(t2, gx, h, params->p, params->ctx);
    // t3 = t1 * t2 = g^{hx} * g^b = g^{hx+b} = g^r (allegedly)
    BN_mod_mul(t3, t1, t2, params->p, params->ctx);

    printf("  ZKP(x%d%s)\n", n, suffix);
    showbn("    zkpg", zkpg);
    showbn("    g^r'", t3);

    // verify t3 == g^r
    if(BN_cmp(t3, zkp->gr) == 0)
	ret = 1;

    // cleanup
    BN_free(t3);
    BN_free(t2);
    BN_free(t1);
    BN_free(h);

    if(ret)
	puts("    OK");
    else
	puts("    FAIL");

    return ret;
    }    

static void sendstep1_substep(JPakeStep1 *s1, const BIGNUM *x,
			      const JPakeUser *us,
			      const JPakeParameters *params, int n)
    {
    s1->gx = BN_new();
    BN_mod_exp(s1->gx, params->g, x, params->p, params->ctx);
    printf("  g^{x%d}", n);
    showbn("", s1->gx);

    CreateZKP(&s1->zkpx, x, us, params->g, params, n, "");
    }

static void sendstep1(const JPakeUser *us, JPakeUserPublic *them,
		      const JPakeParameters *params)
    {
    printf("\n%s sends %s:\n\n", us->p.name, them->name);

    // from's g^xa (which becomes to's g^xc) and ZKP(xa)
    sendstep1_substep(&them->s1c, us->xa, us, params, us->p.base);
    // from's g^xb (which becomes to's g^xd) and ZKP(xb)
    sendstep1_substep(&them->s1d, us->xb, us, params, us->p.base+1);
    }

static int verifystep1(const JPakeUser *us, const JPakeUserPublic *them,
		       const JPakeParameters *params)
    {
    printf("\n%s verifies %s:\n\n", us->p.name, them->name);

    // verify their ZKP(xc)
    if(!VerifyZKP(&us->p.s1c.zkpx, us->p.s1c.gx, them, params->g, params,
		  them->base, ""))
	return 0;

    // verify their ZKP(xd)
    if(!VerifyZKP(&us->p.s1d.zkpx, us->p.s1d.gx, them, params->g, params,
		  them->base+1, ""))
	return 0;

    // g^xd != 1
    printf("  g^{x%d} != 1: ", them->base+1);
    if(BN_is_one(us->p.s1d.gx))
	{
	puts("FAIL");
	return 0;
	}
    puts("OK");

    return 1;
    }

static void sendstep2(const JPakeUser *us, JPakeUserPublic *them,
		      const JPakeParameters *params)
    {
    BIGNUM *t1 = BN_new();
    BIGNUM *t2 = BN_new();

    printf("\n%s sends %s:\n\n", us->p.name, them->name);

    // X = g^{(xa + xc + xd) * xb * s}
    // t1 = g^xa
    BN_mod_exp(t1, params->g, us->xa, params->p, params->ctx);
    // t2 = t1 * g^{xc} = g^{xa} * g^{xc} = g^{xa + xc}
    BN_mod_mul(t2, t1, us->p.s1c.gx, params->p, params->ctx);
    // t1 = t2 * g^{xd} = g^{xa + xc + xd}
    BN_mod_mul(t1, t2, us->p.s1d.gx, params->p, params->ctx);
    // t2 = xb * s
    BN_mod_mul(t2, us->xb, us->secret, params->q, params->ctx);
    // X = t1^{t2} = t1^{xb * s} = g^{(xa + xc + xd) * xb * s}
    them->s2.X = BN_new();
    BN_mod_exp(them->s2.X, t1, t2, params->p, params->ctx);

    // Show
    printf("  g^{(x%d + x%d + x%d) * x%d * s)", us->p.base, them->base,
	   them->base+1, us->p.base+1);
    showbn("", them->s2.X);

    // ZKP(xb * s)
    // XXX: this is kinda funky, because we're using
    //
    // g' = g^{xa + xc + xd}
    //
    // as the generator, which means X is g'^{xb * s}
    CreateZKP(&them->s2.zkpxbs, t2, us, t1, params, us->p.base+1, " * s");

    // cleanup
    BN_free(t1);
    BN_free(t2);
    }

static int verifystep2(const JPakeUser *us, const JPakeUserPublic *them,
		       const JPakeParameters *params)
    {
    BIGNUM *t1 = BN_new();
    BIGNUM *t2 = BN_new();
    int ret = 0;

    printf("\n%s verifies %s:\n\n", us->p.name, them->name);

    // g' = g^{xc + xa + xb} [from our POV]
    // t1 = xa + xb
    BN_mod_add(t1, us->xa, us->xb, params->q, params->ctx);
    // t2 = g^{t1} = g^{xa+xb}
    BN_mod_exp(t2, params->g, t1, params->p, params->ctx);
    // t1 = g^{xc} * t2 = g^{xc + xa + xb}
    BN_mod_mul(t1, us->p.s1c.gx, t2, params->p, params->ctx);

    if(VerifyZKP(&us->p.s2.zkpxbs, us->p.s2.X, them, t1, params, them->base+1,
		  " * s"))
	ret = 1;

    // cleanup
    BN_free(t2);
    BN_free(t1);

    return ret;
    }

static void computekey(JPakeUser *us, const JPakeParameters *params)
    {
    BIGNUM *t1 = BN_new();
    BIGNUM *t2 = BN_new();
    BIGNUM *t3 = BN_new();

    printf("\n%s calculates the shared key:\n\n", us->p.name);

    // K = (X/g^{xb * xd * s})^{xb}
    //   = (g^{(xc + xa + xb) * xd * s - xb * xd *s})^{xb}
    //   = (g^{(xa + xc) * xd * s})^{xb}
    //   = g^{(xa + xc) * xb * xd * s}
    // [which is the same regardless of who calculates it]

    // t1 = (g^{xd})^{xb} = g^{xb * xd}
    BN_mod_exp(t1, us->p.s1d.gx, us->xb, params->p, params->ctx);
    // t2 = -s = q-s
    BN_sub(t2, params->q, us->secret);
    // t3 = t1^t2 = g^{-xb * xd * s}
    BN_mod_exp(t3, t1, t2, params->p, params->ctx);
    // t1 = X * t3 = X/g^{xb * xd * s}
    BN_mod_mul(t1, us->p.s2.X, t3, params->p, params->ctx);
    // K = t1^{xb}
    us->key = BN_new();
    BN_mod_exp(us->key, t1, us->xb, params->p, params->ctx);

    // show
    showbn("  K", us->key);

    // cleanup
    BN_free(t3);
    BN_free(t2);
    BN_free(t1);
    }

int main(int argc, char **argv)
    {
    JPakeParameters params;
    JPakeUser alice, bob;

    alice.p.name = "Alice";
    alice.p.base = 1;
    bob.p.name = "Bob";
    bob.p.base = 3;

    JPakeParametersInit(&params);

    // Shared secret
    alice.secret = BN_new();
    BN_rand(alice.secret, 32, -1, 0);
    bob.secret = alice.secret;
    showbn("secret", alice.secret);

    assert(BN_cmp(alice.secret, params.q) < 0);

    // Alice's x1, x2
    genrand(&alice, &params);

    // Bob's x3, x4
    genrand(&bob, &params);

    // Now send stuff to each other...
    sendstep1(&alice, &bob.p, &params);
    sendstep1(&bob, &alice.p, &params);

    // And verify what each other sent
    if(!verifystep1(&alice, &bob.p, &params))
	return 1;
    if(!verifystep1(&bob, &alice.p, &params))
	return 2;

    // Second send
    sendstep2(&alice, &bob.p, &params);
    sendstep2(&bob, &alice.p, &params);

    // And second verify
    if(!verifystep2(&alice, &bob.p, &params))
	return 3;
    if(!verifystep2(&bob, &alice.p, &params))
	return 4;

    // Compute common key
    computekey(&alice, &params);
    computekey(&bob, &params);

    // Confirm the common key is identical
    // XXX: if the two secrets are not the same, everything works up
    // to this point, so the only way to detect a failure is by the
    // difference in the calculated keys.
    // Since we're all the same code, just compare them directly. In a
    // real system, Alice sends Bob H(H(K)), Bob checks it, then sends
    // back H(K), which Alice checks, or something equivalent.
    puts("\nAlice and Bob check keys are the same:");
    if(BN_cmp(alice.key, bob.key) == 0)
	puts("  OK");
    else
	{
	puts("  FAIL");
	return 5;
	}

    return 0;
    }
