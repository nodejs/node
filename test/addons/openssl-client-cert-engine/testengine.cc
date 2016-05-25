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
# error did not get engine.h
#endif

#define TEST_ENGINE_ID      "testengine"
#define TEST_ENGINE_NAME    "dummy test engine"

#define AGENT_KEY           "test/fixtures/keys/agent1-key.pem"
#define AGENT_CERT          "test/fixtures/keys/agent1-cert.pem"

static int EngineInit(ENGINE* engine) {
  printf("EngineInit\n");
  return 1;
}

static int EngineFinish(ENGINE* engine) {
  printf("EngineFinish");
  return 1;
}

static int EngineDestroy(ENGINE* engine) {
  printf("EngineDestroy");
  return 1;
}

static std::string loadFile(const char* filename) {
  std::string ret;
  FILE* fp = fopen(filename, "r");
  if (fp == NULL) {
    printf("Could not open file '%s'\n", filename);
    return ret;
  }

  char buf[1024];
  while (true) {
    size_t r = fread(buf, 1, 1024, fp);
    if (r > 0) {
       ret.append(buf, r);
    }
    if (r < 1024) {
      break;
    }
  }

  fclose(fp);
  return ret;
}


static int EngineLoadSSLClientCert(ENGINE* engine,
                                   SSL* ssl,
                                   STACK_OF(X509_NAME)* ca_dn,
                                   X509** ppcert,
                                   EVP_PKEY** ppkey,
                                   STACK_OF(X509)** pother,
                                   UI_METHOD* ui_method,
                                   void* callback_data) {
  printf("EngineLoadSSLClientCert\n");

  if (ppcert) {
    std::string cert = loadFile(AGENT_CERT);
    if (cert.size() == 0) {
      return 0;
    }

    BIO* bio = BIO_new_mem_buf(cert.data(), cert.size());
    *ppcert = PEM_read_bio_X509(bio, NULL, NULL, NULL);
     BIO_vfree(bio);
     if (*ppcert == NULL) {
       printf("Could not read certificate\n");
       return 0;
     }
  }

  if (ppkey) {
    std::string key = loadFile(AGENT_KEY);
    if (key.empty()) {
      return 0;
    }

    BIO* bio = BIO_new_mem_buf(key.data(), key.size());
    *ppkey = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
    BIO_vfree(bio);
    if (*ppkey == NULL) {
      printf("Could not read private key\n");
      return 0;
    }
  }

  return 1;
}

static int bind_fn(ENGINE* engine, const char* id) {
  printf("loaded engine: id=%s\n", id);

  ENGINE_set_id(engine, TEST_ENGINE_ID);
  ENGINE_set_name(engine, TEST_ENGINE_NAME);
  ENGINE_set_init_function(engine, EngineInit);
  ENGINE_set_finish_function(engine, EngineFinish);
  ENGINE_set_destroy_function(engine, EngineDestroy);
  ENGINE_set_load_ssl_client_cert_function(engine, EngineLoadSSLClientCert);

  return 1;
}

extern "C" {
  IMPLEMENT_DYNAMIC_CHECK_FN();
  IMPLEMENT_DYNAMIC_BIND_FN(bind_fn);
}
