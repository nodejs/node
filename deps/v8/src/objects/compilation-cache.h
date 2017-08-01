// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_COMPILATION_CACHE_H_
#define V8_OBJECTS_COMPILATION_CACHE_H_

#include "src/objects/hash-table.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class CompilationCacheShape : public BaseShape<HashTableKey*> {
 public:
  static inline bool IsMatch(HashTableKey* key, Object* value) {
    return key->IsMatch(value);
  }

  static inline uint32_t Hash(HashTableKey* key) { return key->Hash(); }

  static inline uint32_t HashForObject(HashTableKey* key, Object* object) {
    return key->HashForObject(object);
  }

  static inline Handle<Object> AsHandle(Isolate* isolate, HashTableKey* key);

  static const int kPrefixSize = 0;
  static const int kEntrySize = 3;
};

class InfoVectorPair {
 public:
  InfoVectorPair() : shared_(nullptr), vector_cell_(nullptr) {}
  InfoVectorPair(SharedFunctionInfo* shared, Cell* vector_cell)
      : shared_(shared), vector_cell_(vector_cell) {}

  SharedFunctionInfo* shared() const { return shared_; }
  Cell* vector() const { return vector_cell_; }

  bool has_shared() const { return shared_ != nullptr; }
  bool has_vector() const { return vector_cell_ != nullptr; }

 private:
  SharedFunctionInfo* shared_;
  Cell* vector_cell_;
};

// This cache is used in two different variants. For regexp caching, it simply
// maps identifying info of the regexp to the cached regexp object. Scripts and
// eval code only gets cached after a second probe for the code object. To do
// so, on first "put" only a hash identifying the source is entered into the
// cache, mapping it to a lifetime count of the hash. On each call to Age all
// such lifetimes get reduced, and removed once they reach zero. If a second put
// is called while such a hash is live in the cache, the hash gets replaced by
// an actual cache entry. Age also removes stale live entries from the cache.
// Such entries are identified by SharedFunctionInfos pointing to either the
// recompilation stub, or to "old" code. This avoids memory leaks due to
// premature caching of scripts and eval strings that are never needed later.
class CompilationCacheTable
    : public HashTable<CompilationCacheTable, CompilationCacheShape,
                       HashTableKey*> {
 public:
  // Find cached value for a string key, otherwise return null.
  Handle<Object> Lookup(Handle<String> src, Handle<Context> context,
                        LanguageMode language_mode);
  InfoVectorPair LookupScript(Handle<String> src, Handle<Context> context,
                              LanguageMode language_mode);
  InfoVectorPair LookupEval(Handle<String> src,
                            Handle<SharedFunctionInfo> shared,
                            Handle<Context> native_context,
                            LanguageMode language_mode, int position);
  Handle<Object> LookupRegExp(Handle<String> source, JSRegExp::Flags flags);
  static Handle<CompilationCacheTable> Put(Handle<CompilationCacheTable> cache,
                                           Handle<String> src,
                                           Handle<Context> context,
                                           LanguageMode language_mode,
                                           Handle<Object> value);
  static Handle<CompilationCacheTable> PutScript(
      Handle<CompilationCacheTable> cache, Handle<String> src,
      Handle<Context> context, LanguageMode language_mode,
      Handle<SharedFunctionInfo> value, Handle<Cell> literals);
  static Handle<CompilationCacheTable> PutEval(
      Handle<CompilationCacheTable> cache, Handle<String> src,
      Handle<SharedFunctionInfo> outer_info, Handle<SharedFunctionInfo> value,
      Handle<Context> native_context, Handle<Cell> literals, int position);
  static Handle<CompilationCacheTable> PutRegExp(
      Handle<CompilationCacheTable> cache, Handle<String> src,
      JSRegExp::Flags flags, Handle<FixedArray> value);
  void Remove(Object* value);
  void Age();
  static const int kHashGenerations = 10;

  DECLARE_CAST(CompilationCacheTable)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(CompilationCacheTable);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_COMPILATION_CACHE_H_
