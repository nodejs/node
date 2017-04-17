#include <openssl/engine.h>

static const char *engine_id = "node_test_engine";
static const char *engine_name = "Node Test Engine for OpenSSL";

static int bind(ENGINE *e, const char *id)
{
  ENGINE_set_id(e, engine_id);
  ENGINE_set_name(e, engine_name);
  return 1;
}

IMPLEMENT_DYNAMIC_BIND_FN(bind)
IMPLEMENT_DYNAMIC_CHECK_FN()
