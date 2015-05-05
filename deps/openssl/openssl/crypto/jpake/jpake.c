#include "jpake.h"

#include <openssl/crypto.h>
#include <openssl/sha.h>
#include <openssl/err.h>
#include <memory.h>
#include <string.h>

/*
 * In the definition, (xa, xb, xc, xd) are Alice's (x1, x2, x3, x4) or
 * Bob's (x3, x4, x1, x2). If you see what I mean.
 */

typedef struct {
    char *name;                 /* Must be unique */
    char *peer_name;
    BIGNUM *p;
    BIGNUM *g;
    BIGNUM *q;
    BIGNUM *gxc;                /* Alice's g^{x3} or Bob's g^{x1} */
    BIGNUM *gxd;                /* Alice's g^{x4} or Bob's g^{x2} */
} JPAKE_CTX_PUBLIC;

struct JPAKE_CTX {
    JPAKE_CTX_PUBLIC p;
    BIGNUM *secret;             /* The shared secret */
    BN_CTX *ctx;
    BIGNUM *xa;                 /* Alice's x1 or Bob's x3 */
    BIGNUM *xb;                 /* Alice's x2 or Bob's x4 */
    BIGNUM *key;                /* The calculated (shared) key */
};

static void JPAKE_ZKP_init(JPAKE_ZKP *zkp)
{
    zkp->gr = BN_new();
    zkp->b = BN_new();
}

static void JPAKE_ZKP_release(JPAKE_ZKP *zkp)
{
    BN_free(zkp->b);
    BN_free(zkp->gr);
}

/* Two birds with one stone - make the global name as expected */
#define JPAKE_STEP_PART_init    JPAKE_STEP2_init
#define JPAKE_STEP_PART_release JPAKE_STEP2_release

void JPAKE_STEP_PART_init(JPAKE_STEP_PART *p)
{
    p->gx = BN_new();
    JPAKE_ZKP_init(&p->zkpx);
}

void JPAKE_STEP_PART_release(JPAKE_STEP_PART *p)
{
    JPAKE_ZKP_release(&p->zkpx);
    BN_free(p->gx);
}

void JPAKE_STEP1_init(JPAKE_STEP1 *s1)
{
    JPAKE_STEP_PART_init(&s1->p1);
    JPAKE_STEP_PART_init(&s1->p2);
}

void JPAKE_STEP1_release(JPAKE_STEP1 *s1)
{
    JPAKE_STEP_PART_release(&s1->p2);
    JPAKE_STEP_PART_release(&s1->p1);
}

static void JPAKE_CTX_init(JPAKE_CTX *ctx, const char *name,
                           const char *peer_name, const BIGNUM *p,
                           const BIGNUM *g, const BIGNUM *q,
                           const BIGNUM *secret)
{
    ctx->p.name = OPENSSL_strdup(name);
    ctx->p.peer_name = OPENSSL_strdup(peer_name);
    ctx->p.p = BN_dup(p);
    ctx->p.g = BN_dup(g);
    ctx->p.q = BN_dup(q);
    ctx->secret = BN_dup(secret);

    ctx->p.gxc = BN_new();
    ctx->p.gxd = BN_new();

    ctx->xa = BN_new();
    ctx->xb = BN_new();
    ctx->key = BN_new();
    ctx->ctx = BN_CTX_new();
}

static void JPAKE_CTX_release(JPAKE_CTX *ctx)
{
    BN_CTX_free(ctx->ctx);
    BN_clear_free(ctx->key);
    BN_clear_free(ctx->xb);
    BN_clear_free(ctx->xa);

    BN_free(ctx->p.gxd);
    BN_free(ctx->p.gxc);

    BN_clear_free(ctx->secret);
    BN_free(ctx->p.q);
    BN_free(ctx->p.g);
    BN_free(ctx->p.p);
    OPENSSL_free(ctx->p.peer_name);
    OPENSSL_free(ctx->p.name);

    memset(ctx, '\0', sizeof *ctx);
}

JPAKE_CTX *JPAKE_CTX_new(const char *name, const char *peer_name,
                         const BIGNUM *p, const BIGNUM *g, const BIGNUM *q,
                         const BIGNUM *secret)
{
    JPAKE_CTX *ctx = OPENSSL_malloc(sizeof *ctx);

    JPAKE_CTX_init(ctx, name, peer_name, p, g, q, secret);

    return ctx;
}

void JPAKE_CTX_free(JPAKE_CTX *ctx)
{
    JPAKE_CTX_release(ctx);
    OPENSSL_free(ctx);
}

