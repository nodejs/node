#include "tracing/trace_config_parser.h"

#include <string.h>

namespace node {
namespace tracing {

using v8::Array;
using v8::Boolean;
using v8::HandleScope;
using v8::JSON;
using v8::NewStringType;
using v8::String;

// String options that can be used to initialize TraceOptions.
static const char* kRecordUntilFull = "record-until-full";
static const char* kRecordContinuously = "record-continuously";
static const char* kRecordAsMuchAsPossible = "record-as-much-as-possible";

static const char* kRecordModeParam = "record_mode";
static const char* kEnableSamplingParam = "enable_sampling";
static const char* kEnableSystraceParam = "enable_systrace";
static const char* kEnableArgumentFilterParam = "enable_argument_filter";
static const char* kIncludedCategoriesParam = "included_categories";
static const char* kExcludedCategoriesParam = "excluded_categories";

void TraceConfigParser::FillTraceConfig(
    Isolate* isolate, TraceConfig* trace_config, const char* json_str) {
  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  Local<String> source =
      String::NewFromUtf8(isolate, json_str, NewStringType::kNormal)
          .ToLocalChecked();
  Local<Value> result = JSON::Parse(context, source).ToLocalChecked();
  Local<Object> trace_config_object = Local<Object>::Cast(result);

  trace_config->SetTraceRecordMode(
      GetTraceRecordMode(isolate, context, trace_config_object));
  if (GetBoolean(isolate, context, trace_config_object, kEnableSamplingParam))
    trace_config->EnableSampling();
  if (GetBoolean(isolate, context, trace_config_object, kEnableSystraceParam))
    trace_config->EnableSystrace();
  if (GetBoolean(
      isolate, context, trace_config_object, kEnableArgumentFilterParam))
    trace_config->EnableArgumentFilter();
  UpdateCategoriesList(isolate, context, trace_config_object,
                       kIncludedCategoriesParam, trace_config);
  UpdateCategoriesList(isolate, context, trace_config_object,
                       kExcludedCategoriesParam, trace_config);
}

bool TraceConfigParser::GetBoolean(Isolate* isolate, Local<Context> context,
                                   Local<Object> object, const char* property) {
  Local<Value> value = GetValue(isolate, context, object, property);
  if (value->IsNumber()) {
    Local<Boolean> v8_boolean = value->ToBoolean(context).ToLocalChecked();
    return v8_boolean->Value();
  }
  return false;
}

int TraceConfigParser::UpdateCategoriesList(
    Isolate* isolate, Local<Context> context, Local<Object> object,
    const char* property, TraceConfig* trace_config) {
  Local<Value> value = GetValue(isolate, context, object, property);
  if (value->IsArray()) {
    Local<Array> v8_array = Local<Array>::Cast(value);
    for (int i = 0, length = v8_array->Length(); i < length; ++i) {
      Local<Value> v = v8_array->Get(context, i)
                           .ToLocalChecked()
                           ->ToString(context)
                           .ToLocalChecked();
      String::Utf8Value str(v->ToString(context).ToLocalChecked());
      if (property == kIncludedCategoriesParam) {
        trace_config->AddIncludedCategory(*str);
      } else if (property == kExcludedCategoriesParam) {
        trace_config->AddExcludedCategory(*str);
      }
    }
    return v8_array->Length();
  }
  return 0;
}

TraceRecordMode TraceConfigParser::GetTraceRecordMode(Isolate* isolate,
                                                      Local<Context> context,
                                                      Local<Object> object) {
  Local<Value> value = GetValue(isolate, context, object, kRecordModeParam);
  if (value->IsString()) {
    Local<String> v8_string = value->ToString(context).ToLocalChecked();
    String::Utf8Value str(v8_string);
    if (strcmp(kRecordUntilFull, *str) == 0) {
      return TraceRecordMode::RECORD_UNTIL_FULL;
    } else if (strcmp(kRecordContinuously, *str) == 0) {
      return TraceRecordMode::RECORD_CONTINUOUSLY;
    } else if (strcmp(kRecordAsMuchAsPossible, *str) == 0) {
      return TraceRecordMode::RECORD_AS_MUCH_AS_POSSIBLE;
    }
  }
  return TraceRecordMode::RECORD_UNTIL_FULL;
}

Local<Value> TraceConfigParser::GetValue(Isolate* isolate,
                                         Local<Context> context,
                                         Local<Object> object,
                                         const char* property) {
  Local<String> v8_str =
      String::NewFromUtf8(isolate, property, NewStringType::kNormal)
          .ToLocalChecked();
  return object->Get(context, v8_str).ToLocalChecked();
}

}  // namespace tracing
}  // namespace node
