// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/profiler/strings-storage.h"

#include <memory>

#include "src/utils/allocation.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

bool StringsStorage::StringsMatch(void* key1, void* key2) {
  return strcmp(reinterpret_cast<char*>(key1), reinterpret_cast<char*>(key2)) ==
         0;
}

StringsStorage::StringsStorage() : names_(StringsMatch) {}

StringsStorage::~StringsStorage() {
  for (base::HashMap::Entry* p = names_.Start(); p != nullptr;
       p = names_.Next(p)) {
    DeleteArray(reinterpret_cast<const char*>(p->key));
  }
}

const char* StringsStorage::GetCopy(const char* src) {
  int len = static_cast<int>(strlen(src));
  base::HashMap::Entry* entry = GetEntry(src, len);
  if (entry->value == nullptr) {
    Vector<char> dst = Vector<char>::New(len + 1);
    StrNCpy(dst, src, len);
    dst[len] = '\0';
    entry->key = dst.begin();
  }
  entry->value =
      reinterpret_cast<void*>(reinterpret_cast<size_t>(entry->value) + 1);
  return reinterpret_cast<const char*>(entry->key);
}

const char* StringsStorage::GetFormatted(const char* format, ...) {
  va_list args;
  va_start(args, format);
  const char* result = GetVFormatted(format, args);
  va_end(args);
  return result;
}

const char* StringsStorage::AddOrDisposeString(char* str, int len) {
  base::HashMap::Entry* entry = GetEntry(str, len);
  if (entry->value == nullptr) {
    // New entry added.
    entry->key = str;
  } else {
    DeleteArray(str);
  }
  entry->value =
      reinterpret_cast<void*>(reinterpret_cast<size_t>(entry->value) + 1);
  return reinterpret_cast<const char*>(entry->key);
}

const char* StringsStorage::GetVFormatted(const char* format, va_list args) {
  Vector<char> str = Vector<char>::New(1024);
  int len = VSNPrintF(str, format, args);
  if (len == -1) {
    DeleteArray(str.begin());
    return GetCopy(format);
  }
  return AddOrDisposeString(str.begin(), len);
}

const char* StringsStorage::GetName(Name name) {
  if (name.IsString()) {
    String str = String::cast(name);
    int length = Min(FLAG_heap_snapshot_string_limit, str.length());
    int actual_length = 0;
    std::unique_ptr<char[]> data = str.ToCString(
        DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL, 0, length, &actual_length);
    return AddOrDisposeString(data.release(), actual_length);
  } else if (name.IsSymbol()) {
    return "<symbol>";
  }
  return "";
}

const char* StringsStorage::GetName(int index) {
  return GetFormatted("%d", index);
}

const char* StringsStorage::GetConsName(const char* prefix, Name name) {
  if (name.IsString()) {
    String str = String::cast(name);
    int length = Min(FLAG_heap_snapshot_string_limit, str.length());
    int actual_length = 0;
    std::unique_ptr<char[]> data = str.ToCString(
        DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL, 0, length, &actual_length);

    int cons_length = actual_length + static_cast<int>(strlen(prefix)) + 1;
    char* cons_result = NewArray<char>(cons_length);
    snprintf(cons_result, cons_length, "%s%s", prefix, data.get());

    return AddOrDisposeString(cons_result, cons_length);
  } else if (name.IsSymbol()) {
    return "<symbol>";
  }
  return "";
}

bool StringsStorage::Release(const char* str) {
  int len = static_cast<int>(strlen(str));
  uint32_t hash = StringHasher::HashSequentialString(str, len, kZeroHashSeed);
  base::HashMap::Entry* entry = names_.Lookup(const_cast<char*>(str), hash);
  DCHECK(entry);
  if (!entry) {
    return false;
  }

  DCHECK(entry->value);
  entry->value =
      reinterpret_cast<void*>(reinterpret_cast<size_t>(entry->value) - 1);

  if (entry->value == 0) {
    names_.Remove(const_cast<char*>(str), hash);
    DeleteArray(str);
  }
  return true;
}

size_t StringsStorage::GetStringCountForTesting() const {
  return names_.occupancy();
}

base::HashMap::Entry* StringsStorage::GetEntry(const char* str, int len) {
  uint32_t hash = StringHasher::HashSequentialString(str, len, kZeroHashSeed);
  return names_.LookupOrInsert(const_cast<char*>(str), hash);
}

}  // namespace internal
}  // namespace v8
