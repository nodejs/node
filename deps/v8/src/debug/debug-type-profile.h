// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_DEBUG_TYPE_PROFILE_H_
#define V8_DEBUG_DEBUG_TYPE_PROFILE_H_

#include <vector>

#include "src/debug/debug-interface.h"
#include "src/handles.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

// Forward declaration.
class Isolate;

struct TypeProfileEntry {
  explicit TypeProfileEntry(
      int pos, std::vector<v8::internal::Handle<internal::String>> t)
      : position(pos), types(std::move(t)) {}
  int position;
  std::vector<v8::internal::Handle<internal::String>> types;
};

struct TypeProfileScript {
  explicit TypeProfileScript(Handle<Script> s) : script(s) {}
  Handle<Script> script;
  std::vector<TypeProfileEntry> entries;
};

class TypeProfile : public std::vector<TypeProfileScript> {
 public:
  static std::unique_ptr<TypeProfile> Collect(Isolate* isolate);
  static void SelectMode(Isolate* isolate, debug::TypeProfile::Mode mode);

 private:
  TypeProfile() = default;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_DEBUG_TYPE_PROFILE_H_
