#include "json_parser.h"
#include <cstdio>
#include "debug_utils.h"

namespace node {

JSONParser::JSONParser() {}

bool JSONParser::Parse(const std::string& content) {
  DCHECK(!parsed_);

  json_content_ = content;
  size_t json_length = json_content_.size();
  json_content_.append(simdjson::SIMDJSON_PADDING, ' ');

  simdjson::padded_string_view json_view(
      json_content_.data(), json_length, json_content_.size());

  simdjson::ondemand::document document;
  simdjson::error_code error = parser_.iterate(json_view).get(document);

  if (error != simdjson::SUCCESS) {
    error_message_ = simdjson::error_message(error);
    std::fprintf(stderr, "%s\n", error_message_.c_str());
    return false;
  }

  simdjson::ondemand::object obj;
  error = document.get_object().get(obj);
  if (error != simdjson::SUCCESS) {
    error_message_ = simdjson::error_message(error);
    std::fprintf(stderr, "%s\n", error_message_.c_str());
    return false;
  }

  parsed_ = true;
  return true;
}

std::optional<std::string> JSONParser::GetTopLevelStringField(
    std::string_view field) {
  if (!parsed_) {
    return {};
  }

  simdjson::padded_string_view json_view(
      json_content_.data(),
      json_content_.size() - simdjson::SIMDJSON_PADDING,
      json_content_.size());

  simdjson::ondemand::document document;
  simdjson::error_code error = parser_.iterate(json_view).get(document);
  if (error != simdjson::SUCCESS) {
    return {};
  }

  simdjson::ondemand::object obj;
  error = document.get_object().get(obj);
  if (error != simdjson::SUCCESS) {
    return {};
  }

  std::string_view result;
  error = obj[field].get_string().get(result);
  if (error != simdjson::SUCCESS) {
    return {};
  }

  return std::string(result);
}

std::optional<bool> JSONParser::GetTopLevelBoolField(std::string_view field) {
  if (!parsed_) {
    return {};
  }

  simdjson::padded_string_view json_view(
      json_content_.data(),
      json_content_.size() - simdjson::SIMDJSON_PADDING,
      json_content_.size());

  simdjson::ondemand::document document;
  simdjson::error_code error = parser_.iterate(json_view).get(document);
  if (error != simdjson::SUCCESS) {
    return {};
  }

  simdjson::ondemand::object obj;
  error = document.get_object().get(obj);
  if (error != simdjson::SUCCESS) {
    return {};
  }

  simdjson::ondemand::value val;
  error = obj[field].get(val);
  if (error != simdjson::SUCCESS) {
    return false;
  }

  bool result;
  error = val.get_bool().get(result);
  if (error != simdjson::SUCCESS) {
    return {};
  }

  return result;
}

std::optional<JSONParser::StringDict> JSONParser::GetTopLevelStringDict(
    std::string_view field) {
  if (!parsed_) {
    return std::nullopt;
  }

  simdjson::padded_string_view json_view(
      json_content_.data(),
      json_content_.size() - simdjson::SIMDJSON_PADDING,
      json_content_.size());

  simdjson::ondemand::document document;
  simdjson::error_code error = parser_.iterate(json_view).get(document);
  if (error != simdjson::SUCCESS) {
    return std::nullopt;
  }

  simdjson::ondemand::object obj;
  error = document.get_object().get(obj);
  if (error != simdjson::SUCCESS) {
    return std::nullopt;
  }

  simdjson::ondemand::value val;
  error = obj[field].get(val);
  if (error != simdjson::SUCCESS) {
    return StringDict();
  }

  simdjson::ondemand::object dict;
  error = val.get_object().get(dict);
  if (error != simdjson::SUCCESS) {
    return std::nullopt;
  }

  StringDict result;
  for (auto field_value : dict) {
    std::string_view key_view;
    error = field_value.unescaped_key().get(key_view);
    if (error != simdjson::SUCCESS) {
      return StringDict();
    }

    std::string_view value_view;
    error = field_value.value().get_string().get(value_view);
    if (error != simdjson::SUCCESS) {
      return StringDict();
    }

    result.emplace(std::string(key_view), std::string(value_view));
  }

  return result;
}

}  // namespace node