static void hashlength(SHA_CTX *sha, size_t l)
{
    unsigned char b[2];

    OPENSSL_assert(l <= 0xffff);
    b[0] = l >> 8;
    b[1] = l & 0xff;
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
    unsigned char *bin = OPENSSL_malloc(l);

    hashlength(sha, l);
    BN_bn2bin(bn, bin);
    SHA1_Update(sha, bin, l);
    OPENSSL_free(bin);
}

/* h=hash(g, g^r, g^x, name) */
static void zkp_hash(BIGNUM *h, const BIGNUM *zkpg, const JPAKE_STEP_PART *p,
                     const char *proof_name)
{
    unsigned char md[SHA_DIGEST_LENGTH];
    SHA_CTX sha;

    /*
     * XXX: hash should not allow moving of the boundaries - Java code
     * is flawed in this respect. Length encoding seems simplest.
     */
    SHA1_Init(&sha);
    hashbn(&sha, zkpg);
    OPENSSL_assert(!BN_is_zero(p->zkpx.gr));
    hashbn(&sha, p->zkpx.gr);
    hashbn(&sha, p->gx);
    hashstring(&sha, proof_name);
    SHA1_Final(md, &sha);
    BN_bin2bn(md, SHA_DIGEST_LENGTH, h);
}

/*
 * Prove knowledge of x
 * Note that p->gx has already been calculated
 */
static void generate_zkp(JPAKE_STEP_PART *p, const BIGNUM *x,
                         const BIGNUM *zkpg, JPAKE_CTX *ctx)
{
    BIGNUM *r = BN_new();
    BIGNUM *h = BN_new();
    BIGNUM *t = BN_new();

   /*-
    * r in [0,q)
    * XXX: Java chooses r in [0, 2^160) - i.e. distribution not uniform
    */
    BN_rand_range(r, ctx->p.q);
    /* g^r */
    BN_mod_exp(p->zkpx.gr, zkpg, r, ctx->p.p, ctx->ctx);

    /* h=hash... */
    zkp_hash(h, zkpg, p, ctx->p.name);

    /* b = r - x*h */
    BN_mod_mul(t, x, h, ctx->p.q, ctx->ctx);
    BN_mod_sub(p->zkpx.b, r, t, ctx->p.q, ctx->ctx);

    /* cleanup */
    BN_free(t);
    BN_free(h);
    BN_free(r);
}

static int verify_zkp(const JPAKE_STEP_PART *p, const BIGNUM *zkpg,
                      JPAKE_CTX *ctx)
{
    BIGNUM *h = BN_new();
    BIGNUM *t1 = BN_new();
    BIGNUM *t2 = BN_new();
    BIGNUM *t3 = BN_new();
    int ret = 0;

    zkp_hash(h, zkpg, p, ctx->p.peer_name);

    /* t1 = g^b */
    BN_mod_exp(t1, zkpg, p->zkpx.b, ctx->p.p, ctx->ctx);
    /* t2 = (g^x)^h = g^{hx} */
    BN_mod_exp(t2, p->gx, h, ctx->p.p, ctx->ctx);
    /* t3 = t1 * t2 = g^{hx} * g^b = g^{hx+b} = g^r (allegedly) */
    BN_mod_mul(t3, t1, t2, ctx->p.p, ctx->ctx);

    /* verify t3 == g^r */
    if (BN_cmp(t3, p->zkpx.gr) == 0)
        ret = 1;
    else
        JPAKEerr(JPAKE_F_VERIFY_ZKP, JPAKE_R_ZKP_VERIFY_FAILED);

    /* cleanup */
    BN_free(t3);
    BN_free(t2);
    BN_free(t1);
    BN_free(h);

    return ret;
}

static void generate_step_part(JPAKE_STEP_PART *p, const BIGNUM *x,
                               const BIGNUM *g, JPAKE_CTX *ctx)
{
    BN_mod_exp(p->gx, g, x, ctx->p.p, ctx->ctx);
    generate_zkp(p, x, g, ctx);
}

/* Generate each party's random numbers. xa is in [0, q), xb is in [1, q). */
static void genrand(JPAKE_CTX *ctx)
{
    BIGNUM *qm1;

    /* xa in [0, q) */
    BN_rand_range(ctx->xa, ctx->p.q);

    /* q-1 */
    qm1 = BN_new();
    BN_copy(qm1, ctx->p.q);
    BN_sub_word(qm1, 1);

    /* ... and xb in [0, q-1) */
    BN_rand_range(ctx->xb, qm1);
    /* [1, q) */
    BN_add_word(ctx->xb, 1);

    /* cleanup */
    BN_free(qm1);
}

