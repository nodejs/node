// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_CORE_ENVIRONMENT_H_
#define V8_EXPERIMENTAL_REDLINE_CORE_ENVIRONMENT_H_

#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "experimental/redline/core/builder.h"

namespace redline {

// A method or function: name, arity range and result type guess.
struct BuiltinFunction {
  std::string_view name;
  int min_args = 0;
  int max_args = 0;
  JsType result = JsType::kUnknown;
  // Argument type hints (kUnknown = anything), at most 3 used.
  JsType args[3] = {JsType::kUnknown, JsType::kUnknown, JsType::kUnknown};
};

// Methods and properties available on values of a given coarse type.
struct BuiltinType {
  JsType type = JsType::kUnknown;
  std::vector<BuiltinFunction> methods;
  std::vector<std::string_view> properties;
};

// A global binding: constructor (Array, Map, ...), namespace (Math, JSON)
// or function (parseInt).
struct BuiltinGlobal {
  std::string_view name;
  JsType type = JsType::kUnknown;           // type of the global itself
  bool constructible = false;               // `new Name(...)` makes sense
  JsType instance_type = JsType::kUnknown;  // type of `new Name()` / `Name()`
  int min_args = 0;
  int max_args = 0;
  std::vector<BuiltinFunction> static_methods;
  std::vector<std::string_view> static_properties;
};

// V8 runtime functions usable with --allow-natives-syntax --fuzzing.
struct NativeFunction {
  std::string_view name;
  int args = 0;  // number of arguments
  JsType arg_type =
      JsType::kUnknown;  // type of the (first) argument, kFunction for most
};

// A callable member discovered on the target: a static method, a prototype
// method or a getter (arity -1).
struct DiscoveredMember {
  std::string_view name;
  int arity = 0;  // Function.length; -1 for getters/plain properties
};

// A global (or dotted path such as "Intl.NumberFormat" or "d8.test") the
// target exposes, as reported by Environment::IntrospectionScript().
struct DiscoveredApi {
  enum class Kind { kFunction, kConstructor, kNamespace };
  std::string_view path;
  Kind kind = Kind::kNamespace;
  int arity = 0;                          // for functions/constructors
  std::vector<DiscoveredMember> statics;  // own function-valued properties
  std::vector<DiscoveredMember> methods;  // prototype methods (constructors)
  std::vector<DiscoveredMember> getters;  // prototype accessors
};

// The static model of what d8 offers: a hand-written, typed core (what the
// generators use for type-directed code) plus everything the target itself
// reports about its globals when the session starts (what the DiscoveredApi
// generator and the Explore mutator use to reach builtins nobody modelled).
class Environment {
 public:
  static const Environment& Default();
  // The hand-written model extended with the target's answer to
  // IntrospectionScript() (the JSON it printed on fuzzout). Returns nullptr
  // with *error set if the JSON is unusable.
  static std::unique_ptr<Environment> WithDiscovered(const std::string& json,
                                                     std::string* error);
  // JavaScript that enumerates the target's globals and prints them as one
  // JSON line via fuzzilli('FUZZILLI_PRINT', ...).
  static std::string_view IntrospectionScript();

  const std::vector<BuiltinGlobal>& globals() const { return globals_; }
  const std::vector<BuiltinType>& types() const { return types_; }
  const std::vector<NativeFunction>& natives() const { return natives_; }
  const std::vector<DiscoveredApi>& discovered() const { return discovered_; }
  // Common property names for synthesized objects/accesses.
  const std::vector<std::string_view>& property_names() const {
    return property_names_;
  }
  // Interesting numbers, strings and regexps.
  const std::vector<double>& numbers() const { return numbers_; }
  const std::vector<std::string_view>& strings() const { return strings_; }
  const std::vector<std::pair<std::string_view, std::string_view>>& regexps()
      const {
    return regexps_;
  }

  const BuiltinType* ForType(JsType type) const;
  const BuiltinGlobal* Global(std::string_view name) const;
  // Globals whose instance_type is `type` and that are constructible.
  std::vector<const BuiltinGlobal*> ConstructorsOf(JsType type) const;
  // Number of discovered members (statics + methods + getters), for logs.
  size_t discovered_member_count() const;

 private:
  Environment();
  std::string_view Intern(std::string_view text);

  std::vector<BuiltinGlobal> globals_;
  std::vector<BuiltinType> types_;
  std::vector<NativeFunction> natives_;
  std::vector<std::string_view> property_names_;
  std::vector<double> numbers_;
  std::vector<std::string_view> strings_;
  std::vector<std::pair<std::string_view, std::string_view>> regexps_;
  std::vector<DiscoveredApi> discovered_;
  // Backing storage for the string_views of discovered entries.
  std::deque<std::string> owned_strings_;
};

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_CORE_ENVIRONMENT_H_
