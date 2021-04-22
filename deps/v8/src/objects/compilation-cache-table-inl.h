// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_COMPILATION_CACHE_TABLE_INL_H_
#define V8_OBJECTS_COMPILATION_CACHE_TABLE_INL_H_

#include "src/objects/compilation-cache-table.h"
#include "src/objects/name-inl.h"
#include "src/objects/script-inl.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/smi.h"
#include "src/objects/string.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

CompilationCacheTable::CompilationCacheTable(Address ptr)
    : HashTable<CompilationCacheTable, CompilationCacheShape>(ptr) {
  SLOW_DCHECK(IsCompilationCacheTable());
}

NEVER_READ_ONLY_SPACE_IMPL(CompilationCacheTable)
CAST_ACCESSOR(CompilationCacheTable)

uint32_t CompilationCacheShape::RegExpHash(String string, Smi flags) {
  return string.EnsureHash() + flags.value();
}

uint32_t CompilationCacheShape::StringSharedHash(String source,
                                                 SharedFunctionInfo shared,
                                                 LanguageMode language_mode,
                                                 int position) {
  uint32_t hash = source.EnsureHash();
  if (shared.HasSourceCode()) {
    // Instead of using the SharedFunctionInfo pointer in the hash
    // code computation, we use a combination of the hash of the
    // script source code and the start position of the calling scope.
    // We do this to ensure that the cache entries can survive garbage
    // collection.
    Script script(Script::cast(shared.script()));
    hash ^= String::cast(script.source()).EnsureHash();
    STATIC_ASSERT(LanguageModeSize == 2);
    if (is_strict(language_mode)) hash ^= 0x8000;
    hash += position;
  }
  return hash;
}

uint32_t CompilationCacheShape::HashForObject(ReadOnlyRoots roots,
                                              Object object) {
  // Eval: The key field contains the hash as a Number.
  if (object.IsNumber()) return static_cast<uint32_t>(object.Number());

  // Code: The key field contains the SFI key.
  if (object.IsSharedFunctionInfo()) {
    return SharedFunctionInfo::cast(object).Hash();
  }

  // Script: See StringSharedKey::ToHandle for the encoding.
  FixedArray val = FixedArray::cast(object);
  if (val.map() == roots.fixed_cow_array_map()) {
    DCHECK_EQ(4, val.length());
    SharedFunctionInfo shared = SharedFunctionInfo::cast(val.get(0));
    String source = String::cast(val.get(1));
    int language_unchecked = Smi::ToInt(val.get(2));
    DCHECK(is_valid_language_mode(language_unchecked));
    LanguageMode language_mode = static_cast<LanguageMode>(language_unchecked);
    int position = Smi::ToInt(val.get(3));
    return StringSharedHash(source, shared, language_mode, position);
  }

  // RegExp: The key field (and the value field) contains the
  // JSRegExp::data fixed array.
  DCHECK_GE(val.length(), JSRegExp::kMinDataArrayLength);
  return RegExpHash(String::cast(val.get(JSRegExp::kSourceIndex)),
                    Smi::cast(val.get(JSRegExp::kFlagsIndex)));
}

InfoCellPair::InfoCellPair(Isolate* isolate, SharedFunctionInfo shared,
                           FeedbackCell feedback_cell)
    : is_compiled_scope_(!shared.is_null() ? shared.is_compiled_scope(isolate)
                                           : IsCompiledScope()),
      shared_(shared),
      feedback_cell_(feedback_cell) {}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_COMPILATION_CACHE_TABLE_INL_H_