int JPAKE_STEP1_generate(JPAKE_STEP1 *send, JPAKE_CTX *ctx)
{
    genrand(ctx);
    generate_step_part(&send->p1, ctx->xa, ctx->p.g, ctx);
    generate_step_part(&send->p2, ctx->xb, ctx->p.g, ctx);

    return 1;
}

/* g^x is a legal value */
static int is_legal(const BIGNUM *gx, const JPAKE_CTX *ctx)
{
    BIGNUM *t;
    int res;

    if (BN_is_negative(gx) || BN_is_zero(gx) || BN_cmp(gx, ctx->p.p) >= 0)
        return 0;

    t = BN_new();
    BN_mod_exp(t, gx, ctx->p.q, ctx->p.p, ctx->ctx);
    res = BN_is_one(t);
    BN_free(t);

    return res;
}

int JPAKE_STEP1_process(JPAKE_CTX *ctx, const JPAKE_STEP1 *received)
{
    if (!is_legal(received->p1.gx, ctx)) {
        JPAKEerr(JPAKE_F_JPAKE_STEP1_PROCESS,
                 JPAKE_R_G_TO_THE_X3_IS_NOT_LEGAL);
        return 0;
    }

    if (!is_legal(received->p2.gx, ctx)) {
        JPAKEerr(JPAKE_F_JPAKE_STEP1_PROCESS,
                 JPAKE_R_G_TO_THE_X4_IS_NOT_LEGAL);
        return 0;
    }

    /* verify their ZKP(xc) */
    if (!verify_zkp(&received->p1, ctx->p.g, ctx)) {
        JPAKEerr(JPAKE_F_JPAKE_STEP1_PROCESS, JPAKE_R_VERIFY_X3_FAILED);
        return 0;
    }

    /* verify their ZKP(xd) */
    if (!verify_zkp(&received->p2, ctx->p.g, ctx)) {
        JPAKEerr(JPAKE_F_JPAKE_STEP1_PROCESS, JPAKE_R_VERIFY_X4_FAILED);
        return 0;
    }

    /* g^xd != 1 */
    if (BN_is_one(received->p2.gx)) {
        JPAKEerr(JPAKE_F_JPAKE_STEP1_PROCESS, JPAKE_R_G_TO_THE_X4_IS_ONE);
        return 0;
    }

    /* Save the bits we need for later */
    BN_copy(ctx->p.gxc, received->p1.gx);
    BN_copy(ctx->p.gxd, received->p2.gx);

    return 1;
}

int JPAKE_STEP2_generate(JPAKE_STEP2 *send, JPAKE_CTX *ctx)
{
    BIGNUM *t1 = BN_new();
    BIGNUM *t2 = BN_new();

   /*-
    * X = g^{(xa + xc + xd) * xb * s}
    * t1 = g^xa
    */
    BN_mod_exp(t1, ctx->p.g, ctx->xa, ctx->p.p, ctx->ctx);
    /* t2 = t1 * g^{xc} = g^{xa} * g^{xc} = g^{xa + xc} */
    BN_mod_mul(t2, t1, ctx->p.gxc, ctx->p.p, ctx->ctx);
    /* t1 = t2 * g^{xd} = g^{xa + xc + xd} */
    BN_mod_mul(t1, t2, ctx->p.gxd, ctx->p.p, ctx->ctx);
    /* t2 = xb * s */
    BN_mod_mul(t2, ctx->xb, ctx->secret, ctx->p.q, ctx->ctx);

   /*-
    * ZKP(xb * s)
    * XXX: this is kinda funky, because we're using
    *
    * g' = g^{xa + xc + xd}
    *
    * as the generator, which means X is g'^{xb * s}
    * X = t1^{t2} = t1^{xb * s} = g^{(xa + xc + xd) * xb * s}
    */
    generate_step_part(send, t2, t1, ctx);

    /* cleanup */
    BN_free(t1);
    BN_free(t2);

    return 1;
}

