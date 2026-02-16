#include "env-inl.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "simdjson.h"
#include "util-inl.h"

#define THROW_AND_RETURN_EMPTY_IF_SIMDJSON_ERROR(isolate, error)               \
  do {                                                                         \
    if ((error)) {                                                             \
      THROW_ERR_MODULE_NOT_INSTANTIATED((isolate));                            \
      return MaybeLocal<Value>();                                              \
    }                                                                          \
  } while (0)

namespace node {
namespace json_parser {

using v8::Boolean;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Value;

void IllegalConstructor(const FunctionCallbackInfo<Value>& args) {
  THROW_ERR_ILLEGAL_CONSTRUCTOR(Environment::GetCurrent(args));
}

Local<String> JsonTypeToV8String(Isolate* isolate,
                                 simdjson::ondemand::json_type type) {
  switch (type) {
    case simdjson::ondemand::json_type::object:
      return FIXED_ONE_BYTE_STRING(isolate, "object");
    case simdjson::ondemand::json_type::array:
      return FIXED_ONE_BYTE_STRING(isolate, "array");
    case simdjson::ondemand::json_type::number:
      return FIXED_ONE_BYTE_STRING(isolate, "number");
    case simdjson::ondemand::json_type::string:
      return FIXED_ONE_BYTE_STRING(isolate, "string");
    case simdjson::ondemand::json_type::boolean:
      return FIXED_ONE_BYTE_STRING(isolate, "boolean");
    case simdjson::ondemand::json_type::null:
      return FIXED_ONE_BYTE_STRING(isolate, "null");
    case simdjson::ondemand::json_type::unknown:
      // todo(araujogui): should throw error?
      return FIXED_ONE_BYTE_STRING(isolate, "unknown");
  }
  UNREACHABLE();
}

class JsonValue;

class JsonDocument : public BaseObject {
 public:
  JsonDocument(Environment* env,
               Local<Object> object,
               simdjson::padded_string padded,
               simdjson::ondemand::document document)
      : BaseObject(env, object),
        padded_(std::move(padded)),
        document_(std::move(document)) {
    MakeWeak();
  }

  static Local<FunctionTemplate> GetConstructorTemplate(Environment* env);

  static BaseObjectPtr<JsonDocument> Create(
      Environment* env,
      simdjson::padded_string padded,
      simdjson::ondemand::document document);

  static void Type(const FunctionCallbackInfo<Value>& args);
  static void Root(const FunctionCallbackInfo<Value>& args);
  static void GetArray(const FunctionCallbackInfo<Value>& args);

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(JsonDocument)
  SET_SELF_SIZE(JsonDocument)

 private:
  simdjson::padded_string padded_;
  simdjson::ondemand::document document_;

  friend class JsonValue;
};

Local<FunctionTemplate> JsonDocument::GetConstructorTemplate(Environment* env) {
  Local<FunctionTemplate> tmpl = env->json_document_constructor_template();
  if (tmpl.IsEmpty()) {
    Isolate* isolate = env->isolate();
    tmpl = NewFunctionTemplate(isolate, nullptr);
    tmpl->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "JsonDocument"));
    SetProtoMethod(isolate, tmpl, "type", JsonDocument::Type);
    SetProtoMethod(isolate, tmpl, "root", JsonDocument::Root);
    SetProtoMethod(isolate, tmpl, "getArray", JsonDocument::GetArray);
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        JsonDocument::kInternalFieldCount);
    env->set_json_document_constructor_template(tmpl);
  }
  return tmpl;
}

BaseObjectPtr<JsonDocument> JsonDocument::Create(
    Environment* env,
    simdjson::padded_string padded,
    simdjson::ondemand::document document) {
  Local<Object> obj;

  if (!GetConstructorTemplate(env)
           ->InstanceTemplate()
           ->NewInstance(env->context())
           .ToLocal(&obj)) {
    return nullptr;
  }

  return MakeBaseObject<JsonDocument>(
      env, obj, std::move(padded), std::move(document));
}

