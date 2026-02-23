#include "node_json_parser.h"
#include "base_object-inl.h"
#include "node_errors.h"
#include "util-inl.h"

#include <cstring>
#include <memory>
#include <string>
#include <string_view>

namespace node {
namespace json_parser {

using v8::Array;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Isolate;
using v8::Just;
using v8::Local;
using v8::LocalVector;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Name;
using v8::Nothing;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Value;

static inline void ThrowParseError(Isolate* isolate, std::string_view msg) {
  Local<String> s;
  if (String::NewFromUtf8(
          isolate, msg.data(), v8::NewStringType::kNormal, msg.size())
          .ToLocal(&s)) {
    isolate->ThrowException(v8::Exception::Error(s));
  }
}

static inline void ThrowFieldError(Isolate* isolate,
                            std::string_view prefix,
                            std::string_view field,
                            std::string_view suffix = {}) {
  std::string msg;
  msg.reserve(prefix.size() + field.size() + suffix.size());
  msg.append(prefix);
  msg.append(field);
  msg.append(suffix);
  ThrowParseError(isolate, msg);
}

static inline void ThrowFieldTypeError(Isolate* isolate,
                                std::string_view field_name,
                                const char* expected) {
  std::string msg;
  msg.reserve(8 + field_name.size() + 12 + strlen(expected));
  msg.append("Field '");
  msg.append(field_name);
  msg.append("': expected ");
  msg.append(expected);
  ThrowParseError(isolate, msg);
}

static Maybe<SchemaType> ParseSchemaType(Isolate* isolate,
                                         std::string_view sv) {
  if (sv == "string") return Just(SchemaType::kString);
  if (sv == "number") return Just(SchemaType::kNumber);
  if (sv == "integer") return Just(SchemaType::kInteger);
  if (sv == "boolean") return Just(SchemaType::kBoolean);
  if (sv == "null") return Just(SchemaType::kNull);
  if (sv == "object") return Just(SchemaType::kObject);
  if (sv == "array") return Just(SchemaType::kArray);
  std::string msg = "Unsupported schema type: ";
  msg += sv;
  THROW_ERR_INVALID_ARG_VALUE(isolate, msg.c_str());
  return Nothing<SchemaType>();
}

JsonParser::JsonParser(Environment* env,
                       Local<Object> obj,
                       std::unique_ptr<SchemaNode> schema)
    : BaseObject(env, obj), schema_(std::move(schema)) {
  Isolate* isolate = env->isolate();
  object_proto_.Reset(isolate, Object::New(isolate)->GetPrototypeV2());
  MakeWeak();
}

Maybe<std::unique_ptr<SchemaNode>> JsonParser::ParseSchema(
    Isolate* isolate, Local<Context> ctx, Local<Object> schema_obj) {
  Local<Value> type_val;
  if (!schema_obj->Get(ctx, FIXED_ONE_BYTE_STRING(isolate, "type"))
           .ToLocal(&type_val)) {
    return Nothing<std::unique_ptr<SchemaNode>>();
  }
  if (!type_val->IsString()) {
    THROW_ERR_INVALID_ARG_TYPE(isolate, "schema 'type' must be a string");
    return Nothing<std::unique_ptr<SchemaNode>>();
  }

  Utf8Value type_str(isolate, type_val);
  std::string_view type_sv(*type_str, type_str.length());

  auto type_result = ParseSchemaType(isolate, type_sv);
  if (type_result.IsNothing()) return Nothing<std::unique_ptr<SchemaNode>>();

  auto node = std::make_unique<SchemaNode>();
  node->type = type_result.FromJust();

  switch (node->type) {
    case SchemaType::kObject: {
      Local<Value> props_val;
      if (!schema_obj->Get(ctx, FIXED_ONE_BYTE_STRING(isolate, "properties"))
               .ToLocal(&props_val)) {
        return Nothing<std::unique_ptr<SchemaNode>>();
      }
      if (!props_val->IsUndefined() && !props_val->IsNull()) {
        if (!props_val->IsObject()) {
          THROW_ERR_INVALID_ARG_TYPE(isolate,
                                     "schema 'properties' must be an object");
          return Nothing<std::unique_ptr<SchemaNode>>();
        }
        if (!ParseObjectProperties(
                isolate, ctx, props_val.As<Object>(), node.get()))
          return Nothing<std::unique_ptr<SchemaNode>>();
      }
      break;
    }
    case SchemaType::kArray: {
      Local<Value> items_val;
      if (!schema_obj->Get(ctx, FIXED_ONE_BYTE_STRING(isolate, "items"))
               .ToLocal(&items_val)) {
        return Nothing<std::unique_ptr<SchemaNode>>();
      }
      if (!items_val->IsUndefined() && !items_val->IsNull()) {
        if (!items_val->IsObject()) {
          THROW_ERR_INVALID_ARG_TYPE(isolate,
                                     "schema 'items' must be an object");
          return Nothing<std::unique_ptr<SchemaNode>>();
        }
        auto items_result = ParseSchema(isolate, ctx, items_val.As<Object>());
        if (items_result.IsNothing()) {
          return Nothing<std::unique_ptr<SchemaNode>>();
        }
        node->items = std::move(items_result).FromJust();
      }
      break;
    }
    default:
      break;
  }

  return Just(std::move(node));
}

bool JsonParser::ParseObjectProperties(Isolate* isolate,
                                       Local<Context> ctx,
                                       Local<Object> props_obj,
                                       SchemaNode* node) {
  Local<Array> prop_names;
  if (!props_obj->GetOwnPropertyNames(ctx).ToLocal(&prop_names)) return false;

  for (uint32_t i = 0; i < prop_names->Length(); i++) {
    Local<Value> key;
    if (!prop_names->Get(ctx, i).ToLocal(&key)) return false;

    Local<Value> child_val;
    if (!props_obj->Get(ctx, key).ToLocal(&child_val)) return false;

    if (!child_val->IsObject()) {
      THROW_ERR_INVALID_ARG_TYPE(isolate, "schema property must be an object");
      return false;
    }

    auto child_result = ParseSchema(isolate, ctx, child_val.As<Object>());
    if (child_result.IsNothing()) return false;

    Utf8Value key_str(isolate, key);
    PropertyEntry entry;
    entry.node = std::move(child_result).FromJust();
    entry.v8_key.Reset(isolate, key.As<String>());
    node->properties.emplace(std::string(*key_str, key_str.length()),
                             std::move(entry));
  }
  return true;
}

void JsonParser::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (!args.IsConstructCall()) {
    THROW_ERR_CONSTRUCT_CALL_REQUIRED(env);
    return;
  }