/* gx = g^{xc + xa + xb} * xd * s */
static int compute_key(JPAKE_CTX *ctx, const BIGNUM *gx)
{
    BIGNUM *t1 = BN_new();
    BIGNUM *t2 = BN_new();
    BIGNUM *t3 = BN_new();

   /*-
    * K = (gx/g^{xb * xd * s})^{xb}
    *   = (g^{(xc + xa + xb) * xd * s - xb * xd *s})^{xb}
    *   = (g^{(xa + xc) * xd * s})^{xb}
    *   = g^{(xa + xc) * xb * xd * s}
    * [which is the same regardless of who calculates it]
    */

    /* t1 = (g^{xd})^{xb} = g^{xb * xd} */
    BN_mod_exp(t1, ctx->p.gxd, ctx->xb, ctx->p.p, ctx->ctx);
    /* t2 = -s = q-s */
    BN_sub(t2, ctx->p.q, ctx->secret);
    /* t3 = t1^t2 = g^{-xb * xd * s} */
    BN_mod_exp(t3, t1, t2, ctx->p.p, ctx->ctx);
    /* t1 = gx * t3 = X/g^{xb * xd * s} */
    BN_mod_mul(t1, gx, t3, ctx->p.p, ctx->ctx);
    /* K = t1^{xb} */
    BN_mod_exp(ctx->key, t1, ctx->xb, ctx->p.p, ctx->ctx);

    /* cleanup */
    BN_free(t3);
    BN_free(t2);
    BN_free(t1);

    return 1;
}

int JPAKE_STEP2_process(JPAKE_CTX *ctx, const JPAKE_STEP2 *received)
{
    BIGNUM *t1 = BN_new();
    BIGNUM *t2 = BN_new();
    int ret = 0;

   /*-
    * g' = g^{xc + xa + xb} [from our POV]
    * t1 = xa + xb
    */
    BN_mod_add(t1, ctx->xa, ctx->xb, ctx->p.q, ctx->ctx);
    /* t2 = g^{t1} = g^{xa+xb} */
    BN_mod_exp(t2, ctx->p.g, t1, ctx->p.p, ctx->ctx);
    /* t1 = g^{xc} * t2 = g^{xc + xa + xb} */
    BN_mod_mul(t1, ctx->p.gxc, t2, ctx->p.p, ctx->ctx);

    if (verify_zkp(received, t1, ctx))
        ret = 1;
    else
        JPAKEerr(JPAKE_F_JPAKE_STEP2_PROCESS, JPAKE_R_VERIFY_B_FAILED);

    compute_key(ctx, received->gx);

    /* cleanup */
    BN_free(t2);
    BN_free(t1);

    return ret;
}

static void quickhashbn(unsigned char *md, const BIGNUM *bn)
{
    SHA_CTX sha;

    SHA1_Init(&sha);
    hashbn(&sha, bn);
    SHA1_Final(md, &sha);
}

void JPAKE_STEP3A_init(JPAKE_STEP3A *s3a)
{
}

int JPAKE_STEP3A_generate(JPAKE_STEP3A *send, JPAKE_CTX *ctx)
{
    quickhashbn(send->hhk, ctx->key);
    SHA1(send->hhk, sizeof send->hhk, send->hhk);

    return 1;
}

int JPAKE_STEP3A_process(JPAKE_CTX *ctx, const JPAKE_STEP3A *received)
{
    unsigned char hhk[SHA_DIGEST_LENGTH];

    quickhashbn(hhk, ctx->key);
    SHA1(hhk, sizeof hhk, hhk);
    if (memcmp(hhk, received->hhk, sizeof hhk)) {
        JPAKEerr(JPAKE_F_JPAKE_STEP3A_PROCESS,
                 JPAKE_R_HASH_OF_HASH_OF_KEY_MISMATCH);
        return 0;
    }
    return 1;
}

void JPAKE_STEP3A_release(JPAKE_STEP3A *s3a)
{
}

void JPAKE_STEP3B_init(JPAKE_STEP3B *s3b)
{
}

int JPAKE_STEP3B_generate(JPAKE_STEP3B *send, JPAKE_CTX *ctx)
{
    quickhashbn(send->hk, ctx->key);

    return 1;
}

int JPAKE_STEP3B_process(JPAKE_CTX *ctx, const JPAKE_STEP3B *received)
{
    unsigned char hk[SHA_DIGEST_LENGTH];

    quickhashbn(hk, ctx->key);
    if (memcmp(hk, received->hk, sizeof hk)) {
        JPAKEerr(JPAKE_F_JPAKE_STEP3B_PROCESS, JPAKE_R_HASH_OF_KEY_MISMATCH);
        return 0;
    }
    return 1;
}

void JPAKE_STEP3B_release(JPAKE_STEP3B *s3b)
{
}

const BIGNUM *JPAKE_get_shared_key(JPAKE_CTX *ctx)
{
    return ctx->key;
}
