// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROFILER_STRINGS_STORAGE_H_
#define V8_PROFILER_STRINGS_STORAGE_H_

#include <stdarg.h>

#include "src/base/compiler-specific.h"
#include "src/base/hashmap.h"
#include "src/common/globals.h"

namespace v8 {
namespace internal {

class Name;

// Provides a storage of strings allocated in C++ heap, to hold them
// forever, even if they disappear from JS heap or external storage.
class V8_EXPORT_PRIVATE StringsStorage {
 public:
  StringsStorage();
  ~StringsStorage();

  // Copies the given c-string and stores it, returning the stored copy, or just
  // returns the existing string in storage if it already exists.
  const char* GetCopy(const char* src);
  // Returns a formatted string, de-duplicated via the storage.
  PRINTF_FORMAT(2, 3) const char* GetFormatted(const char* format, ...);
  // Returns a stored string resulting from name, or "<symbol>" for a symbol.
  const char* GetName(Name name);
  // Returns the string representation of the int from the store.
  const char* GetName(int index);
  // Appends string resulting from name to prefix, then returns the stored
  // result.
  const char* GetConsName(const char* prefix, Name name);

 private:
  static bool StringsMatch(void* key1, void* key2);
  // Adds the string to storage and returns it, or if a matching string exists
  // in the storage, deletes str and returns the matching string instead.
  const char* AddOrDisposeString(char* str, int len);
  base::CustomMatcherHashMap::Entry* GetEntry(const char* str, int len);
  PRINTF_FORMAT(2, 0)
  const char* GetVFormatted(const char* format, va_list args);

  base::CustomMatcherHashMap names_;

  DISALLOW_COPY_AND_ASSIGN(StringsStorage);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PROFILER_STRINGS_STORAGE_H_
