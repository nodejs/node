#include <openssl/engine.h>

#include <stdio.h>

int bind(ENGINE* e, const char* id) {
  if (ENGINE_set_id(e, "libtest_crypto_engine") == 0) {
    fprintf(stderr, "ENGINE_set_id() failed.\n");
    return 0;
  }

  if (ENGINE_set_name(e, "A test crypto engine") == 0) {
    fprintf(stderr, "ENGINE_set_name() failed.\n");
    return 0;
  }

  return 1;
}

IMPLEMENT_DYNAMIC_BIND_FN(bind)
IMPLEMENT_DYNAMIC_CHECK_FN()