  if (!args[0]->IsObject()) {
    THROW_ERR_INVALID_ARG_TYPE(env, "schema must be an object");
    return;
  }

  Local<Context> ctx = env->context();
  auto schema_result = ParseSchema(env->isolate(), ctx, args[0].As<Object>());
  if (schema_result.IsNothing()) return;  // JS exception already set

  new JsonParser(env, args.This(), std::move(schema_result).FromJust());
}

MaybeLocal<Value> JsonParser::ConvertValue(Local<Context> ctx,
                                           simdjson::ondemand::value& val,
                                           const SchemaNode* schema,
                                           std::string_view field_name) {
  Isolate* isolate = env()->isolate();

  switch (schema->type) {
    case SchemaType::kString: {
      std::string_view sv;
      if (val.get_string().get(sv)) {
        ThrowFieldTypeError(isolate, field_name, "string");
        return {};
      }
      Local<String> s;
      if (!String::NewFromUtf8(
               isolate, sv.data(), v8::NewStringType::kNormal, sv.size())
               .ToLocal(&s)) {
        return {};
      }
      return s;
    }
    case SchemaType::kInteger: {
      int64_t i;
      if (val.get_int64().get(i)) {
        ThrowFieldTypeError(isolate, field_name, "integer");
        return {};
      }
      return Number::New(isolate, static_cast<double>(i));
    }
    case SchemaType::kNumber: {
      double d;
      if (val.get_double().get(d)) {
        ThrowFieldTypeError(isolate, field_name, "number");
        return {};
      }
      return Number::New(isolate, d);
    }
    case SchemaType::kBoolean: {
      bool b;
      if (val.get_bool().get(b)) {
        ThrowFieldTypeError(isolate, field_name, "boolean");
        return {};
      }
      return v8::Boolean::New(isolate, b);
    }
    case SchemaType::kNull: {
      bool is_null_val = false;
      if (val.is_null().get(is_null_val) || !is_null_val) {
        ThrowFieldTypeError(isolate, field_name, "null");
        return {};
      }
      return v8::Null(isolate).As<Value>();
    }
    case SchemaType::kObject: {
      simdjson::ondemand::object obj;
      if (val.get_object().get(obj)) {
        ThrowFieldTypeError(isolate, field_name, "object");
        return {};
      }
      return ConvertObject(ctx, std::move(obj), schema, field_name);
    }
    case SchemaType::kArray: {
      simdjson::ondemand::array arr;
      if (val.get_array().get(arr)) {
        ThrowFieldTypeError(isolate, field_name, "array");
        return {};
      }
      return ConvertArray(ctx, std::move(arr), schema, field_name);
    }
  }
  UNREACHABLE();
}

MaybeLocal<Value> JsonParser::ConvertObject(Local<Context> ctx,
                                            simdjson::ondemand::object obj,
                                            const SchemaNode* schema,
                                            std::string_view field_name,
                                            LocalVector<Name>* names_buf,
                                            LocalVector<Value>* values_buf) {
  Isolate* isolate = env()->isolate();
  LocalVector<Name> local_names(isolate);
  LocalVector<Value> local_values(isolate);
  LocalVector<Name>& names = names_buf ? *names_buf : local_names;
  LocalVector<Value>& values = values_buf ? *values_buf : local_values;
  names.clear();
  values.clear();
  names.reserve(schema->properties.size());
  values.reserve(schema->properties.size());

  std::string_view key_sv;
  simdjson::ondemand::value field_val;

  for (auto field : obj) {
    if (field.unescaped_key().get(key_sv)) {
      ThrowFieldError(
          isolate, "JSON parse error reading key in '", field_name, "'");
      return {};
    }

    auto it = schema->properties.find(key_sv);
    if (it == schema->properties.end()) {
      // Skip — don't call field.value() so simdjson auto-skips the value
      continue;
    }

    if (field.value().get(field_val)) {
      ThrowFieldError(isolate, "JSON parse error reading field '", key_sv, "'");
      return {};
    }

    MaybeLocal<Value> converted =
        ConvertValue(ctx, field_val, it->second.node.get(), key_sv);
    if (converted.IsEmpty()) return {};

    names.push_back(Local<String>::New(isolate, it->second.v8_key).As<Name>());
    values.push_back(converted.ToLocalChecked());
  }

  return Object::New(isolate,
                     Local<Value>::New(isolate, object_proto_),
                     names.data(),
                     values.data(),
                     names.size());
}

MaybeLocal<Value> JsonParser::ConvertArray(Local<Context> ctx,
                                           simdjson::ondemand::array arr,
                                           const SchemaNode* schema,
                                           std::string_view field_name) {
  Isolate* isolate = env()->isolate();
  LocalVector<Value> elems(isolate);
  elems.reserve(8);  // reasonable default; push_back handles growth
  simdjson::ondemand::value elem_val;

  // Fast path: pre-allocate name/value buffers once for array-of-objects.
  LocalVector<Name> names_buf(isolate);
  LocalVector<Value> values_buf(isolate);
  const bool items_is_object =
      schema->items && schema->items->type == SchemaType::kObject;
  if (items_is_object) {
    names_buf.reserve(schema->items->properties.size());
    values_buf.reserve(schema->items->properties.size());
  }

  for (auto elem_result : arr) {
    if (std::move(elem_result).get(elem_val)) {
      ThrowFieldError(isolate,
                      "JSON parse error reading array element in '",
                      field_name,
                      "'");
      return {};
    }

    MaybeLocal<Value> converted;
    if (items_is_object) {
      // schema->items is non-null by construction of items_is_object
      simdjson::ondemand::object obj;
      if (elem_val.get_object().get(obj)) {
        ThrowFieldError(isolate, "Field '", field_name, "': expected object");
        return {};
      }
      converted = ConvertObject(ctx,
                                std::move(obj),
                                schema->items.get(),
                                field_name,
                                &names_buf,
                                &values_buf);
    } else if (schema->items == nullptr) {
      elems.emplace_back(v8::Undefined(isolate));
      continue;
    } else {
      converted = ConvertValue(ctx, elem_val, schema->items.get(), field_name);
    }

    if (converted.IsEmpty()) return {};
    elems.emplace_back(converted.ToLocalChecked());
  }

  return Array::New(isolate, elems.data(), elems.size());
}

void JsonParser::Parse(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  JsonParser* self;
  ASSIGN_OR_RETURN_UNWRAP(&self, args.This());

  Isolate* isolate = env->isolate();
  Local<Context> ctx = env->context();

  if (!args[0]->IsString()) {
    return THROW_ERR_INVALID_ARG_TYPE(env, "input must be a string");
  }

  Local<String> str = args[0].As<String>();
  size_t len;
  if (str->IsOneByte()) {
    const uint32_t char_len = str->Length();
    self->padded_buffer_.resize(char_len + simdjson::SIMDJSON_PADDING);
    std::memset(
        self->padded_buffer_.data() + char_len, 0, simdjson::SIMDJSON_PADDING);
    str->WriteOneByteV2(isolate,
                        0,
                        char_len,
                        reinterpret_cast<uint8_t*>(self->padded_buffer_.data()));
    len = char_len;
  } else {
    const size_t utf8_len = str->Utf8LengthV2(isolate);
    self->padded_buffer_.resize(utf8_len + simdjson::SIMDJSON_PADDING);
    std::memset(
        self->padded_buffer_.data() + utf8_len, 0, simdjson::SIMDJSON_PADDING);
    len = str->WriteUtf8V2(isolate, self->padded_buffer_.data(), utf8_len);
  }

  simdjson::padded_string_view view(
      self->padded_buffer_.data(), len, self->padded_buffer_.size());
  simdjson::ondemand::document doc;
  if (self->parser_.iterate(view).get(doc)) {
    isolate->ThrowException(
        v8::Exception::Error(FIXED_ONE_BYTE_STRING(isolate, "Invalid JSON")));
    return;
  }

  MaybeLocal<Value> result;

  switch (self->schema_->type) {
    case SchemaType::kObject: {
      simdjson::ondemand::object obj;
      if (doc.get_object().get(obj)) {
        isolate->ThrowException(v8::Exception::Error(
            FIXED_ONE_BYTE_STRING(isolate, "Expected JSON object at root")));
        return;
      }
      result =
          self->ConvertObject(ctx, std::move(obj), self->schema_.get(), "root");
      break;
    }
    case SchemaType::kArray: {
      simdjson::ondemand::array arr;
      if (doc.get_array().get(arr)) {
        isolate->ThrowException(v8::Exception::Error(
            FIXED_ONE_BYTE_STRING(isolate, "Expected JSON array at root")));
        return;
      }
      result =
          self->ConvertArray(ctx, std::move(arr), self->schema_.get(), "root");
      break;
    }
    case SchemaType::kString: {
      std::string_view sv;
      if (doc.get_string().get(sv)) {
        isolate->ThrowException(v8::Exception::Error(
            FIXED_ONE_BYTE_STRING(isolate, "Expected JSON string at root")));
        return;
      }
      Local<String> s;
      if (!String::NewFromUtf8(
               isolate, sv.data(), v8::NewStringType::kNormal, sv.size())
               .ToLocal(&s)) {
        return;
      }
      result = s;
      break;
    }
    case SchemaType::kInteger: {
      int64_t i;
      if (doc.get_int64().get(i)) {
        isolate->ThrowException(v8::Exception::Error(
            FIXED_ONE_BYTE_STRING(isolate, "Expected JSON integer at root")));
        return;
      }
      result = Number::New(isolate, static_cast<double>(i));
      break;
    }
    case SchemaType::kNumber: {
      double d;
      if (doc.get_double().get(d)) {
        isolate->ThrowException(v8::Exception::Error(
            FIXED_ONE_BYTE_STRING(isolate, "Expected JSON number at root")));
        return;
      }
      result = Number::New(isolate, d);
      break;
    }
    case SchemaType::kBoolean: {
      bool b;
      if (doc.get_bool().get(b)) {
        isolate->ThrowException(v8::Exception::Error(
            FIXED_ONE_BYTE_STRING(isolate, "Expected JSON boolean at root")));
        return;
      }
      result =
          b ? v8::True(isolate).As<Value>() : v8::False(isolate).As<Value>();
      break;
    }
    case SchemaType::kNull: {
      bool is_null_val = false;
      if (doc.is_null().get(is_null_val) || !is_null_val) {
        isolate->ThrowException(v8::Exception::Error(
            FIXED_ONE_BYTE_STRING(isolate, "Expected JSON null at root")));
        return;
      }
      result = v8::Null(isolate).As<Value>();
      break;
    }
    default:
      UNREACHABLE();
  }

  Local<Value> res_val;
  if (result.ToLocal(&res_val)) {
    args.GetReturnValue().Set(res_val);
  }
}

void JsonParser::Initialize(Local<Object> target,
                            Local<Value> /* unused */,
                            Local<Context> context,
                            void* /* priv */) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();

  Local<FunctionTemplate> tmpl = NewFunctionTemplate(isolate, New);
  tmpl->InstanceTemplate()->SetInternalFieldCount(
      BaseObject::kInternalFieldCount);
  tmpl->SetClassName(FIXED_ONE_BYTE_STRING(isolate, "Parser"));

  SetProtoMethod(isolate, tmpl, "parse", Parse);
  SetConstructorFunction(context, target, "Parser", tmpl);
}

// static
void JsonParser::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(New);
  registry->Register(Parse);
}

}  // namespace json_parser
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(json_parser,
                                    node::json_parser::JsonParser::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(
    json_parser, node::json_parser::JsonParser::RegisterExternalReferences)
