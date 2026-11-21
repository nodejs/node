#ifndef SRC_INSPECTOR_INSPECTOR_OBJECT_UTILS_H_
#define SRC_INSPECTOR_INSPECTOR_OBJECT_UTILS_H_

#include <v8.h>
#include <string>
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

// Throws a TypeError with the given message, used to report malformed
// protocol event parameters.
void ThrowEventError(v8::Isolate* isolate, const std::string& message);

// Like the ObjectGet*() helpers above, but for required properties: on failure
// a TypeError "Missing <prefix><property> in event" is thrown and false is
// returned.
bool ObjectGetRequired(v8::Local<v8::Context> context,
                       v8::Local<v8::Object> object,
                       const char* property,
                       protocol::String* out,
                       const char* prefix = "");
bool ObjectGetRequired(v8::Local<v8::Context> context,
                       v8::Local<v8::Object> object,
                       const char* property,
                       double* out,
                       const char* prefix = "");
bool ObjectGetRequired(v8::Local<v8::Context> context,
                       v8::Local<v8::Object> object,
                       const char* property,
                       int* out,
                       const char* prefix = "");
bool ObjectGetRequired(v8::Local<v8::Context> context,
                       v8::Local<v8::Object> object,
                       const char* property,
                       bool* out,
                       const char* prefix = "");
bool ObjectGetRequired(v8::Local<v8::Context> context,
                       v8::Local<v8::Object> object,
                       const char* property,
                       v8::Local<v8::Object>* out,
                       const char* prefix = "");

}  // namespace inspector
}  // namespace node
#endif  // SRC_INSPECTOR_INSPECTOR_OBJECT_UTILS_H_
