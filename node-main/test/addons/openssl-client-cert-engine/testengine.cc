#include <openssl/engine.h>
#include <openssl/pem.h>

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include <fstream>
#include <iterator>
#include <string>

#ifndef ENGINE_CMD_BASE
# error did not get engine.h
#endif

#define TEST_ENGINE_ID      "testengine"
#define TEST_ENGINE_NAME    "dummy test engine"

#define AGENT_KEY           "test/fixtures/keys/agent1-key.pem"
#define AGENT_CERT          "test/fixtures/keys/agent1-cert.pem"

#ifdef _WIN32
# define DEFAULT_VISIBILITY __declspec(dllexport)
#else
# define DEFAULT_VISIBILITY __attribute__((visibility("default")))
#endif

namespace {

int EngineInit(ENGINE* engine) {
  return 1;
}

int EngineFinish(ENGINE* engine) {
  return 1;
}

int EngineDestroy(ENGINE* engine) {
  return 1;
}

std::string LoadFile(const char* filename) {
  std::ifstream file(filename);
  return std::string(std::istreambuf_iterator<char>(file),
                     std::istreambuf_iterator<char>());
}


int EngineLoadSSLClientCert(ENGINE* engine,
                            SSL* ssl,
                            STACK_OF(X509_NAME)* ca_dn,
                            X509** ppcert,
                            EVP_PKEY** ppkey,
                            STACK_OF(X509)** pother,
                            UI_METHOD* ui_method,
                            void* callback_data) {
  if (ppcert != nullptr) {
    std::string cert = LoadFile(AGENT_CERT);
    if (cert.empty()) {
      return 0;
    }

    BIO* bio = BIO_new_mem_buf(cert.data(), cert.size());
    *ppcert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
     BIO_vfree(bio);
     if (*ppcert == nullptr) {
       printf("Could not read certificate\n");
       return 0;
     }
  }

  if (ppkey != nullptr) {
    std::string key = LoadFile(AGENT_KEY);
    if (key.empty()) {
      return 0;
    }

    BIO* bio = BIO_new_mem_buf(key.data(), key.size());
    *ppkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    BIO_vfree(bio);
    if (*ppkey == nullptr) {
      printf("Could not read private key\n");
      return 0;
    }
  }

  return 1;
}

int bind_fn(ENGINE* engine, const char* id) {
  ENGINE_set_id(engine, TEST_ENGINE_ID);
  ENGINE_set_name(engine, TEST_ENGINE_NAME);
  ENGINE_set_init_function(engine, EngineInit);
  ENGINE_set_finish_function(engine, EngineFinish);
  ENGINE_set_destroy_function(engine, EngineDestroy);
  ENGINE_set_load_ssl_client_cert_function(engine, EngineLoadSSLClientCert);

  return 1;
}

extern "C" {
  DEFAULT_VISIBILITY IMPLEMENT_DYNAMIC_CHECK_FN();
  DEFAULT_VISIBILITY IMPLEMENT_DYNAMIC_BIND_FN(bind_fn);
}

}  // anonymous namespace
