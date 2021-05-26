/*
 * Copyright 2007-2021 The OpenSSL Project Authors. All Rights Reserved.
 * Copyright Nokia 2007-2019
 * Copyright Siemens AG 2015-2019
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * A collection of test cases where check-format.pl should not report issues.
 * There are some known false positives, though, which are marked below.
 */

/*-
 * allow double space  in format-tagged multi-line comment
 */
int f(void) /*
             * trailing multi-line comment
             */
{
    if (ctx == NULL) { /* non-leading intra-line comment */
        if (/* comment after '(' */ pem_name != NULL /* comment before ')' */)
            /* entire-line comment indent usually like for the following line */
            return NULL; /* hanging indent also for this line after comment */
        /* leading comment has same indentation as normal code */ stmt;
        /* entire-line comment may have same indent as normal code */
    }

    for (;;)
        ;
    for (i = 0;;)
        ;
    for (i = 0; i < 1;)
        ;

#if X
    if (1) /* bad style: just part of control structure depends on #if */
#else
    if (2) /*@ resulting false positive */
#endif
        c; /*@ resulting false positive */

    if (1)
        if (2)
            c;
        else
            e;
    else
        f;
    do
        do
            2;
        while (1);
    while (2);

    if (1)
        f(a, b);
    do
        1; while (2); /*@ more than one stmt just to construct case */
    if (1)
        f(a, b);
    else
        do
            1;
        while (2);
    if (1)
        f(a, b);
    else do /*@ (non-brace) code before 'do' just to construct case */
             1;
        while (2);
    f1234(a,
          b); do /*@ (non-brace) code before 'do' just to construct case */
                  1;
    while (2);
    if (1)
        f(a,
          b); do /*@ (non-brace) code before 'do' just to construct case */
                  1;
    while (2);
    if (1)
        f(a, b);
    else
        do f(c, c); /*@ (non-brace) code after 'do' just to construct case */
        while (2);

    if (1)
        f(a, b);
    else
        return;
    if (1)
        f(a,
          b); else /*@ (non-brace) code before 'else' just to construct case */
        do
            1;
        while (2);

    if (1)
    { /*@ brace after 'if' not on same line just to construct case */
        c;
        d;
    }
    /* this comment is correctly indented if it refers to the following line */
    d;

    if (1) {
        2;
    } else /*@ no brace after 'else' just to construct case */
        3;
    do {
    } while (x);
    if (1) {
        2;
    } else {
        3;
    }
    if (4)
        5;
    else
        6;

    if (1) {
        if (2) {
        case MAC_TYPE_MAC:
            {
                EVP_MAC_CTX *new_mac_ctx;

                if (ctx->pkey == NULL)
                    return 0;
            }
            break;
        default:
            /* This should be dead code */
            return 0;
        }
    }
    if (expr_line1
        == expr_line2
            && expr_line3) {
        c1;
    } else {
        c;
        d;
    }
    if (expr_line1
        == expr_line2
            && expr_line3)
        hanging_stmt;
}

const OPTIONS passwd_options[] = {
    {"aixmd5", OPT_AIXMD5, '-', "AIX MD5-based password algorithm"},
#if !defined(OPENSSL_NO_DES) && !defined(OPENSSL_NO_DEPRECATED_3_0)
    {"crypt", OPT_CRYPT, '-', "Standard Unix password algorithm (default)"},
#endif
    OPT_R_OPTIONS,

    {NULL}
};

typedef * d(int)
    x;
typedef (int)
x;
typedef (int)*()
    x;
typedef *int *
x;
typedef OSSL_CMP_MSG *(*cmp_srv_process_cb_t)
    (OSSL_CMP_SRV_CTX *ctx, OSSL_CMP_MSG *msg)
    xx;
int f()
{
    c;
    if (1) {
        c;
    }
    c;
    if (1)
        if (2)
        { /*@ brace after 'if' not on same line just to construct case */
            c;
        }
    e;
    const usign = {
                   0xDF,
                   {
                    dd
                   },
                   dd
    };
    const unsign = {
                    0xDF, {
                           dd
                    },
                    dd
    };
}
const unsigned char trans_id[OSSL_CMP_TRANSACTIONID_LENGTH] = {
                                                               0xDF,
};
const unsigned char trans_id[OSSL_CMP_TRANSACTIONID_LENGTH] =
    {
     0xDF,
    };
