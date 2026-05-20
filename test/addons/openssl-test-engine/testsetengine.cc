#include <openssl/engine.h>

#ifndef ENGINE_CMD_BASE
# error did not get engine.h
#endif

#define TEST_ENGINE_ID      "testsetengine"
#define TEST_ENGINE_NAME    "dummy test engine"

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

int bind_fn(ENGINE* engine, const char* id) {
  ENGINE_set_id(engine, TEST_ENGINE_ID);
  ENGINE_set_name(engine, TEST_ENGINE_NAME);
  ENGINE_set_init_function(engine, EngineInit);
  ENGINE_set_finish_function(engine, EngineFinish);
  ENGINE_set_destroy_function(engine, EngineDestroy);
  return 1;
}

extern "C" {
  DEFAULT_VISIBILITY IMPLEMENT_DYNAMIC_CHECK_FN();
  DEFAULT_VISIBILITY IMPLEMENT_DYNAMIC_BIND_FN(bind_fn);
}

}  // anonymous namespace
