// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/compilation-cache-table.h"

#include "src/common/assert-scope.h"
#include "src/objects/compilation-cache-table-inl.h"

namespace v8 {
namespace internal {

namespace {

const int kLiteralEntryLength = 2;
const int kLiteralInitialLength = 2;
const int kLiteralContextOffset = 0;
const int kLiteralLiteralsOffset = 1;

// The initial placeholder insertion of the eval cache survives this many GCs.
const int kHashGenerations = 10;

int SearchLiteralsMapEntry(CompilationCacheTable cache, int cache_entry,
                           Context native_context) {
  DisallowGarbageCollection no_gc;
  DCHECK(native_context.IsNativeContext());
  Object obj = cache.get(cache_entry);

  // Check that there's no confusion between FixedArray and WeakFixedArray (the
  // object used to be a FixedArray here).
  DCHECK(!obj.IsFixedArray());
  if (obj.IsWeakFixedArray()) {
    WeakFixedArray literals_map = WeakFixedArray::cast(obj);
    int length = literals_map.length();
    for (int i = 0; i < length; i += kLiteralEntryLength) {
      DCHECK(literals_map.Get(i + kLiteralContextOffset)->IsWeakOrCleared());
      if (literals_map.Get(i + kLiteralContextOffset) ==
          HeapObjectReference::Weak(native_context)) {
        return i;
      }
    }
  }
  return -1;
}

void AddToFeedbackCellsMap(Handle<CompilationCacheTable> cache, int cache_entry,
                           Handle<Context> native_context,
                           Handle<FeedbackCell> feedback_cell) {
  Isolate* isolate = native_context->GetIsolate();
  DCHECK(native_context->IsNativeContext());
  STATIC_ASSERT(kLiteralEntryLength == 2);
  Handle<WeakFixedArray> new_literals_map;
  int entry;

  Object obj = cache->get(cache_entry);

  // Check that there's no confusion between FixedArray and WeakFixedArray (the
  // object used to be a FixedArray here).
  DCHECK(!obj.IsFixedArray());
  if (!obj.IsWeakFixedArray() || WeakFixedArray::cast(obj).length() == 0) {
    new_literals_map = isolate->factory()->NewWeakFixedArray(
        kLiteralInitialLength, AllocationType::kOld);
    entry = 0;
  } else {
    Handle<WeakFixedArray> old_literals_map(WeakFixedArray::cast(obj), isolate);
    entry = SearchLiteralsMapEntry(*cache, cache_entry, *native_context);
    if (entry >= 0) {
      // Just set the code of the entry.
      old_literals_map->Set(entry + kLiteralLiteralsOffset,
                            HeapObjectReference::Weak(*feedback_cell));
      return;
    }

    // Can we reuse an entry?
    DCHECK_LT(entry, 0);
    int length = old_literals_map->length();
    for (int i = 0; i < length; i += kLiteralEntryLength) {
      if (old_literals_map->Get(i + kLiteralContextOffset)->IsCleared()) {
        new_literals_map = old_literals_map;
        entry = i;
        break;
      }
    }

    if (entry < 0) {
      // Copy old optimized code map and append one new entry.
      new_literals_map = isolate->factory()->CopyWeakFixedArrayAndGrow(
          old_literals_map, kLiteralEntryLength);
      entry = old_literals_map->length();
    }
  }

  new_literals_map->Set(entry + kLiteralContextOffset,
                        HeapObjectReference::Weak(*native_context));
  new_literals_map->Set(entry + kLiteralLiteralsOffset,
                        HeapObjectReference::Weak(*feedback_cell));

#ifdef DEBUG
  for (int i = 0; i < new_literals_map->length(); i += kLiteralEntryLength) {
    MaybeObject object = new_literals_map->Get(i + kLiteralContextOffset);
    DCHECK(object->IsCleared() ||
           object->GetHeapObjectAssumeWeak().IsNativeContext());
    object = new_literals_map->Get(i + kLiteralLiteralsOffset);
    DCHECK(object->IsCleared() ||
           object->GetHeapObjectAssumeWeak().IsFeedbackCell());
  }
#endif

  Object old_literals_map = cache->get(cache_entry);
  if (old_literals_map != *new_literals_map) {
    cache->set(cache_entry, *new_literals_map);
  }
}

FeedbackCell SearchLiteralsMap(CompilationCacheTable cache, int cache_entry,
                               Context native_context) {
  FeedbackCell result;
  int entry = SearchLiteralsMapEntry(cache, cache_entry, native_context);
  if (entry >= 0) {
    WeakFixedArray literals_map = WeakFixedArray::cast(cache.get(cache_entry));
    DCHECK_LE(entry + kLiteralEntryLength, literals_map.length());
    MaybeObject object = literals_map.Get(entry + kLiteralLiteralsOffset);

    if (!object->IsCleared()) {
      result = FeedbackCell::cast(object->GetHeapObjectAssumeWeak());
    }
  }
  DCHECK(result.is_null() || result.IsFeedbackCell());
  return result;
}

// StringSharedKeys are used as keys in the eval cache.
class StringSharedKey : public HashTableKey {
 public:
  // This tuple unambiguously identifies calls to eval() or
  // CreateDynamicFunction() (such as through the Function() constructor).
  // * source is the string passed into eval(). For dynamic functions, this is
  //   the effective source for the function, some of which is implicitly
  //   generated.
  // * shared is the shared function info for the function containing the call
  //   to eval(). for dynamic functions, shared is the native context closure.
  // * When positive, position is the position in the source where eval is
  //   called. When negative, position is the negation of the position in the
  //   dynamic function's effective source where the ')' ends the parameters.
  StringSharedKey(Handle<String> source, Handle<SharedFunctionInfo> shared,
                  LanguageMode language_mode, int position)
      : HashTableKey(CompilationCacheShape::StringSharedHash(
            *source, *shared, language_mode, position)),
        source_(source),
        shared_(shared),
        language_mode_(language_mode),
        position_(position) {}

