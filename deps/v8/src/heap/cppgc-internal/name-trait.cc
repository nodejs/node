// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/internal/name-trait.h"

#include <stdio.h>

#include <string_view>

#include "src/base/logging.h"

namespace cppgc {

// static
constexpr const char NameProvider::kHiddenName[];

// static
constexpr const char NameProvider::kNoNameDeducible[];

namespace internal {

// static
HeapObjectName NameTraitBase::GetNameFromTypeSignature(const char* signature) {
  // Parsing string of structure:
  //    static HeapObjectName NameTrait<int>::GetNameFor(...) [T = int]
  if (!signature) return {NameProvider::kNoNameDeducible, false};

  const std::string_view raw(signature);
  const auto start_pos = raw.rfind("T = ") + 4;
  DCHECK_NE(std::string_view::npos, start_pos);
  const auto len = raw.length() - start_pos - 1;
  const std::string_view name = raw.substr(start_pos, len);
  char* name_buffer = new char[name.length() + 1];
  memcpy(name_buffer, name.data(), name.length());
  name_buffer[name.length()] = '\0';  // ensure null terminator set
  return {name_buffer, false};
}

}  // namespace internal
}  // namespace cppgc
