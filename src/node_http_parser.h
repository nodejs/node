#ifndef SRC_NODE_HTTP_PARSER_H_
#define SRC_NODE_HTTP_PARSER_H_

#include "v8.h"

#include "http_parser.h"

namespace node {

void InitHttpParser(v8::Handle<v8::Object> target);

}  // namespace node

#endif  // SRC_NODE_HTTP_PARSER_H_
