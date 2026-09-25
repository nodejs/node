#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "http_parser.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  static const http_parser_settings settings_null = {
    .on_message_begin = 0
    , .on_header_field = 0
    ,.on_header_value = 0
    ,.on_url = 0
    ,.on_status = 0
    ,.on_body = 0
    ,.on_headers_complete = 0
    ,.on_message_complete = 0
    ,.on_chunk_header = 0
    ,.on_chunk_complete = 0
  };

  http_parser parser;
  http_parser_init(&parser, HTTP_BOTH);
  http_parser_execute(&parser, &settings_null, (char*)data, size);

  return 0;
}