  bool IsMatch(Object other) override {
    DisallowGarbageCollection no_gc;
    if (!other.IsFixedArray()) {
      DCHECK(other.IsNumber());
      uint32_t other_hash = static_cast<uint32_t>(other.Number());
      return Hash() == other_hash;
    }
    FixedArray other_array = FixedArray::cast(other);
    SharedFunctionInfo shared = SharedFunctionInfo::cast(other_array.get(0));
    if (shared != *shared_) return false;
    int language_unchecked = Smi::ToInt(other_array.get(2));
    DCHECK(is_valid_language_mode(language_unchecked));
    LanguageMode language_mode = static_cast<LanguageMode>(language_unchecked);
    if (language_mode != language_mode_) return false;
    int position = Smi::ToInt(other_array.get(3));
    if (position != position_) return false;
    String source = String::cast(other_array.get(1));
    return source.Equals(*source_);
  }

  Handle<Object> AsHandle(Isolate* isolate) {
    Handle<FixedArray> array = isolate->factory()->NewFixedArray(4);
    array->set(0, *shared_);
    array->set(1, *source_);
    array->set(2, Smi::FromEnum(language_mode_));
    array->set(3, Smi::FromInt(position_));
    array->set_map(ReadOnlyRoots(isolate).fixed_cow_array_map());
    return array;
  }

 private:
  Handle<String> source_;
  Handle<SharedFunctionInfo> shared_;
  LanguageMode language_mode_;
  int position_;
};

// RegExpKey carries the source and flags of a regular expression as key.
class RegExpKey : public HashTableKey {
 public:
  RegExpKey(Handle<String> string, JSRegExp::Flags flags)
      : HashTableKey(
            CompilationCacheShape::RegExpHash(*string, Smi::FromInt(flags))),
        string_(string),
        flags_(Smi::FromInt(flags)) {}

  // Rather than storing the key in the hash table, a pointer to the
  // stored value is stored where the key should be.  IsMatch then
  // compares the search key to the found object, rather than comparing
  // a key to a key.
  bool IsMatch(Object obj) override {
    FixedArray val = FixedArray::cast(obj);
    return string_->Equals(String::cast(val.get(JSRegExp::kSourceIndex))) &&
           (flags_ == val.get(JSRegExp::kFlagsIndex));
  }

  Handle<String> string_;
  Smi flags_;
};

// CodeKey carries the SharedFunctionInfo key associated with a Code
// object value.
class CodeKey : public HashTableKey {
 public:
  explicit CodeKey(Handle<SharedFunctionInfo> key)
      : HashTableKey(key->Hash()), key_(key) {}

  bool IsMatch(Object string) override { return *key_ == string; }

