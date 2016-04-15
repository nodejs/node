#undef NDEBUG
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include <openssl/opensslv.h>
#include <openssl/opensslconf.h>
#include <openssl/crypto.h>
#include <openssl/objects.h>
#include <openssl/engine.h>
#include <openssl/dso.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/bn.h>

#include <string>

#ifndef ENGINE_CMD_BASE
#   error did not get engine.h
#endif

#define TEST_ENGINE_ID      "testengine"
#define TEST_ENGINE_NAME    "dummy test engine"

#define AGENT_KEY           "test/fixtures/keys/agent1-key.pem"
#define AGENT_CERT          "test/fixtures/keys/agent1-cert.pem"

static int engineInit(ENGINE* engine)
{
    printf("engineInit\n");
    return 1;
}

static int engineFinish(ENGINE* engine)
{
    printf("engineFinish");
    return 1;
}

static int engineDestroy(ENGINE* engine)
{
    printf("engineDestroy");
    return 1;
}

//static int engineCtrl(ENGINE* engine, int cmd, long i, void *p, void (*f) ())
//{
//    return 1;
//}

//static EVP_PKEY* engineLoadPrivateKey(ENGINE* engine, const char* s_key_id, UI_METHOD* ui_method, void* cb_data)
//{
//    return NULL;
//}

//static EVP_PKEY* engineLoadPublicKey(ENGINE* engine, const char* s_key_id, UI_METHOD* ui_method, void* cb_data)
//{
//    return NULL;
//}

static std::string loadFile(const char *filename)
{
    std::string ret;
    FILE *fp = fopen(filename, "r");
    if( fp==NULL )
    {
        printf("Could not open file '%s'\n", filename);
        return ret;
    }

    char buf[1024];
    while( true )
    {
        size_t r = fread(buf, 1, 1024, fp);
        if( r>0 )
        {
            ret.append(buf, r);
        }
        if( r<1024 )
        {
            break;
        }
    }

    fclose(fp);
    return ret;
}


static int engineLoadSSLClientCert(ENGINE* engine, SSL *ssl, STACK_OF(X509_NAME) *ca_dn, X509 **ppcert, EVP_PKEY **ppkey, STACK_OF(X509) **pother, UI_METHOD *ui_method, void *callback_data)
{
    printf("engineLoadSSLClientCert\n");

    if( ppcert )
    {
        std::string cert = loadFile(AGENT_CERT);
        if( cert.size()==0 )
        {
            return 0;
        }

        BIO *bio = BIO_new_mem_buf((void*)cert.data(), (int)cert.size());
        *ppcert = PEM_read_bio_X509(bio, NULL, NULL, NULL);
        BIO_vfree(bio);
        if( *ppcert==NULL )
        {
            printf("Could not read certificate\n");
            return 0;
        }
    }

    if( ppkey )
    {
        std::string key = loadFile(AGENT_KEY);
        if( key.size()==0 )
        {
            return 0;
        }

        BIO *bio = BIO_new_mem_buf((void*)key.data(), (int)key.size());
        *ppkey = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
        BIO_vfree(bio);
        if( *ppkey==NULL )
        {
            printf("Could not read private key\n");
            return 0;
        }
    }

    return 1;
}

/*
static int rsaEncrypt(int flen, const unsigned char *from, unsigned char *to, RSA *rsa, int padding)
{
    return 0;
}

static int rsaDecrypt(int flen, const unsigned char *from, unsigned char *to, RSA *rsa, int padding)
{
    return 0;
}

static int rsaSign(int type, const unsigned char *m, unsigned int m_length, unsigned char *sigret, unsigned int *siglen, const RSA *rsa)
{
    return 0;
}

static int rsaVerify(int type, const unsigned char *m, unsigned int m_length, const unsigned char *sigbuf, unsigned int siglen, const RSA *rsa)
{
    return 0;
}

static void rsaExDataFree(void *parent, void *ptr, CRYPTO_EX_DATA *ad, int idx, long argl, void *argp)
{
}

static int rsaExDataDup(CRYPTO_EX_DATA *to, CRYPTO_EX_DATA *from, void *from_d, int idx, long argl, void *argp)
{
    return 0;
}

static RSA_METHOD rsaMethods;
static RSA_METHOD* engineRSAMethods()
{
    rsaMethods.rsa_priv_enc = rsaEncrypt;
    rsaMethods.rsa_priv_dec = rsaDecrypt;
    rsaMethods.rsa_sign     = rsaSign;
    rsaMethods.rsa_verify   = rsaVerify;
//  rsaMethods.finish       = rsaFinish;
    rsaMethods.flags       |= RSA_FLAG_SIGN_VER;
    return &rsaMethods;
}
*/

static int bind_fn(ENGINE * engine, const char *id)
{
    printf("loaded engine: id=%s\n", id);

    if( !ENGINE_set_id(engine, TEST_ENGINE_ID)
     || !ENGINE_set_name(engine, TEST_ENGINE_NAME)
     || !ENGINE_set_init_function(engine, engineInit)
     || !ENGINE_set_finish_function(engine, engineFinish)
     || !ENGINE_set_destroy_function(engine, engineDestroy)
     || !ENGINE_set_load_ssl_client_cert_function(engine, engineLoadSSLClientCert)
//     || !ENGINE_set_load_privkey_function(engine, engineLoadPrivateKey)
//     || !ENGINE_set_ctrl_function(engine, engineCtrl)
//     || !ENGINE_set_cmd_defns(engine, mCommands)
#ifndef OPENSSL_NO_RSA
//        !ENGINE_set_RSA(engine, engineRSAMethods())
#endif
    ) {
        printf("failed to setup all engine functions");
        return 0;
    }

    return 1;
}

extern "C"
{
    IMPLEMENT_DYNAMIC_CHECK_FN();
    IMPLEMENT_DYNAMIC_BIND_FN(bind_fn);
}