class JsonValue : public BaseObject {
 public:
  JsonValue(Environment* env,
            Local<Object> object,
            BaseObjectPtr<JsonDocument> document,
            simdjson::ondemand::value value)
      : BaseObject(env, object),
        document_(std::move(document)),
        value_(std::move(value)) {
    MakeWeak();
  }

  static Local<FunctionTemplate> GetConstructorTemplate(Environment* env);

  static BaseObjectPtr<JsonValue> Create(
      Environment* env,
      BaseObjectPtr<JsonDocument> document,
      simdjson::ondemand::value value);

  static void Type(const FunctionCallbackInfo<Value>& args);
  static void GetString(const FunctionCallbackInfo<Value>& args);
  static void GetNumber(const FunctionCallbackInfo<Value>& args);
  static void GetBoolean(const FunctionCallbackInfo<Value>& args);
  static void IsNull(const FunctionCallbackInfo<Value>& args);

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(JsonValue)
  SET_SELF_SIZE(JsonValue)

 private:
  BaseObjectPtr<JsonDocument> document_;
  simdjson::ondemand::value value_;
};

Local<FunctionTemplate> JsonValue::GetConstructorTemplate(Environment* env) {
  Local<FunctionTemplate> tmpl = env->json_value_constructor_template();
  if (tmpl.IsEmpty()) {
    Isolate* isolate = env->isolate();
    tmpl = NewFunctionTemplate(isolate, nullptr);
    tmpl->SetClassName(FIXED_ONE_BYTE_STRING(isolate, "JsonValue"));
    SetProtoMethod(isolate, tmpl, "type", JsonValue::Type);
    SetProtoMethod(isolate, tmpl, "getString", JsonValue::GetString);
    SetProtoMethod(isolate, tmpl, "getNumber", JsonValue::GetNumber);
    SetProtoMethod(isolate, tmpl, "getBoolean", JsonValue::GetBoolean);
    SetProtoMethod(isolate, tmpl, "isNull", JsonValue::IsNull);
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        JsonValue::kInternalFieldCount);
    env->set_json_value_constructor_template(tmpl);
  }
  return tmpl;
}

BaseObjectPtr<JsonValue> JsonValue::Create(
    Environment* env,
    BaseObjectPtr<JsonDocument> document,
    simdjson::ondemand::value value) {
  Local<Object> obj;

  if (!GetConstructorTemplate(env)
           ->InstanceTemplate()
           ->NewInstance(env->context())
           .ToLocal(&obj)) {
    return nullptr;
  }

  return MakeBaseObject<JsonValue>(
      env, obj, std::move(document), std::move(value));
}

void JsonValue::Type(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  JsonValue* json_value;
  ASSIGN_OR_RETURN_UNWRAP(&json_value, args.This());

  simdjson::ondemand::json_type type;
  auto error = json_value->value_.type().get(type);
  if (error) {
    THROW_ERR_INVALID_STATE(env, "Failed to get value type.");
    return;
  }

  args.GetReturnValue().Set(JsonTypeToV8String(args.GetIsolate(), type));
}

void JsonValue::GetString(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  JsonValue* json_value;
  ASSIGN_OR_RETURN_UNWRAP(&json_value, args.This());

  std::string_view str;
  auto error = json_value->value_.get_string().get(str);
  if (error) {
    THROW_ERR_INVALID_STATE(env, "Value is not a string.");
    return;
  }

  args.GetReturnValue().Set(
      String::NewFromUtf8(
          args.GetIsolate(), str.data(), v8::NewStringType::kNormal, str.size())
          .ToLocalChecked());
}

void JsonValue::GetNumber(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  JsonValue* json_value;
  ASSIGN_OR_RETURN_UNWRAP(&json_value, args.This());

  double num;
  auto error = json_value->value_.get_double().get(num);
  if (error) {
    THROW_ERR_INVALID_STATE(env, "Value is not a number.");
    return;
  }

  args.GetReturnValue().Set(Number::New(args.GetIsolate(), num));
}

void JsonValue::GetBoolean(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  JsonValue* json_value;
  ASSIGN_OR_RETURN_UNWRAP(&json_value, args.This());

  bool val;
  auto error = json_value->value_.get_bool().get(val);
  if (error) {
    THROW_ERR_INVALID_STATE(env, "Value is not a boolean.");
    return;
  }

  args.GetReturnValue().Set(Boolean::New(args.GetIsolate(), val));
}

