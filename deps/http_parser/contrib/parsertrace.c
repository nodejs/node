/* Copyright Joyent, Inc. and other Node contributors.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/* Dump what the parser finds to stdout as it happen */

#include "http_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int on_message_begin(http_parser* _) {
  (void)_;
  printf("\n***MESSAGE BEGIN***\n\n");
  return 0;
}

int on_headers_complete(http_parser* _) {
  (void)_;
  printf("\n***HEADERS COMPLETE***\n\n");
  return 0;
}

int on_message_complete(http_parser* _) {
  (void)_;
  printf("\n***MESSAGE COMPLETE***\n\n");
  return 0;
}

int on_url(http_parser* _, const char* at, size_t length) {
  (void)_;
  printf("Url: %.*s\n", (int)length, at);
  return 0;
}

int on_header_field(http_parser* _, const char* at, size_t length) {
  (void)_;
  printf("Header field: %.*s\n", (int)length, at);
  return 0;
}

int on_header_value(http_parser* _, const char* at, size_t length) {
  (void)_;
  printf("Header value: %.*s\n", (int)length, at);
  return 0;
}

int on_body(http_parser* _, const char* at, size_t length) {
  (void)_;
  printf("Body: %.*s\n", (int)length, at);
  return 0;
}

void usage(const char* name) {
  fprintf(stderr,
          "Usage: %s $type $filename\n"
          "  type: -x, where x is one of {r,b,q}\n"
          "  parses file as a Response, reQuest, or Both\n",
          name);
  exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {
  enum http_parser_type file_type;

  if (argc != 3) {
    usage(argv[0]);
  }

  char* type = argv[1];
  if (type[0] != '-') {
    usage(argv[0]);
  }

  switch (type[1]) {
    /* in the case of "-", type[1] will be NUL */
    case 'r':
      file_type = HTTP_RESPONSE;
      break;
    case 'q':
      file_type = HTTP_REQUEST;
      break;
    case 'b':
      file_type = HTTP_BOTH;
      break;
    default:
      usage(argv[0]);
  }

  char* filename = argv[2];
  FILE* file = fopen(filename, "r");
  if (file == NULL) {
    perror("fopen");
    goto fail;
  }

  fseek(file, 0, SEEK_END);
  long file_length = ftell(file);
  if (file_length == -1) {
    perror("ftell");
    goto fail;
  }
  fseek(file, 0, SEEK_SET);

  char* data = malloc(file_length);
  if (fread(data, 1, file_length, file) != (size_t)file_length) {
    fprintf(stderr, "couldn't read entire file\n");
    free(data);
    goto fail;
  }

  http_parser_settings settings;
  memset(&settings, 0, sizeof(settings));
  settings.on_message_begin = on_message_begin;
  settings.on_url = on_url;
  settings.on_header_field = on_header_field;
  settings.on_header_value = on_header_value;
  settings.on_headers_complete = on_headers_complete;
  settings.on_body = on_body;
  settings.on_message_complete = on_message_complete;

  http_parser parser;
  http_parser_init(&parser, file_type);
  size_t nparsed = http_parser_execute(&parser, &settings, data, file_length);
  free(data);

  if (nparsed != (size_t)file_length) {
    fprintf(stderr,
            "Error: %s (%s)\n",
            http_errno_description(HTTP_PARSER_ERRNO(&parser)),
            http_errno_name(HTTP_PARSER_ERRNO(&parser)));
    goto fail;
  }

  return EXIT_SUCCESS;

fail:
  fclose(file);
  return EXIT_FAILURE;
}
