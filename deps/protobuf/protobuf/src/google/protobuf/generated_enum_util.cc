// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <google/protobuf/generated_enum_util.h>

#include <algorithm>

#include <google/protobuf/generated_message_util.h>

namespace google {
namespace protobuf {
namespace internal {
namespace {

bool EnumCompareByName(const EnumEntry& a, const EnumEntry& b) {
  return StringPiece(a.name) < StringPiece(b.name);
}

// Gets the numeric value of the EnumEntry at the given index, but returns a
// special value for the index -1. This gives a way to use std::lower_bound on a
// sorted array of indices while searching for value that we associate with -1.
int GetValue(const EnumEntry* enums, int i, int target) {
  if (i == -1) {
    return target;
  } else {
    return enums[i].value;
  }
}

}  // namespace

bool LookUpEnumValue(const EnumEntry* enums, size_t size,
                     StringPiece name, int* value) {
  EnumEntry target{name, 0};
  auto it = std::lower_bound(enums, enums + size, target, EnumCompareByName);
  if (it != enums + size && it->name == name) {
    *value = it->value;
    return true;
  }
  return false;
}

int LookUpEnumName(const EnumEntry* enums, const int* sorted_indices,
                   size_t size, int value) {
  auto comparator = [enums, value](int a, int b) {
    return GetValue(enums, a, value) < GetValue(enums, b, value);
  };
  auto it =
      std::lower_bound(sorted_indices, sorted_indices + size, -1, comparator);
  if (it != sorted_indices + size && enums[*it].value == value) {
    return it - sorted_indices;
  }
  return -1;
}

bool InitializeEnumStrings(
    const EnumEntry* enums, const int* sorted_indices, size_t size,
    internal::ExplicitlyConstructed<std::string>* enum_strings) {
  for (int i = 0; i < size; ++i) {
    enum_strings[i].Construct(enums[sorted_indices[i]].name);
    internal::OnShutdownDestroyString(enum_strings[i].get_mutable());
  }
  return true;
}

}  // namespace internal
}  // namespace protobuf
}  // namespace google
