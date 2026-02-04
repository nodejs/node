#ifndef SRC_NODE_JSON_PARSER_H_
#define SRC_NODE_JSON_PARSER_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_external_reference.h"

namespace node {
namespace json_parser {

void Initialize(v8::Local<v8::Object> target,
                v8::Local<v8::Value> unused,
                v8::Local<v8::Context> context,
                void* priv);

void RegisterExternalReferences(ExternalReferenceRegistry* registry);

}  // namespace json_parser
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_JSON_PARSER_H_
