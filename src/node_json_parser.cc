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

class JsonParser::Impl {
 public:
  simdjson::dom::parser parser;
};

JsonParser::JsonParser() : impl_(std::make_unique<Impl>()) {}

JsonParser::~JsonParser() = default;

void* JsonParser::GetParser() {
  return &impl_->parser;
}

using std::string;

using v8::Array;
using v8::ArrayBuffer;
using v8::ArrayBufferView;
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
using v8::Array;
using v8::String;
using v8::Undefined;
using v8::Value;
using v8::LocalVector;

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

      size_t size = array.size();
      LocalVector<Value> elements(isolate);
      elements.reserve(size);

      for (simdjson::dom::element child : array) {
        Local<Value> converted;

        if (!ConvertSimdjsonElement(isolate, child).ToLocal(&converted))
          return MaybeLocal<Value>();

        elements.push_back(converted);
      }

      Local<Array> v8_array = Array::New(isolate, elements.data(), size);

      return MaybeLocal<Value>(v8_array);
    }
    case simdjson::dom::element_type::OBJECT: {
      simdjson::dom::object object;
      simdjson::error_code error = element.get_object().get(object);

      THROW_AND_RETURN_EMPTY_IF_SIMDJSON_ERROR(isolate, error);

      size_t size = object.size();

      LocalVector<v8::Name> names(isolate);
      LocalVector<Value> values(isolate);
      names.reserve(size);
      values.reserve(size);

      for (auto [key, value] : object) {
        Local<String> v8_key;

        if (!String::NewFromUtf8(isolate,
                                 key.data(),
                                 v8::NewStringType::kNormal,
                                 key.size())
                 .ToLocal(&v8_key)) {
          return MaybeLocal<Value>();
        }

        Local<Value> converted;
        if (!ConvertSimdjsonElement(isolate, value).ToLocal(&converted))
          return MaybeLocal<Value>();

        names.push_back(v8_key);
        values.push_back(converted);
      }

      Local<Object> v8_object = Object::New(
          isolate, Null(isolate), names.data(), values.data(), size);

      return MaybeLocal<Value>(v8_object);
    }
    default:
      THROW_ERR_MODULE_NOT_INSTANTIATED(isolate);
      return MaybeLocal<Value>();
  }
}

MaybeLocal<Value> ParseInternal(Environment* env,
                               const char* data,
                               size_t length) {
  Isolate* isolate = env->isolate();
  simdjson::padded_string padded_string(data, length);

  // Get the cached parser from the Environment
  simdjson::dom::parser* parser = static_cast<simdjson::dom::parser*>(
      env->json_parser()->GetParser());
  simdjson::dom::element doc;

  simdjson::error_code error = parser->parse(padded_string).get(doc);

  if (error) {
    // TODO(araujogui): create a ERR_INVALID_JSON macro
    THROW_ERR_SOURCE_PHASE_NOT_DEFINED(
        isolate, "The \"json\" argument must be valid JSON.");
    return MaybeLocal<Value>();
  }

  return ConvertSimdjsonElement(isolate, doc);
}

void Parse(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (args.Length() < 1 || !args[0]->IsString()) {
    THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                               "The \"text\" argument must be a string.");
    return;
  }

  Local<String> json_str = args[0].As<String>();

  string str = ObjectToString(env->isolate(), json_str);

  Local<Value> result;
  if (!ParseInternal(env, str.data(), str.size()).ToLocal(&result))
    return;

  args.GetReturnValue().Set(result);
}

void ParseFromBuffer(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  if (args.Length() < 1) {
    THROW_ERR_INVALID_ARG_TYPE(
        isolate, "The \"buffer\" argument must be an ArrayBuffer or a view.");
    return;
  }

  const char* data;
  size_t length;

  if (args[0]->IsArrayBufferView()) {
    Local<ArrayBufferView> view = args[0].As<ArrayBufferView>();
    Local<ArrayBuffer> buffer = view->Buffer();
    data = static_cast<const char*>(buffer->Data()) + view->ByteOffset();
    length = view->ByteLength();
  } else if (args[0]->IsArrayBuffer()) {
    Local<ArrayBuffer> buffer = args[0].As<ArrayBuffer>();
    data = static_cast<const char*>(buffer->Data());
    length = buffer->ByteLength();
  } else {
    THROW_ERR_INVALID_ARG_TYPE(
        isolate, "The \"buffer\" argument must be an ArrayBuffer or a view.");
    return;
  }

  Local<Value> result;
  if (!ParseInternal(env, data, length).ToLocal(&result)) return;

  args.GetReturnValue().Set(result);
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(Parse);
  registry->Register(ParseFromBuffer);
}

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  SetMethodNoSideEffect(context, target, "parse", Parse);
  SetMethodNoSideEffect(context, target, "parseFromBuffer", ParseFromBuffer);
}

}  // namespace json_parser
}  // namespace node

#undef THROW_AND_RETURN_EMPTY_IF_SIMDJSON_ERROR

NODE_BINDING_CONTEXT_AWARE_INTERNAL(json_parser, node::json_parser::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(json_parser,
                                node::json_parser::RegisterExternalReferences)