void JsonValue::IsNull(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  JsonValue* json_value;
  ASSIGN_OR_RETURN_UNWRAP(&json_value, args.This());

  bool val;
  auto error = json_value->value_.is_null().get(val);
  if (error) {
    THROW_ERR_INVALID_STATE(env, "Failed to check if value is null.");
    return;
  }
  args.GetReturnValue().Set(val);
}

void JsonDocument::Type(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  JsonDocument* doc;
  ASSIGN_OR_RETURN_UNWRAP(&doc, args.This());

  simdjson::ondemand::json_type type;
  auto error = doc->document_.type().get(type);
  if (error) {
    THROW_ERR_INVALID_STATE(env, "Failed to get document type.");
    return;
  }

  args.GetReturnValue().Set(JsonTypeToV8String(args.GetIsolate(), type));
}

void JsonDocument::Root(const FunctionCallbackInfo<Value>& args) {
  JsonDocument* doc;
  ASSIGN_OR_RETURN_UNWRAP(&doc, args.This());
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  bool is_scalar = false;
  auto error = doc->document_.is_scalar().get(is_scalar);
  if (error) {
    THROW_ERR_INVALID_STATE(env, "Failed to check if document is scalar.");
    return;
  }

  if (is_scalar) {
    simdjson::ondemand::json_type type;
    if (doc->document_.type().get(type)) {
      THROW_ERR_INVALID_STATE(env, "Failed to get document type.");
      return;
    }

    switch (type) {
      case simdjson::ondemand::json_type::string: {
        std::string_view str;
        if (doc->document_.get_string().get(str)) {
          THROW_ERR_INVALID_STATE(env, "Failed to get root string.");
          return;
        }
        args.GetReturnValue().Set(
            String::NewFromUtf8(
                isolate, str.data(), v8::NewStringType::kNormal, str.size())
                .ToLocalChecked());
        return;
      }
      case simdjson::ondemand::json_type::number: {
        double num;
        if (doc->document_.get_double().get(num)) {
          THROW_ERR_INVALID_STATE(env, "Failed to get root number.");
          return;
        }
        args.GetReturnValue().Set(Number::New(isolate, num));
        return;
      }
      case simdjson::ondemand::json_type::boolean: {
        bool val;
        if (doc->document_.get_bool().get(val)) {
          THROW_ERR_INVALID_STATE(env, "Failed to get root boolean.");
          return;
        }
        args.GetReturnValue().Set(Boolean::New(isolate, val));
        return;
      }
      case simdjson::ondemand::json_type::null: {
        bool is_null;
        if (doc->document_.is_null().get(is_null)) {
          THROW_ERR_INVALID_STATE(env, "Failed to check root null.");
          return;
        }
        args.GetReturnValue().SetNull();
        return;
      }
      default:
        THROW_ERR_INVALID_STATE(env, "Unsupported scalar type.");
        return;
    }
  }

  args.GetReturnValue().Set(args.This());
}

namespace {
Local<v8::DictionaryTemplate> GetIterResultTemplate(Environment* env) {
  auto tmpl = env->iter_template();
  if (tmpl.IsEmpty()) {
    static constexpr std::string_view keys[] = {"done", "value"};
    tmpl = v8::DictionaryTemplate::New(env->isolate(), keys);
    env->set_iter_template(tmpl);
  }
  return tmpl;
}
}  // namespace

class JsonArrayIterator : public BaseObject {
 public:
  JsonArrayIterator(Environment* env,
                    Local<Object> object,
                    BaseObjectPtr<JsonDocument> document,
                    simdjson::ondemand::array array,
                    simdjson::ondemand::array_iterator iter,
                    simdjson::ondemand::array_iterator end)
      : BaseObject(env, object),
        document_(std::move(document)),
        array_(std::move(array)),
        iter_(std::move(iter)),
        end_(std::move(end)),
        done_(false) {
    MakeWeak();
  }

