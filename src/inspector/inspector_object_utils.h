#ifndef SRC_INSPECTOR_INSPECTOR_OBJECT_UTILS_H_
#define SRC_INSPECTOR_INSPECTOR_OBJECT_UTILS_H_

#include <v8.h>
#include "node/inspector/protocol/Protocol.h"

namespace node {
namespace inspector {

v8::Maybe<protocol::String> ObjectGetProtocolString(
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> object,
    v8::Local<v8::String> property);

v8::Maybe<protocol::String> ObjectGetProtocolString(
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> object,
    const char* property);

v8::Maybe<double> ObjectGetDouble(v8::Local<v8::Context> context,
                                  v8::Local<v8::Object> object,
                                  const char* property);

v8::Maybe<int> ObjectGetInt(v8::Local<v8::Context> context,
                            v8::Local<v8::Object> object,
                            const char* property);

v8::Maybe<bool> ObjectGetBool(v8::Local<v8::Context> context,
                              v8::Local<v8::Object> object,
                              const char* property);

v8::MaybeLocal<v8::Object> ObjectGetObject(v8::Local<v8::Context> context,
                                           v8::Local<v8::Object> object,
                                           const char* property);

}  // namespace inspector
}  // namespace node
#endif  // SRC_INSPECTOR_INSPECTOR_OBJECT_UTILS_H_