  Handle<SharedFunctionInfo> key_;
};

}  // namespace

MaybeHandle<SharedFunctionInfo> CompilationCacheTable::LookupScript(
    Handle<CompilationCacheTable> table, Handle<String> src,
    Handle<Context> native_context, LanguageMode language_mode) {
  // We use the empty function SFI as part of the key. Although the
  // empty_function is native context dependent, the SFI is de-duped on
  // snapshot builds by the StartupObjectCache, and so this does not prevent
  // reuse of scripts in the compilation cache across native contexts.
  Handle<SharedFunctionInfo> shared(native_context->empty_function().shared(),
                                    native_context->GetIsolate());
  Isolate* isolate = native_context->GetIsolate();
  src = String::Flatten(isolate, src);
  StringSharedKey key(src, shared, language_mode, kNoSourcePosition);
  InternalIndex entry = table->FindEntry(isolate, &key);
  if (entry.is_not_found()) return MaybeHandle<SharedFunctionInfo>();
  int index = EntryToIndex(entry);
  if (!table->get(index).IsFixedArray()) {
    return MaybeHandle<SharedFunctionInfo>();
  }
  Object obj = table->get(index + 1);
  if (obj.IsSharedFunctionInfo()) {
    return handle(SharedFunctionInfo::cast(obj), native_context->GetIsolate());
  }
  return MaybeHandle<SharedFunctionInfo>();
}

InfoCellPair CompilationCacheTable::LookupEval(
    Handle<CompilationCacheTable> table, Handle<String> src,
    Handle<SharedFunctionInfo> outer_info, Handle<Context> native_context,
    LanguageMode language_mode, int position) {
  InfoCellPair empty_result;
  Isolate* isolate = native_context->GetIsolate();
  src = String::Flatten(isolate, src);

  StringSharedKey key(src, outer_info, language_mode, position);
  InternalIndex entry = table->FindEntry(isolate, &key);
  if (entry.is_not_found()) return empty_result;

  int index = EntryToIndex(entry);
  if (!table->get(index).IsFixedArray()) return empty_result;
  Object obj = table->get(index + 1);
  if (!obj.IsSharedFunctionInfo()) return empty_result;

  STATIC_ASSERT(CompilationCacheShape::kEntrySize == 3);
  FeedbackCell feedback_cell =
      SearchLiteralsMap(*table, index + 2, *native_context);
  return InfoCellPair(isolate, SharedFunctionInfo::cast(obj), feedback_cell);
}

Handle<Object> CompilationCacheTable::LookupRegExp(Handle<String> src,
                                                   JSRegExp::Flags flags) {
  Isolate* isolate = GetIsolate();
  DisallowGarbageCollection no_gc;
  RegExpKey key(src, flags);
  InternalIndex entry = FindEntry(isolate, &key);
  if (entry.is_not_found()) return isolate->factory()->undefined_value();
  return Handle<Object>(get(EntryToIndex(entry) + 1), isolate);
}

MaybeHandle<Code> CompilationCacheTable::LookupCode(
    Handle<SharedFunctionInfo> key) {
  Isolate* isolate = GetIsolate();
  DisallowGarbageCollection no_gc;
  CodeKey k(key);
  InternalIndex entry = FindEntry(isolate, &k);
  if (entry.is_not_found()) return {};
  return Handle<Code>(Code::cast(get(EntryToIndex(entry) + 1)), isolate);
}

Handle<CompilationCacheTable> CompilationCacheTable::PutScript(
    Handle<CompilationCacheTable> cache, Handle<String> src,
    Handle<Context> native_context, LanguageMode language_mode,
    Handle<SharedFunctionInfo> value) {
  Isolate* isolate = native_context->GetIsolate();
  // We use the empty function SFI as part of the key. Although the
  // empty_function is native context dependent, the SFI is de-duped on
  // snapshot builds by the StartupObjectCache, and so this does not prevent
  // reuse of scripts in the compilation cache across native contexts.
  Handle<SharedFunctionInfo> shared(native_context->empty_function().shared(),
                                    isolate);
  src = String::Flatten(isolate, src);
  StringSharedKey key(src, shared, language_mode, kNoSourcePosition);
  Handle<Object> k = key.AsHandle(isolate);
  cache = EnsureCapacity(isolate, cache);
  InternalIndex entry = cache->FindInsertionEntry(isolate, key.Hash());
  cache->set(EntryToIndex(entry), *k);
  cache->set(EntryToIndex(entry) + 1, *value);
  cache->ElementAdded();
  return cache;
}

Handle<CompilationCacheTable> CompilationCacheTable::PutEval(
    Handle<CompilationCacheTable> cache, Handle<String> src,
    Handle<SharedFunctionInfo> outer_info, Handle<SharedFunctionInfo> value,
    Handle<Context> native_context, Handle<FeedbackCell> feedback_cell,
    int position) {
  Isolate* isolate = native_context->GetIsolate();
  src = String::Flatten(isolate, src);
  StringSharedKey key(src, outer_info, value->language_mode(), position);

  // This block handles 'real' insertions, i.e. the initial dummy insert
  // (below) has already happened earlier.
  {
    Handle<Object> k = key.AsHandle(isolate);
    InternalIndex entry = cache->FindEntry(isolate, &key);
    if (entry.is_found()) {
      cache->set(EntryToIndex(entry), *k);
      cache->set(EntryToIndex(entry) + 1, *value);
      // AddToFeedbackCellsMap may allocate a new sub-array to live in the
      // entry, but it won't change the cache array. Therefore EntryToIndex
      // and entry remains correct.
      STATIC_ASSERT(CompilationCacheShape::kEntrySize == 3);
      AddToFeedbackCellsMap(cache, EntryToIndex(entry) + 2, native_context,
                            feedback_cell);
      // Add hash again even on cache hit to avoid unnecessary cache delay in
      // case of hash collisions.
    }
  }

  // Create a dummy entry to mark that this key has already been inserted once.
  cache = EnsureCapacity(isolate, cache);
  InternalIndex entry = cache->FindInsertionEntry(isolate, key.Hash());
  Handle<Object> k =
      isolate->factory()->NewNumber(static_cast<double>(key.Hash()));
  cache->set(EntryToIndex(entry), *k);
  cache->set(EntryToIndex(entry) + 1, Smi::FromInt(kHashGenerations));
  cache->ElementAdded();
  return cache;
}

Handle<CompilationCacheTable> CompilationCacheTable::PutRegExp(
    Isolate* isolate, Handle<CompilationCacheTable> cache, Handle<String> src,
    JSRegExp::Flags flags, Handle<FixedArray> value) {
  RegExpKey key(src, flags);
  cache = EnsureCapacity(isolate, cache);
  InternalIndex entry = cache->FindInsertionEntry(isolate, key.Hash());
  // We store the value in the key slot, and compare the search key
  // to the stored value with a custom IsMatch function during lookups.
  cache->set(EntryToIndex(entry), *value);
  cache->set(EntryToIndex(entry) + 1, *value);
  cache->ElementAdded();
  return cache;
}

Handle<CompilationCacheTable> CompilationCacheTable::PutCode(
    Isolate* isolate, Handle<CompilationCacheTable> cache,
    Handle<SharedFunctionInfo> key, Handle<Code> value) {
  CodeKey k(key);

  {
    InternalIndex entry = cache->FindEntry(isolate, &k);
    if (entry.is_found()) {
      // Update.
      cache->set(EntryToIndex(entry), *key);
      cache->set(EntryToIndex(entry) + 1, *value);
      return cache;
    }
  }

  // Insert.
  cache = EnsureCapacity(isolate, cache);
  InternalIndex entry = cache->FindInsertionEntry(isolate, k.Hash());
  cache->set(EntryToIndex(entry), *key);
  cache->set(EntryToIndex(entry) + 1, *value);
  cache->ElementAdded();
  return cache;
}

void CompilationCacheTable::Age(Isolate* isolate) {
  DisallowGarbageCollection no_gc;
  for (InternalIndex entry : IterateEntries()) {
    const int entry_index = EntryToIndex(entry);
    const int value_index = entry_index + 1;

    Object key = get(entry_index);
    if (key.IsNumber()) {
      // The ageing mechanism for the initial dummy entry in the eval cache.
      // The 'key' is the hash represented as a Number. The 'value' is a smi
      // counting down from kHashGenerations. On reaching zero, the entry is
      // cleared.
      // Note: The following static assert only establishes an explicit
      // connection between initialization- and use-sites of the smi value
      // field.
      STATIC_ASSERT(kHashGenerations);
      const int new_count = Smi::ToInt(get(value_index)) - 1;
      if (new_count == 0) {
        RemoveEntry(entry_index);
      } else {
        DCHECK_GT(new_count, 0);
        NoWriteBarrierSet(*this, value_index, Smi::FromInt(new_count));
      }
    } else if (key.IsFixedArray()) {
      // The ageing mechanism for script and eval caches.
      SharedFunctionInfo info = SharedFunctionInfo::cast(get(value_index));
      if (info.IsInterpreted() && info.GetBytecodeArray(isolate).IsOld()) {
        RemoveEntry(entry_index);
      }
    }
  }
}

void CompilationCacheTable::Remove(Object value) {
  DisallowGarbageCollection no_gc;
  for (InternalIndex entry : IterateEntries()) {
    int entry_index = EntryToIndex(entry);
    int value_index = entry_index + 1;
    if (get(value_index) == value) {
      RemoveEntry(entry_index);
    }
  }
}

void CompilationCacheTable::RemoveEntry(int entry_index) {
  Object the_hole_value = GetReadOnlyRoots().the_hole_value();
  for (int i = 0; i < kEntrySize; i++) {
    NoWriteBarrierSet(*this, entry_index + i, the_hole_value);
  }
  ElementRemoved();
}

}  // namespace internal
}  // namespace v8
