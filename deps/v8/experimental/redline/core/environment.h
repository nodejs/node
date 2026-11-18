// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_CORE_ENVIRONMENT_H_
#define V8_EXPERIMENTAL_REDLINE_CORE_ENVIRONMENT_H_

#include <deque>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "experimental/redline/core/builder.h"

namespace redline {

// What a parameter of a builtin needs beyond its coarse JsType. The CodeGen
// helper ArgumentFor() uses a visible variable of the kind or makes a new
// value of the kind.
enum class ArgKind : uint8_t {
  kAny,       // any value
  kValue,     // a value of the coarse type `type`
  kInteger,   // an integer: small, a boundary or an interesting value
  kIndex,     // an index into the receiver: mostly small, not negative
  kLength,    // a size or a count: mostly small, not negative
  kRange,     // an integer from `min` to `max`
  kIterable,  // an iterable with elements of the type `type`
  kCallback,  // a function that takes `min` parameters
  kOptions,   // an options bag: the keys of Environment::Options(`name`)
  kEnum,      // one of the strings of Environment::Enum(`name`)
  kInstance,  // an object of the builtin type `type`
};

// One parameter of a builtin function, method or constructor.
struct ArgSpec {
  ArgKind kind = ArgKind::kAny;
  JsType type = JsType::kUnknown;
  int min = 0;
  int max = 0;
  std::string_view name = {};
  bool optional = false;  // the caller can stop before this parameter
  bool rest = false;      // the parameter takes 0 or more values
};

// Shorthands for the parameter lists of the model.
namespace arg {
constexpr ArgSpec Make(ArgKind kind, JsType type) {
  ArgSpec a;
  a.kind = kind;
  a.type = type;
  return a;
}
constexpr ArgSpec Any() { return ArgSpec(); }
constexpr ArgSpec Of(JsType type) { return Make(ArgKind::kValue, type); }
constexpr ArgSpec Int() { return Make(ArgKind::kInteger, JsType::kNumber); }
constexpr ArgSpec Index() { return Make(ArgKind::kIndex, JsType::kNumber); }
constexpr ArgSpec Length() { return Make(ArgKind::kLength, JsType::kNumber); }
constexpr ArgSpec Range(int min, int max) {
  ArgSpec a = Make(ArgKind::kRange, JsType::kNumber);
  a.min = min;
  a.max = max;
  return a;
}
constexpr ArgSpec Iterable(JsType element = JsType::kUnknown) {
  return Make(ArgKind::kIterable, element);
}
constexpr ArgSpec Callback(int arity) {
  ArgSpec a = Make(ArgKind::kCallback, JsType::kFunction);
  a.min = arity;
  return a;
}
constexpr ArgSpec Options(std::string_view bag) {
  ArgSpec a = Make(ArgKind::kOptions, JsType::kObject);
  a.name = bag;
  return a;
}
constexpr ArgSpec Enum(std::string_view list) {
  ArgSpec a = Make(ArgKind::kEnum, JsType::kString);
  a.name = list;
  return a;
}
constexpr ArgSpec Instance(JsType type) {
  return Make(ArgKind::kInstance, type);
}
constexpr ArgSpec Opt(ArgSpec a) {
  a.optional = true;
  return a;
}
constexpr ArgSpec Rest(ArgSpec a) {
  a.optional = true;
  a.rest = true;
  return a;
}
}  // namespace arg

// A method or function: name, arity range and result type guess.
struct BuiltinFunction {
  std::string_view name;
  int min_args = 0;
  int max_args = 0;
  JsType result = JsType::kUnknown;
  // Argument type hints (kUnknown = anything), at most 3 used.
  JsType args[3] = {JsType::kUnknown, JsType::kUnknown, JsType::kUnknown};
  // All the parameters, if the model knows them (see ArgSpec). Empty if the
  // model knows only the arity and the hints above.
  std::span<const ArgSpec> params = {};
};

// Methods and properties available on values of a given coarse type.
struct BuiltinType {
  JsType type = JsType::kUnknown;
  std::vector<BuiltinFunction> methods;
  std::vector<std::string_view> properties;
  // The properties that a store changes: data properties, not the getters
  // of the prototype. Empty if the model does not know them.
  std::vector<std::string_view> writable = {};
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
  // The parameters of `new Name(...)` (or of `Name(...)` for a function), if
  // the model knows them. Empty if it knows only the arity.
  std::span<const ArgSpec> params = {};
};

// A named list of the strings that a builtin accepts for one parameter.
struct EnumList {
  std::string_view name;
  std::vector<std::string_view> values;
};

// One key of an options bag, with the kind of its value. A generated bag
// always has the required keys.
struct OptionKey {
  std::string_view key;
  ArgSpec value;
  bool required = false;
};

// A named options bag: the keys that a builtin reads from an object
// argument. A generated bag has at least `min_keys` of these keys.
struct OptionsBag {
  std::string_view name;
  std::vector<OptionKey> keys;
  int min_keys = 0;
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
  // True for a static that the target also reported as a constructor of its
  // own (Intl.Locale, Temporal.PlainDate): a call needs `new`.
  bool constructor = false;
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
  // JSON line via redline('REDLINE_PRINT', ...).
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

  // The named string lists and options bags of typed parameters (ArgSpec
  // kinds kEnum and kOptions), or nullptr for an unknown name.
  const EnumList* Enum(std::string_view name) const;
  const OptionsBag* Options(std::string_view name) const;
  // The signature that the model knows for a discovered callable, or
  // nullptr. `member` empty: the API itself (`new Path(...)` or
  // `Path(...)`). Otherwise a static of `path`, or a prototype method if
  // `prototype` is true. The DiscoveredApi generator uses it instead of the
  // arity that the target reports.
  const BuiltinFunction* DiscoveredSignature(std::string_view path,
                                             std::string_view member,
                                             bool prototype) const;

 private:
  // A signature for a discovered path that the static model does not
  // cover (d8.test, the base64 methods of Uint8Array, ...).
  struct DiscoveredSignatureEntry {
    std::string_view path;
    std::string_view member;
    bool prototype = false;
    BuiltinFunction function;
  };

  Environment();
  std::string_view Intern(std::string_view text);
  // Adds the typed part of the model (environment_typed.cc): parameter
  // lists, enums, options bags and the types that the coarse tables lack.
  void AddTypedModel();
  // Keeps a parameter list alive as long as the environment.
  std::span<const ArgSpec> KeepParams(std::vector<ArgSpec> params);

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
  // Backing storage for the parameter lists (spans) of the typed model.
  std::deque<std::vector<ArgSpec>> owned_params_;
  std::vector<EnumList> enums_;
  std::vector<OptionsBag> options_;
  std::vector<DiscoveredSignatureEntry> discovered_signatures_;
};

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_CORE_ENVIRONMENT_H_
