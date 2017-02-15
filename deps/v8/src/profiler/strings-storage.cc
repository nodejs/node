// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/profiler/strings-storage.h"

#include <memory>

#include "src/objects-inl.h"

namespace v8 {
namespace internal {


bool StringsStorage::StringsMatch(void* key1, void* key2) {
  return strcmp(reinterpret_cast<char*>(key1), reinterpret_cast<char*>(key2)) ==
         0;
}


StringsStorage::StringsStorage(Heap* heap)
    : hash_seed_(heap->HashSeed()), names_(StringsMatch) {}


StringsStorage::~StringsStorage() {
  for (base::HashMap::Entry* p = names_.Start(); p != NULL;
       p = names_.Next(p)) {
    DeleteArray(reinterpret_cast<const char*>(p->value));
  }
}


const char* StringsStorage::GetCopy(const char* src) {
  int len = static_cast<int>(strlen(src));
  base::HashMap::Entry* entry = GetEntry(src, len);
  if (entry->value == NULL) {
    Vector<char> dst = Vector<char>::New(len + 1);
    StrNCpy(dst, src, len);
    dst[len] = '\0';
    entry->key = dst.start();
    entry->value = entry->key;
  }
  return reinterpret_cast<const char*>(entry->value);
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
  if (entry->value == NULL) {
    // New entry added.
    entry->key = str;
    entry->value = str;
  } else {
    DeleteArray(str);
  }
  return reinterpret_cast<const char*>(entry->value);
}


const char* StringsStorage::GetVFormatted(const char* format, va_list args) {
  Vector<char> str = Vector<char>::New(1024);
  int len = VSNPrintF(str, format, args);
  if (len == -1) {
    DeleteArray(str.start());
    return GetCopy(format);
  }
  return AddOrDisposeString(str.start(), len);
}


const char* StringsStorage::GetName(Name* name) {
  if (name->IsString()) {
    String* str = String::cast(name);
    int length = Min(kMaxNameSize, str->length());
    int actual_length = 0;
    std::unique_ptr<char[]> data = str->ToCString(
        DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL, 0, length, &actual_length);
    return AddOrDisposeString(data.release(), actual_length);
  } else if (name->IsSymbol()) {
    return "<symbol>";
  }
  return "";
}


const char* StringsStorage::GetName(int index) {
  return GetFormatted("%d", index);
}


const char* StringsStorage::GetFunctionName(Name* name) {
  return GetName(name);
}


const char* StringsStorage::GetFunctionName(const char* name) {
  return GetCopy(name);
}


size_t StringsStorage::GetUsedMemorySize() const {
  size_t size = sizeof(*this);
  size += sizeof(base::HashMap::Entry) * names_.capacity();
  for (base::HashMap::Entry* p = names_.Start(); p != NULL;
       p = names_.Next(p)) {
    size += strlen(reinterpret_cast<const char*>(p->value)) + 1;
  }
  return size;
}

base::HashMap::Entry* StringsStorage::GetEntry(const char* str, int len) {
  uint32_t hash = StringHasher::HashSequentialString(str, len, hash_seed_);
  return names_.LookupOrInsert(const_cast<char*>(str), hash);
}
}  // namespace internal
}  // namespace v8
