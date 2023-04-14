#include "json_parser.h"
#include "node_errors.h"
#include "node_v8_platform-inl.h"
#include "util-inl.h"

namespace node {
using v8::ArrayBuffer;
using v8::Context;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

static Isolate* NewIsolate(v8::ArrayBuffer::Allocator* allocator) {
  Isolate* isolate = Isolate::Allocate();
  CHECK_NOT_NULL(isolate);
  per_process::v8_platform.Platform()->RegisterIsolate(isolate,
                                                       uv_default_loop());
  Isolate::CreateParams params;
  params.array_buffer_allocator = allocator;
  Isolate::Initialize(isolate, params);
  return isolate;
}

void JSONParser::FreeIsolate(Isolate* isolate) {
  per_process::v8_platform.Platform()->UnregisterIsolate(isolate);
  isolate->Dispose();
}

JSONParser::JSONParser()
    : allocator_(ArrayBuffer::Allocator::NewDefaultAllocator()),
      isolate_(NewIsolate(allocator_.get())),
      handle_scope_(isolate_.get()),
      context_(isolate_.get(), Context::New(isolate_.get())),
      context_scope_(context_.Get(isolate_.get())) {}

bool JSONParser::Parse(const std::string& content) {
  DCHECK(!parsed_);

  Isolate* isolate = isolate_.get();
  Local<Context> context = context_.Get(isolate);

  // It's not a real script, so don't print the source line.
  errors::PrinterTryCatch bootstrapCatch(
      isolate, errors::PrinterTryCatch::kDontPrintSourceLine);
  Local<Value> json_string_value;
  Local<Value> result_value;
  if (!ToV8Value(context, content).ToLocal(&json_string_value) ||
      !json_string_value->IsString() ||
      !v8::JSON::Parse(context, json_string_value.As<String>())
           .ToLocal(&result_value) ||
      !result_value->IsObject()) {
    return false;
  }
  content_.Reset(isolate, result_value.As<Object>());
  parsed_ = true;
  return true;
}

std::optional<std::string> JSONParser::GetTopLevelStringField(
    const std::string& field) {
  Isolate* isolate = isolate_.get();
  Local<Context> context = context_.Get(isolate);
  Local<Object> content_object = content_.Get(isolate);
  Local<Value> value;
  // It's not a real script, so don't print the source line.
  errors::PrinterTryCatch bootstrapCatch(
      isolate, errors::PrinterTryCatch::kDontPrintSourceLine);
  if (!content_object
           ->Get(context, OneByteString(isolate, field.c_str(), field.length()))
           .ToLocal(&value) ||
      !value->IsString()) {
    return {};
  }
  Utf8Value utf8_value(isolate, value);
  return utf8_value.ToString();
}

std::optional<bool> JSONParser::GetTopLevelBoolField(const std::string& field) {
  Isolate* isolate = isolate_.get();
  Local<Context> context = context_.Get(isolate);
  Local<Object> content_object = content_.Get(isolate);
  Local<Value> value;
  // It's not a real script, so don't print the source line.
  errors::PrinterTryCatch bootstrapCatch(
      isolate, errors::PrinterTryCatch::kDontPrintSourceLine);
  if (!content_object
           ->Has(context, OneByteString(isolate, field.c_str(), field.length()))
           .FromMaybe(false)) {
    return false;
  }
  if (!content_object
           ->Get(context, OneByteString(isolate, field.c_str(), field.length()))
           .ToLocal(&value) ||
      !value->IsBoolean()) {
    return {};
  }
  return value->BooleanValue(isolate);
}

}  // namespace node
