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

#define TEST_ENGINE_ID      "testkeyengine"
#define TEST_ENGINE_NAME    "dummy test key engine"

#define PRIVATE_KEY           "test/fixtures/keys/agent1-key.pem"

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

static EVP_PKEY* EngineLoadPrivkey(ENGINE* engine, const char* name,
                                   UI_METHOD* ui_method, void* callback_data) {
  if (strcmp(name, "dummykey") == 0) {
    std::string key = LoadFile(PRIVATE_KEY);
    BIO* bio = BIO_new_mem_buf(key.data(), key.size());
    EVP_PKEY* ret = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);

    BIO_vfree(bio);
    if (ret != nullptr) {
      return ret;
    }
  }

  return nullptr;
}

int bind_fn(ENGINE* engine, const char* id) {
  ENGINE_set_id(engine, TEST_ENGINE_ID);
  ENGINE_set_name(engine, TEST_ENGINE_NAME);
  ENGINE_set_init_function(engine, EngineInit);
  ENGINE_set_finish_function(engine, EngineFinish);
  ENGINE_set_destroy_function(engine, EngineDestroy);
  ENGINE_set_load_privkey_function(engine, EngineLoadPrivkey);

  return 1;
}

extern "C" {
  DEFAULT_VISIBILITY IMPLEMENT_DYNAMIC_CHECK_FN();
  DEFAULT_VISIBILITY IMPLEMENT_DYNAMIC_BIND_FN(bind_fn);
}

}  // anonymous namespace
