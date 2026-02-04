#include "node_json_parser.h"
#include "env-inl.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "simdjson.h"
#include "simdutf.h"

#define THROW_AND_RETURN_EMPTY_IF_SIMDJSON_ERROR(isolate, error)               \
  do {                                                                         \
    if ((error)) {                                                             \
      THROW_ERR_MODULE_NOT_INSTANTIATED((isolate));                            \
      return MaybeLocal<Value>();                                              \
    }                                                                          \
  } while (0)

namespace node {
namespace json_parser {

using std::string;

using v8::Array;
using v8::Boolean;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Null;
using v8::Number;
using v8::Object;
using v8::Primitive;
using v8::String;
using v8::Undefined;
using v8::Value;

template <typename T>
inline MaybeLocal<Value> ToV8Number(Isolate* isolate,
                                    simdjson::dom::element element) {
  T value;
  simdjson::error_code error = std::move(element.get<T>()).get(value);

  THROW_AND_RETURN_EMPTY_IF_SIMDJSON_ERROR(isolate, error);

  return MaybeLocal<Value>(Number::New(isolate, static_cast<T>(value)));
}

string ObjectToString(Isolate* isolate, Local<Value> value) {
  // TODO(araujogui): investigate if V8 WriteUtf8V2 is faster than
  // convert_utf16_to_utf8
  Utf8Value utf8_value(isolate, value);
  return string(*utf8_value);
}

MaybeLocal<Value> ConvertSimdjsonElement(Isolate* isolate,
                                         simdjson::dom::element element) {
  simdjson::dom::element_type type = element.type();

  switch (type) {
    case simdjson::dom::element_type::INT64: {
      return ToV8Number<int64_t>(isolate, element);
    }
    case simdjson::dom::element_type::UINT64: {
      return ToV8Number<uint64_t>(isolate, element);
    }
    case simdjson::dom::element_type::DOUBLE: {
      return ToV8Number<double>(isolate, element);
    }
    case simdjson::dom::element_type::BOOL: {
      bool value;
      simdjson::error_code error = element.get_bool().get(value);

      THROW_AND_RETURN_EMPTY_IF_SIMDJSON_ERROR(isolate, error);

      return MaybeLocal<Value>(Boolean::New(isolate, value));
    }
    case simdjson::dom::element_type::NULL_VALUE: {
      Local<Primitive> null = Null(isolate);

      return MaybeLocal<Value>(null);
    }
    case simdjson::dom::element_type::STRING: {
      string value;
      simdjson::error_code error = element.get_string().get(value);

      THROW_AND_RETURN_EMPTY_IF_SIMDJSON_ERROR(isolate, error);

      return String::NewFromUtf8(isolate, value.c_str());
    }
    case simdjson::dom::element_type::ARRAY: {
      simdjson::dom::array array;
      simdjson::error_code error = element.get_array().get(array);

      THROW_AND_RETURN_EMPTY_IF_SIMDJSON_ERROR(isolate, error);

      Local<Array> v8_array =
          v8::Array::New(isolate, array.size());

      Local<Context> context = isolate->GetCurrentContext();

      uint32_t index = 0;
      for (simdjson::dom::element child : array) {
        Local<Value> converted;

        if (!ConvertSimdjsonElement(isolate, child).ToLocal(&converted))
          return MaybeLocal<Value>();

        if (v8_array->Set(context, index, converted).IsNothing())
          return MaybeLocal<Value>();
        index++;
      }

      return MaybeLocal<Value>(v8_array);
    }
    case simdjson::dom::element_type::OBJECT: {
      simdjson::dom::object object;
      simdjson::error_code error = element.get_object().get(object);

      THROW_AND_RETURN_EMPTY_IF_SIMDJSON_ERROR(isolate, error);

      Local<Object> v8_object = Object::New(isolate);
      Local<Context> context = isolate->GetCurrentContext();

      for (const simdjson::dom::key_value_pair& kv : object) {
        const std::string_view key = kv.key;

        Local<String> v8_key;
        if (!String::NewFromUtf8(isolate,
                                 key.data(),
                                 v8::NewStringType::kNormal,
                                 static_cast<int>(key.size()))
                 .ToLocal(&v8_key)) {
          return MaybeLocal<Value>();
        }

        Local<Value> converted;
        if (!ConvertSimdjsonElement(isolate, kv.value).ToLocal(&converted))
          return MaybeLocal<Value>();

        if (v8_object->Set(context, v8_key, converted).IsNothing())
          return MaybeLocal<Value>();
      }

      return MaybeLocal<Value>(v8_object);
    }
    default:
      THROW_ERR_MODULE_NOT_INSTANTIATED(isolate);
      return MaybeLocal<Value>();
  }
}

void Parse(const FunctionCallbackInfo<Value>& args) {
  if (args.Length() < 1 || !args[0]->IsString()) {
    THROW_ERR_INVALID_ARG_TYPE(args.GetIsolate(),
                               "The \"text\" argument must be a string.");
    return;
  }

  Local<String> json_str = args[0].As<String>();

  // TODO(araujogui): Remove memory copy
  string key = ObjectToString(args.GetIsolate(), json_str);
  simdjson::padded_string padded_string(key);

  simdjson::dom::parser parser;
  simdjson::dom::element doc;

  simdjson::error_code error = parser.parse(padded_string).get(doc);

  if (error) {
    // TODO(araujogui): create a ERR_INVALID_JSON macro
    THROW_ERR_SOURCE_PHASE_NOT_DEFINED(
        args.GetIsolate(), "The \"json\" argument must be a string.");
    return;
  }

  Local<Value> result;

  if (!ConvertSimdjsonElement(args.GetIsolate(), doc).ToLocal(&result)) return;

  args.GetReturnValue().Set(result);
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(Parse);
}

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  SetMethodNoSideEffect(context, target, "parse", Parse);
}

}  // namespace json_parser
}  // namespace node

#undef THROW_AND_RETURN_EMPTY_IF_SIMDJSON_ERROR

NODE_BINDING_CONTEXT_AWARE_INTERNAL(json_parser, node::json_parser::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(json_parser,
                                node::json_parser::RegisterExternalReferences)