typedef
int
a;

typedef
struct
{
    int a;
} b;
typedef enum {
              w = 0
} e_type;
typedef struct {
    enum {
          w = 0
    } e_type;
    enum {
          w = 0
    } e_type;
} e;
struct s_type {
    enum e_type {
                 w = 0
    };
};
struct s_type
{
    enum e_type {
                 w = 0
    };
    enum e2_type {
                  w = 0
    };
};

#define X  1          + 1
#define Y  /* .. */ 2 + 2
#define Z  3          + 3 * (*a++)

static varref cmp_vars[] = { /* comment.  comment?  comment!  */
    {&opt_config}, {&opt_section},

    {&opt_server}, {&opt_proxy}, {&opt_path},
};

#define SWITCH(x)                               \
    switch (x) {                                \
    case 0:                                     \
        break;                                  \
    default:                                    \
        break;                                  \
    }

#define DEFINE_SET_GET_BASE_TEST(PREFIX, SETN, GETN, DUP, FIELD, TYPE, ERR, \
                                 DEFAULT, NEW, FREE) \
    static int execute_CTX_##SETN##_##GETN##_##FIELD( \
                                                     TEST_FIXTURE *fixture) \
    { \
        CTX *ctx = fixture->ctx; \
        int (*set_fn)(CTX *ctx, TYPE) = \
            (int (*)(CTX *ctx, TYPE))PREFIX##_##SETN##_##FIELD; \
        /* comment */ \
    }

/* 'struct' in function header */
static int f(struct pem_pass_data *pass_data)
{
    if (pass_data == NULL)
        return 0;
}

static void *fun(void)
{
    if (pem_name != NULL)
        /* comment */
        return NULL;

label0:
 label1: /* allow special indent 1 for label at outermost level in body */
    do {
    label2:
        size_t available_len, data_len;
        const char *curr = txt, *next = txt;
        char *tmp;

        {
        label3:
        }
    } while (1);

    char *intraline_string_with_comment_delimiters_and_dbl_space = "1  /*1";
    char *multiline_string_with_comment_delimiters_and_dbl_space = "1  /*1\
2222222\'22222222222222222\"222222222" "33333  /*3333333333" "44  /*44444444444\
55555555555555\
6666";
}

ASN1_CHOICE(OSSL_CRMF_POPO) = {
    ASN1_IMP(OSSL_CRMF_POPO, value.raVerified, ASN1_NULL, 0),
    ASN1_EXP(OSSL_CRMF_POPO, value.keyAgreement, OSSL_CRMF_POPOPRIVKEY, 3)
} ASN1_CHOICE_END(OSSL_CRMF_POPO)
IMPLEMENT_ASN1_FUNCTIONS(OSSL_CRMF_POPO)

ASN1_ADB(OSSL_CRMF_ATTRIBUTETYPEANDVALUE) = {
    ADB_ENTRY(NID_id_regCtrl_regToken,
              ASN1_SIMPLE(OSSL_CRMF_ATTRIBUTETYPEANDVALUE,
                          value.regToken, ASN1_UTF8STRING)),
} ASN1_ADB_END(OSSL_CRMF_ATTRIBUTETYPEANDVALUE, 0, type, 0,
               &attributetypeandvalue_default_tt, NULL);

ASN1_ITEM_TEMPLATE(OSSL_CRMF_MSGS) =
    ASN1_EX_TEMPLATE_TYPE(ASN1_TFLG_SEQUENCE_OF, 0,
                          OSSL_CRMF_MSGS, OSSL_CRMF_MSG)
ASN1_ITEM_TEMPLATE_END(OSSL_CRMF_MSGS)

void f_looong_body_200()
{ /* function body length up to 200 lines accepted */
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
}

void f_looong_body_201()
{ /* function body length > 200 lines, but LONG BODY marker present */
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
    ;
}
