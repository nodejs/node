#include "http_parser.h"
#include <stdio.h>
#include <string.h>

void
dump_url (const char *url, const struct http_parser_url *u)
{
  unsigned int i;

  printf("\tfield_set: 0x%x, port: %u\n", u->field_set, u->port);
  for (i = 0; i < UF_MAX; i++) {
    if ((u->field_set & (1 << i)) == 0) {
      printf("\tfield_data[%u]: unset\n", i);
      continue;
    }

    printf("\tfield_data[%u]: off: %u, len: %u, part: %.*s\n",
           i,
           u->field_data[i].off,
           u->field_data[i].len,
           u->field_data[i].len,
           url + u->field_data[i].off);
  }
}

int main(int argc, char ** argv) {
  struct http_parser_url u;
  int len, connect, result;

  if (argc != 3) {
    printf("Syntax : %s connect|get url\n", argv[0]);
    return 1;
  }
  len = strlen(argv[2]);
  connect = strcmp("connect", argv[1]) == 0 ? 1 : 0;
  printf("Parsing %s, connect %d\n", argv[2], connect);

  result = http_parser_parse_url(argv[2], len, connect, &u);
  if (result != 0) {
    printf("Parse error : %d\n", result);
    return result;
  }
  printf("Parse ok, result : \n");
  dump_url(argv[2], &u);
  return 0;
}