// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/core/selection.h"

#include <string>
#include <vector>

namespace redline {

namespace {

const char* TypeName(ParamSpec::Type type) {
  switch (type) {
    case ParamSpec::Type::kNumber:
      return "number";
    case ParamSpec::Type::kBool:
      return "bool";
    case ParamSpec::Type::kString:
      return "string";
    case ParamSpec::Type::kObject:
      return "object";
  }
  return "?";
}

bool HasType(const JsonValue& value, ParamSpec::Type type) {
  switch (type) {
    case ParamSpec::Type::kNumber:
      return value.is_number();
    case ParamSpec::Type::kBool:
      return value.is_bool();
    case ParamSpec::Type::kString:
      return value.is_string();
    case ParamSpec::Type::kObject:
      return value.is_object();
  }
  return false;
}

}  // namespace

bool ValidateParams(std::string_view strategy, const JsonValue& params,
                    const std::vector<ParamSpec>& specs, std::string* error) {
  if (params.is_null()) {
    return true;
  }
  std::string prefix = "selection strategy '" + std::string(strategy) + "': ";
  if (!params.is_object()) {
    *error = prefix + "\"params\" must be an object";
    return false;
  }
  for (const auto& [key, value] : params.as_object()) {
    const ParamSpec* spec = nullptr;
    for (const ParamSpec& s : specs) {
      if (s.name == key) {
        spec = &s;
        break;
      }
    }
    if (spec == nullptr) {
      std::vector<std::string> known;
      known.reserve(specs.size());
      for (const ParamSpec& s : specs) {
        known.emplace_back(s.name);
      }
      *error = prefix;
      *error += "unknown parameter '";
      *error += key;
      *error += '\'';
      if (known.empty()) {
        *error += " (it takes no parameters)";
      } else {
        *error += " (known: ";
        *error += JoinNames(known);
        *error += ')';
      }
      return false;
    }
    if (!HasType(value, spec->type)) {
      *error = prefix;
      *error += "parameter '";
      *error += key;
      *error += "' must be a ";
      *error += TypeName(spec->type);
      return false;
    }
  }
  return true;
}

bool ReadWeightOverrides(const JsonValue& params,
                         std::vector<std::pair<std::string, double>>* out,
                         std::string* error) {
  out->clear();
  if (params.is_null() || !params.Has("weights")) {
    return true;
  }
  const JsonValue& weights = params.Get("weights");
  if (!weights.is_object()) {
    *error = "\"weights\" must be an object of {\"Name\": number}";
    return false;
  }
  for (const auto& [name, value] : weights.as_object()) {
    if (!value.is_number() || value.as_number() < 0) {
      *error = "weight of '" + name + "' must be a non-negative number";
      return false;
    }
    out->emplace_back(name, value.as_number());
  }
  return true;
}

std::string JoinNames(const std::vector<std::string>& names) {
  std::string out;
  for (const std::string& n : names) {
    if (!out.empty()) {
      out += ", ";
    }
    out += n;
  }
  return out;
}

}  // namespace redline
