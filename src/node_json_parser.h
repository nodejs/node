#ifndef SRC_NODE_JSON_PARSER_H_
#define SRC_NODE_JSON_PARSER_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
#include "node_external_reference.h"
#include "simdjson.h"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace node {
namespace json_parser {

enum class SchemaType {
  kString,
  kNumber,
  kInteger,
  kBoolean,
  kNull,
  kObject,
  kArray
};

struct StringViewHash {
  using is_transparent = void;
  size_t operator()(std::string_view sv) const noexcept {
    return std::hash<std::string_view>{}(sv);
  }
};

struct SchemaNode;

struct PropertyEntry {
  std::unique_ptr<SchemaNode> node;
  v8::Global<v8::String> v8_key;
};

struct SchemaNode {
  SchemaType type;
  std::
      unordered_map<std::string, PropertyEntry, StringViewHash, std::equal_to<>>
          properties;
  std::unique_ptr<SchemaNode> items;
};

class JsonParser : public BaseObject {
 public:
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Parse(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Initialize(v8::Local<v8::Object> target,
                         v8::Local<v8::Value> unused,
                         v8::Local<v8::Context> context,
                         void* priv);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

  JsonParser(Environment* env,
             v8::Local<v8::Object> obj,
             std::unique_ptr<SchemaNode> schema);

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(JsonParser)
  SET_SELF_SIZE(JsonParser)

 private:
  simdjson::ondemand::parser parser_;
  std::string padded_buffer_;
  std::unique_ptr<SchemaNode> schema_;
  v8::Global<v8::Value> object_proto_;

  static v8::Maybe<std::unique_ptr<SchemaNode>> ParseSchema(
      v8::Isolate* isolate,
      v8::Local<v8::Context> ctx,
      v8::Local<v8::Object> schema_obj);

  static bool ParseObjectProperties(v8::Isolate* isolate,
                                    v8::Local<v8::Context> ctx,
                                    v8::Local<v8::Object> props_obj,
                                    SchemaNode* node);

  v8::MaybeLocal<v8::Value> ConvertObject(
      v8::Local<v8::Context> ctx,
      simdjson::ondemand::object obj,
      const SchemaNode* schema,
      std::string_view field_name,
      v8::LocalVector<v8::Name>* names_buf = nullptr,
      v8::LocalVector<v8::Value>* values_buf = nullptr);

  v8::MaybeLocal<v8::Value> ConvertArray(v8::Local<v8::Context> ctx,
                                         simdjson::ondemand::array arr,
                                         const SchemaNode* schema,
                                         std::string_view field_name);

  v8::MaybeLocal<v8::Value> ConvertValue(v8::Local<v8::Context> ctx,
                                         simdjson::ondemand::value& val,
                                         const SchemaNode* schema,
                                         std::string_view field_name);
};

}  // namespace json_parser
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_NODE_JSON_PARSER_H_
