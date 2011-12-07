#include <node_vars.h>
#if HAVE_OPENSSL
# include <node_crypto.h>
#endif
#include <string.h>

namespace node {

// For now we just statically initialize the globals structure. Later there
// will be one struct globals for each isolate.

static struct globals g_struct;
static struct globals* g_ptr;


static void globals_init(struct globals* g) {
  memset(g, 0, sizeof(struct globals));
  g->debug_port = 5858;

#ifdef OPENSSL_NPN_NEGOTIATED
  g->use_npn = true;
#else
  g->use_npn = false;
#endif

#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
  g->use_sni = true;
#else
  g->use_sni = false;
#endif
}


struct globals* globals_get() {
  if (!g_ptr) {
    g_ptr = &g_struct;
    globals_init(g_ptr);
  }
  return g_ptr;
}

}  // namespace node
