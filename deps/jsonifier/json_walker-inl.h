#ifndef DEPS_JSONIFIER_JSON_WALKER_INL_H_
#define DEPS_JSONIFIER_JSON_WALKER_INL_H_

namespace node {
namespace modules {
namespace NODE_JSONIFIER_ISA_NS {

using GenericDocument = jsonifier::generic::generic_document<false>;
using GenericObject = jsonifier::generic::generic_object<false>;
using GenericArray = jsonifier::generic::generic_array<false>;
using GenericField = jsonifier::generic::generic_field<false>;
using GenericValue = jsonifier::generic::generic_value<false>;

class JsonWalker {
 public:
  static std::string_view Walk(const GenericValue& value,
                               json::JsonEventSink* sink,
                               size_t depth) {
    if (depth > kMaxDepth) {
      return kTooDeep;
    }
    switch (value.type()) {
      case jsonifier::json_types::object:
        return WalkObject(value, sink, depth);
      case jsonifier::json_types::array:
        return WalkArray(value, sink, depth);
      case jsonifier::json_types::string: {
        std::string_view text;
        if (const auto code = value.getString(text); code != kSuccess) {
          return Name(code);
        }
        sink->String(text);
        return {};
      }
      case jsonifier::json_types::number: {
        jsonifier::string_view raw{};
        double parsed;
        if (const auto code = value.rawJson(raw); code != kSuccess) {
          return Name(code);
        }
        if (const auto code = value.getDouble(parsed); code != kSuccess) {
          return Name(code);
        }
        sink->Number(ToView(raw), parsed);
        return {};
      }
      case jsonifier::json_types::boolean: {
        bool parsed;
        if (const auto code = value.getBool(parsed); code != kSuccess) {
          return Name(code);
        }
        sink->Bool(parsed);
        return {};
      }
      case jsonifier::json_types::null:
        if (!value.isNull()) {
          return Name(jsonifier::generic::error_codes::n_atom_error);
        }
        sink->Null();
        return {};
      default:
        return Name(jsonifier::generic::error_codes::tape_error);
    }
  }

  static std::string_view Name(jsonifier::generic::error_codes code) {
    return jsonifier::internal::getName(code)
        .operator std::basic_string_view<char>();
  }

 private:
  static constexpr size_t kMaxDepth = 1024;
  static constexpr std::string_view kTooDeep = "depth_limit";
  static constexpr jsonifier::generic::error_codes kSuccess =
      jsonifier::generic::error_codes::success;

  static std::string_view WalkObject(const GenericValue& value,
                                     json::JsonEventSink* sink,
                                     size_t depth) {
    GenericObject object = value.getObject();
    if (object.error() != kSuccess) {
      return Name(object.error());
    }
    sink->StartObject();
    for (GenericField field : object) {
      std::string_view key;
      if (field.error() != kSuccess) {
        return Name(field.error());
      }
      if (const auto code = field.unescapedKey(key); code != kSuccess) {
        return Name(code);
      }
      sink->Key(key);
      if (const std::string_view error = Walk(field.value(), sink, depth + 1);
          !error.empty()) {
        return error;
      }
    }
    jsonifier::string_view raw{};
    if (const auto code = value.rawJson(raw); code != kSuccess) {
      return Name(code);
    }
    sink->EndObject(ToView(raw));
    return {};
  }

  static std::string_view WalkArray(const GenericValue& value,
                                    json::JsonEventSink* sink,
                                    size_t depth) {
    GenericArray array = value.getArray();
    if (array.error() != kSuccess) {
      return Name(array.error());
    }
    sink->StartArray();
    for (GenericValue element : array) {
      if (element.error() != kSuccess) {
        return Name(element.error());
      }
      if (const std::string_view error = Walk(element, sink, depth + 1);
          !error.empty()) {
        return error;
      }
    }
    jsonifier::string_view raw{};
    if (const auto code = value.rawJson(raw); code != kSuccess) {
      return Name(code);
    }
    sink->EndArray(ToView(raw));
    return {};
  }
};

std::string_view WalkJson(std::string_view json, json::JsonEventSink* sink) {
  if (json.starts_with(kJsonifierUtf8Bom)) {
    json.remove_prefix(kJsonifierUtf8Bom.size());
  }
  jsonifier::jsonifier_core parser;
  GenericDocument document = parser.iterate(json);
  if (document.error() != jsonifier::generic::error_codes::success) {
    return JsonWalker::Name(document.error());
  }
  return JsonWalker::Walk(document, sink, 0);
}

}  // namespace NODE_JSONIFIER_ISA_NS
}  // namespace modules
}  // namespace node

#endif  // DEPS_JSONIFIER_JSON_WALKER_INL_H_
