// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tunables: the numbers inside generators, mutators and the engine that
// used to be literals ("insert 1-3 fields", "replace by a literal with
// probability 0.15"). Each one has a name that says who owns it
// (`generators.ObjectDecl.max_fields`, `mutators.Input.literal_chance`), a
// default, a range and one line of documentation, and can be set from the
// configuration:
//
//   "tunables": {"generators.ObjectDecl.max_fields": 5}
//   --tunable=generators.ObjectDecl.max_fields=5,mutators.Input.literal_chance=0.3
//
// An unknown name or a value outside the range is an error at startup.
// `redline fuzz --help-tunables` lists them all; the `start` event records
// the ones that differ from their default.
//
// Declare one at namespace scope next to its use:
//
//   REDLINE_TUNABLE(kObjectDeclMaxExtraFields,
//   "generators.ObjectDecl.max_extra_fields", 3, 0, 32,
//                   "fields an object literal gets beyond the first
//                   (geometric)");
//   ...
//   int fields = 1 + r.SmallCount(kObjectDeclMaxExtraFields.AsInt());
//
// Convention: a tunable's default is exactly the number it replaced, so the
// code reads as before; "max_extra_*" names the n in `1 + SmallCount(n)`.
//
// Reading a tunable is a plain load; values are set once, before the
// workers start.

#ifndef V8_EXPERIMENTAL_REDLINE_CORE_TUNABLE_H_
#define V8_EXPERIMENTAL_REDLINE_CORE_TUNABLE_H_

#include <string>
#include <string_view>
#include <vector>

namespace redline {

class Tunable {
 public:
  Tunable(std::string_view name, double default_value, double min, double max,
          std::string_view doc);
  Tunable(const Tunable&) = delete;
  Tunable& operator=(const Tunable&) = delete;
  ~Tunable() = default;

  double operator()() const { return value_; }
  int AsInt() const { return static_cast<int>(value_); }

  std::string_view name() const { return name_; }
  double default_value() const { return default_; }
  double min() const { return min_; }
  double max() const { return max_; }
  std::string_view doc() const { return doc_; }
  bool is_default() const { return value_ == default_; }

 private:
  friend class TunableRegistry;
  std::string_view name_;
  double value_;
  double default_;
  double min_;
  double max_;
  std::string_view doc_;
};

// All tunables, by name (registered by their constructors).
class TunableRegistry {
 public:
  static TunableRegistry& Instance();

  // Sets a tunable; false with *error for an unknown name or a value
  // outside its range.
  bool Set(std::string_view name, double value, std::string* error);
  // Parses "name=value[,name=value...]" (the --tunable flag).
  bool SetFromList(std::string_view list, std::string* error);
  // Back to the defaults (tests).
  void Reset();

  const Tunable* Find(std::string_view name) const;
  std::vector<const Tunable*> All() const;  // sorted by name
  // One line per tunable: name = value (default, range)  doc.
  std::string Help() const;
  // {"name": value} of the tunables that differ from their default.
  std::string NonDefaultsJson() const;
  // {"name": value} of all tunables (the full configuration dump).
  std::string AllJson() const;
  size_t size() const { return tunables_.size(); }

 private:
  friend class Tunable;
  void Register(Tunable* tunable);

  std::vector<Tunable*> tunables_;
};

// Declares a tunable at namespace scope.
#define REDLINE_TUNABLE(identifier, name, default_value, min, max, doc) \
  static ::redline::Tunable identifier(name, default_value, min, max, doc)

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_CORE_TUNABLE_H_
