#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "urlparse.h"
#include "http-parser/http_parser.h"

namespace {
/* Copied from https://github.com/nodejs/http-parser */
void dump_url(const uint8_t *url, const urlparse_url *u) {
  size_t i;

  fprintf(stderr, "\tfield_set: 0x%x, port: %u\n", u->field_set, u->port);
  for (i = 0; i < URLPARSE_MAX; i++) {
    if ((u->field_set & (1 << i)) == 0) {
      fprintf(stderr, "\tfield_data[%zu]: unset\n", i);
      continue;
    }

    fprintf(stderr, "\tfield_data[%zu]: off: %u len: %u part: \"%.*s\"\n", i,
            u->field_data[i].off, u->field_data[i].len, u->field_data[i].len,
            url + u->field_data[i].off);
  }
}
} // namespace

namespace {
int fuzz(const uint8_t *data, size_t size, int is_connect) {
  urlparse_url u;
  struct http_parser_url hu;
  int rv1, rv2;

  rv1 = urlparse_parse_url(reinterpret_cast<const char *>(data), size,
                           is_connect, &u);
  memset(&hu, 0, sizeof(hu));
  rv2 = http_parser_parse_url(reinterpret_cast<const char *>(data), size,
                              is_connect, &hu);
  if (rv2 != 0) {
    rv2 = URLPARSE_ERR_PARSE;
  }

  if (rv1 != rv2) {
    fprintf(
      stderr,
      "urlparse_parse_url(%d) and http_parser_parse_url(%d) disagree with "
      "is_connect = %d\n",
      rv1, rv2, is_connect);
    return -1;
  }

  if (rv1 == 0 && memcmp(&hu, &u, sizeof(u)) != 0) {
    fprintf(stderr, "is_connect = %d\n", is_connect);
    fprintf(stderr, "target http_parser_url:\n");
    dump_url(data, reinterpret_cast<urlparse_url *>(&hu));
    fprintf(stderr, "result urlparse_url:\n");
    dump_url(data, &u);

    return -1;
  }

  return 0;
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (fuzz(data, size, /* is_connect = */ 0) != 0 ||
      fuzz(data, size, /* is_connect = */ 1) != 0) {
    abort();
  }

  return 0;
}
