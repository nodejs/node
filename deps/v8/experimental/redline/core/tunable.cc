// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/core/tunable.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "experimental/redline/base/json.h"
#include "experimental/redline/base/log.h"

namespace redline {

Tunable::Tunable(std::string_view name, double default_value, double min,
                 double max, std::string_view doc)
    : name_(name),
      value_(default_value),
      default_(default_value),
      min_(min),
      max_(max),
      doc_(doc) {
  TunableRegistry::Instance().Register(this);
}

// static
TunableRegistry& TunableRegistry::Instance() {
  static TunableRegistry* registry = new TunableRegistry();
  return *registry;
}

void TunableRegistry::Register(Tunable* tunable) {
  // Two declarations with the same name would silently diverge; refuse.
  if (Find(tunable->name()) != nullptr) {
    fprintf(stderr, "redline: tunable '%.*s' declared twice\n",
            static_cast<int>(tunable->name().size()), tunable->name().data());
    abort();
  }
  tunables_.push_back(tunable);
}

const Tunable* TunableRegistry::Find(std::string_view name) const {
  for (const Tunable* t : tunables_) {
    if (t->name() == name) {
      return t;
    }
  }
  return nullptr;
}

bool TunableRegistry::Set(std::string_view name, double value,
                          std::string* error) {
  for (Tunable* t : tunables_) {
    if (t->name() != name) {
      continue;
    }
    if (!std::isfinite(value) || value < t->min() || value > t->max()) {
      *error = StringPrintf("tunable '%.*s': %g is outside its range [%g, %g]",
                            static_cast<int>(name.size()), name.data(), value,
                            t->min(), t->max());
      return false;
    }
    t->value_ = value;
    return true;
  }
  *error = StringPrintf(
      "unknown tunable '%.*s' (redline fuzz --help-tunables lists them)",
      static_cast<int>(name.size()), name.data());
  return false;
}

bool TunableRegistry::SetFromList(std::string_view list, std::string* error) {
  size_t start = 0;
  while (start < list.size()) {
    size_t comma = list.find(',', start);
    if (comma == std::string_view::npos) {
      comma = list.size();
    }
    std::string_view item = list.substr(start, comma - start);
    start = comma + 1;
    if (item.empty()) {
      continue;
    }
    size_t eq = item.find('=');
    if (eq == std::string_view::npos) {
      *error = "--tunable expects NAME=VALUE, got '" + std::string(item) + "'";
      return false;
    }
    char* end = nullptr;
    std::string text(item.substr(eq + 1));
    double value = strtod(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0') {
      *error = "--tunable: '" + text + "' is not a number";
      return false;
    }
    if (!Set(item.substr(0, eq), value, error)) {
      return false;
    }
  }
  return true;
}

void TunableRegistry::Reset() {
  for (Tunable* t : tunables_) {
    t->value_ = t->default_;
  }
}

std::vector<const Tunable*> TunableRegistry::All() const {
  std::vector<const Tunable*> all(tunables_.begin(), tunables_.end());
  std::sort(all.begin(), all.end(), [](const Tunable* a, const Tunable* b) {
    return a->name() < b->name();
  });
  return all;
}

std::string TunableRegistry::Help() const {
  std::string out;
  for (const Tunable* t : All()) {
    out += StringPrintf("  %-52.*s = %-8g (default %g, range %g..%g)  %.*s\n",
                        static_cast<int>(t->name().size()), t->name().data(),
                        (*t)(), t->default_value(), t->min(), t->max(),
                        static_cast<int>(t->doc().size()), t->doc().data());
  }
  return out;
}

std::string TunableRegistry::NonDefaultsJson() const {
  JsonObject o;
  for (const Tunable* t : All()) {
    if (!t->is_default()) {
      o.Add(t->name(), (*t)());
    }
  }
  return o.ToString();
}

std::string TunableRegistry::AllJson() const {
  JsonObject o;
  for (const Tunable* t : All()) {
    o.Add(t->name(), (*t)());
  }
  return o.ToString();
}

}  // namespace redline
