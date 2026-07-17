#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "http_parser.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  struct http_parser_url u;
  http_parser_url_init(&u);
  http_parser_parse_url((char*)data, size, 0, &u);
  http_parser_parse_url((char*)data, size, 1, &u);

  return 0;
}