  static Local<FunctionTemplate> GetConstructorTemplate(Environment* env);

  static BaseObjectPtr<JsonArrayIterator> Create(
      Environment* env, BaseObjectPtr<JsonDocument> document,
      simdjson::ondemand::array array);

  static void Next(const FunctionCallbackInfo<Value>& args);
  static void Return(const FunctionCallbackInfo<Value>& args);

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(JsonArrayIterator)
  SET_SELF_SIZE(JsonArrayIterator)

 private:
  BaseObjectPtr<JsonDocument> document_;
  simdjson::ondemand::array array_;
  simdjson::ondemand::array_iterator iter_;
  simdjson::ondemand::array_iterator end_;
  bool done_;
};

Local<FunctionTemplate> JsonArrayIterator::GetConstructorTemplate(
    Environment* env) {
  Local<FunctionTemplate> tmpl =
      env->json_array_iterator_constructor_template();
  if (tmpl.IsEmpty()) {
    Isolate* isolate = env->isolate();
    tmpl = NewFunctionTemplate(isolate, IllegalConstructor);
    tmpl->SetClassName(
        FIXED_ONE_BYTE_STRING(isolate, "JsonArrayIterator"));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        JsonArrayIterator::kInternalFieldCount);
    SetProtoMethod(isolate, tmpl, "next", JsonArrayIterator::Next);
    SetProtoMethod(isolate, tmpl, "return", JsonArrayIterator::Return);
    env->set_json_array_iterator_constructor_template(tmpl);
  }
  return tmpl;
}

BaseObjectPtr<JsonArrayIterator> JsonArrayIterator::Create(
    Environment* env,
    BaseObjectPtr<JsonDocument> document,
    simdjson::ondemand::array array) {
  Local<Object> obj;

  if (!GetConstructorTemplate(env)
           ->InstanceTemplate()
           ->NewInstance(env->context())
           .ToLocal(&obj)) {
    return nullptr;
  }

  simdjson::ondemand::array_iterator begin;
  auto begin_err = array.begin().get(begin);
  if (begin_err) {
    THROW_ERR_INVALID_STATE(env, "Failed to begin array iteration.");
    return nullptr;
  }

  simdjson::ondemand::array_iterator end;
  auto end_err = array.end().get(end);
  if (end_err) {
    THROW_ERR_INVALID_STATE(env, "Failed to get array end iterator.");
    return nullptr;
  }

  return MakeBaseObject<JsonArrayIterator>(
      env, obj, std::move(document), std::move(array),
      std::move(begin), std::move(end));
}

void JsonArrayIterator::Next(const FunctionCallbackInfo<Value>& args) {
  JsonArrayIterator* iter;
  ASSIGN_OR_RETURN_UNWRAP(&iter, args.This());
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  auto iter_template = GetIterResultTemplate(env);

  if (iter->done_ || iter->iter_ == iter->end_) {
    iter->done_ = true;
    MaybeLocal<Value> values[] = {
        Boolean::New(isolate, true),
        v8::Null(isolate),
    };
    Local<Object> result;
    if (NewDictionaryInstanceNullProto(env->context(), iter_template, values)
            .ToLocal(&result)) {
      args.GetReturnValue().Set(result);
    }
    return;
  }

  simdjson::ondemand::value element;
  auto elem_error = (*iter->iter_).get(element);
  ++(iter->iter_);
  if (elem_error) {
    THROW_ERR_INVALID_STATE(env, "Failed to get array element.");
    return;
  }

  auto json_value =
      JsonValue::Create(env, iter->document_, std::move(element));
  if (!json_value) return;

  MaybeLocal<Value> values[] = {
      Boolean::New(isolate, false),
      json_value->object(),
  };
  Local<Object> result;
  if (NewDictionaryInstanceNullProto(env->context(), iter_template, values)
          .ToLocal(&result)) {
    args.GetReturnValue().Set(result);
  }
}

