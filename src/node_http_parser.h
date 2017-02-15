#ifndef SRC_NODE_HTTP_PARSER_H_
#define SRC_NODE_HTTP_PARSER_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "v8.h"

#include "http_parser.h"

namespace node {

void InitHttpParser(v8::Local<v8::Object> target);

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_HTTP_PARSER_H_
