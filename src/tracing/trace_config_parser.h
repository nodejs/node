#ifndef SRC_TRACE_CONFIG_PARSER_H_
#define SRC_TRACE_CONFIG_PARSER_H_

#include "libplatform/v8-tracing.h"
#include "v8.h"

namespace node {
namespace tracing {

using v8::platform::tracing::TraceConfig;
using v8::platform::tracing::TraceRecordMode;
using v8::Context;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::Value;

class TraceConfigParser {
 public:
  static void FillTraceConfig(Isolate* isolate, TraceConfig* trace_config,
                              const char* json_str);

 private:
  static bool GetBoolean(Isolate* isolate, Local<Context> context,
                         Local<Object> object, const char* property);

  static int UpdateCategoriesList(Isolate* isolate, Local<Context> context,
                                  Local<Object> object,
                                  const char* property,
                                  TraceConfig* trace_config);

  static TraceRecordMode GetTraceRecordMode(Isolate* isolate,
                                            Local<Context> context,
                                            Local<Object> object);

  static Local<Value> GetValue(Isolate* isolate, Local<Context> context,
                               Local<Object> object, const char* property);
};

}  // namespace tracing
}  // namespace node

#endif  // SRC_TRACE_CONFIG_PARSER_H_