void JsonArrayIterator::Return(const FunctionCallbackInfo<Value>& args) {
  JsonArrayIterator* iter;
  ASSIGN_OR_RETURN_UNWRAP(&iter, args.This());
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  iter->done_ = true;

  auto iter_template = GetIterResultTemplate(env);
  MaybeLocal<Value> values[] = {
      Boolean::New(isolate, true),
      v8::Null(isolate),
  };
  Local<Object> result;
  if (NewDictionaryInstanceNullProto(env->context(), iter_template, values)
          .ToLocal(&result)) {
    args.GetReturnValue().Set(result);
  }
}

void JsonDocument::GetArray(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  JsonDocument* doc;
  ASSIGN_OR_RETURN_UNWRAP(&doc, args.This());

  simdjson::ondemand::array arr;
  auto error = doc->document_.get_array().get(arr);
  if (error) {
    THROW_ERR_INVALID_STATE(env, "Document root is not an array.");
    return;
  }

  auto iter = JsonArrayIterator::Create(
      env, BaseObjectPtr<JsonDocument>(doc), std::move(arr));
  if (!iter) return;

  Local<Context> context = env->context();
  Local<Object> global = context->Global();
  Local<Value> js_iterator;
  Local<Value> js_iterator_prototype;

  if (!global->Get(context, env->iterator_string()).ToLocal(&js_iterator) ||
      !js_iterator.As<Object>()
           ->Get(context, env->prototype_string())
           .ToLocal(&js_iterator_prototype)) {
    return;
  }

  if (iter->object()
          ->GetPrototypeV2()
          .As<Object>()
          ->SetPrototypeV2(context, js_iterator_prototype)
          .IsNothing()) {
    return;
  }

  args.GetReturnValue().Set(iter->object());
}

simdjson::padded_string ToPaddedString(Isolate* isolate, Local<Value> value) {
  Utf8Value utf8_value(isolate, value);
  return simdjson::padded_string(utf8_value.ToStringView());
}

class LazyParser : public BaseObject {
 public:
  LazyParser(Environment* env, Local<Object> obj);
  ~LazyParser();

  static void New(const FunctionCallbackInfo<Value>& args);
  static void Parse(const FunctionCallbackInfo<Value>& args);

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(JsonParser)
  SET_SELF_SIZE(LazyParser)

 private:
  simdjson::ondemand::parser parser_;
};

LazyParser::LazyParser(Environment* env, Local<Object> obj)
    : BaseObject(env, obj) {
  MakeWeak();

  parser_ = simdjson::ondemand::parser();
}

LazyParser::~LazyParser() = default;

void LazyParser::Parse(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  LazyParser* parser;
  ASSIGN_OR_RETURN_UNWRAP(&parser, args.This());

  Isolate* isolate = env->isolate();

  if (args.Length() < 1 || !args[0]->IsString()) {
    THROW_ERR_INVALID_ARG_TYPE(isolate,
                               "The \"text\" argument must be a string.");
    return;
  }

  Local<String> text = args[0].As<String>();

  simdjson::padded_string padded_string = ToPaddedString(isolate, text);

  simdjson::ondemand::document document;

  auto error = parser->parser_.iterate(padded_string).get(document);

  if (error) {
    // TODO(araujogui): create a ERR_INVALID_JSON macro
    THROW_ERR_SOURCE_PHASE_NOT_DEFINED(
        isolate, "The \"json\" argument must be valid JSON.");
    return;
  }

  BaseObjectPtr<JsonDocument> json_document =
      JsonDocument::Create(env, std::move(padded_string), std::move(document));

  args.GetReturnValue().Set(json_document->object());
}

void LazyParser::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (!args.IsConstructCall()) {
    THROW_ERR_CONSTRUCT_CALL_REQUIRED(env);
    return;
  }

  new LazyParser(env, args.This());
}

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();

  Local<FunctionTemplate> parser_tmpl =
      NewFunctionTemplate(isolate, LazyParser::New);

  parser_tmpl->InstanceTemplate()->SetInternalFieldCount(
      LazyParser::kInternalFieldCount);

  SetProtoMethodNoSideEffect(isolate, parser_tmpl, "parse", LazyParser::Parse);

  SetConstructorFunction(context, target, "Parser", parser_tmpl);
}

}  // namespace json_parser
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(json_parser, node::json_parser::Initialize)
