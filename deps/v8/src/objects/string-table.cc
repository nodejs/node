// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/string-table.h"

#include "src/base/macros.h"
#include "src/common/assert-scope.h"
#include "src/common/globals.h"
#include "src/common/ptr-compr-inl.h"
#include "src/execution/isolate-utils-inl.h"
#include "src/objects/internal-index.h"
#include "src/objects/object-list-macros.h"
#include "src/objects/slots-inl.h"
#include "src/objects/slots.h"
#include "src/objects/string-inl.h"
#include "src/objects/string-table-inl.h"
#include "src/snapshot/deserializer.h"
#include "src/utils/allocation.h"
#include "src/utils/ostreams.h"

namespace v8 {
namespace internal {

namespace {

static constexpr int kStringTableMaxEmptyFactor = 4;
static constexpr int kStringTableMinCapacity = 2048;

bool StringTableHasSufficientCapacityToAdd(int capacity, int number_of_elements,
                                           int number_of_deleted_elements,
                                           int number_of_additional_elements) {
  int nof = number_of_elements + number_of_additional_elements;
  // Return true if:
  //   50% is still free after adding number_of_additional_elements elements and
  //   at most 50% of the free elements are deleted elements.
  if ((nof < capacity) &&
      ((number_of_deleted_elements <= (capacity - nof) / 2))) {
    int needed_free = nof / 2;
    if (nof + needed_free <= capacity) return true;
  }
  return false;
}

int ComputeStringTableCapacity(int at_least_space_for) {
  // Add 50% slack to make slot collisions sufficiently unlikely.
  // See matching computation in StringTableHasSufficientCapacityToAdd().
  int raw_capacity = at_least_space_for + (at_least_space_for >> 1);
  int capacity = base::bits::RoundUpToPowerOfTwo32(raw_capacity);
  return Max(capacity, kStringTableMinCapacity);
}

int ComputeStringTableCapacityWithShrink(int current_capacity,
                                         int at_least_room_for) {
  // Only shrink if the table is very empty to avoid performance penalty.
  DCHECK_GE(current_capacity, kStringTableMinCapacity);
  if (at_least_room_for > (current_capacity / kStringTableMaxEmptyFactor))
    return current_capacity;

  // Recalculate the smaller capacity actually needed.
  int new_capacity = ComputeStringTableCapacity(at_least_room_for);
  DCHECK_GE(new_capacity, at_least_room_for);
  // Don't go lower than room for {kStringTableMinCapacity} elements.
  if (new_capacity < kStringTableMinCapacity) return current_capacity;
  return new_capacity;
}

template <typename StringTableKey>
bool KeyIsMatch(StringTableKey* key, String string) {
  if (string.hash_field() != key->hash_field()) return false;
  if (string.length() != key->length()) return false;
  return key->IsMatch(string);
}

}  // namespace

// Data holds the actual data of the string table, including capacity and number
// of elements.
//
// It is a variable sized structure, with a "header" followed directly in memory
// by the elements themselves. These are accessed as offsets from the elements_
// field, which itself provides storage for the first element.
//
// The elements themselves are stored as an open-addressed hash table, with
// quadratic probing and Smi 0 and Smi 1 as the empty and deleted sentinels,
// respectively.
class StringTable::Data {
 public:
  static std::unique_ptr<Data> New(int capacity);
  static void Resize(const Isolate* isolate, std::unique_ptr<Data>& data,
                     int capacity);

  OffHeapObjectSlot slot(InternalIndex index) const {
    return OffHeapObjectSlot(&elements_[index.as_uint32()]);
  }

  Object Get(const Isolate* isolate, InternalIndex index) const {
    return slot(index).Relaxed_Load(isolate);
  }

  void Set(InternalIndex index, String entry) {
    slot(index).Relaxed_Store(entry);
  }

  void ElementAdded() {
    DCHECK_LT(number_of_elements_ + 1, capacity());
    DCHECK(StringTableHasSufficientCapacityToAdd(
        capacity(), number_of_elements(), number_of_deleted_elements(), 1));

    number_of_elements_++;
  }
  void DeletedElementOverwritten() {
    DCHECK_LT(number_of_elements_ + 1, capacity());
    DCHECK(StringTableHasSufficientCapacityToAdd(
        capacity(), number_of_elements(), number_of_deleted_elements() - 1, 1));

    number_of_elements_++;
    number_of_deleted_elements_--;
  }
  void ElementsRemoved(int count) {
    DCHECK_LE(count, number_of_elements_);
    number_of_elements_ -= count;
    number_of_deleted_elements_ += count;
  }

  void* operator new(size_t size, int capacity);
  void* operator new(size_t size) = delete;
  void operator delete(void* description);

  int capacity() const { return capacity_; }
  int number_of_elements() const { return number_of_elements_; }
  int number_of_deleted_elements() const { return number_of_deleted_elements_; }

  template <typename StringTableKey>
  InternalIndex FindEntry(const Isolate* isolate, StringTableKey* key,
                          uint32_t hash) const;

  InternalIndex FindInsertionEntry(const Isolate* isolate, uint32_t hash) const;

  template <typename StringTableKey>
  InternalIndex FindEntryOrInsertionEntry(const Isolate* isolate,
                                          StringTableKey* key,
                                          uint32_t hash) const;

  // Helper method for StringTable::TryStringToIndexOrLookupExisting.
  template <typename Char>
  static Address TryStringToIndexOrLookupExisting(Isolate* isolate,
                                                  String string, String source,
                                                  size_t start);

  void IterateElements(RootVisitor* visitor);

  Data* PreviousData() { return previous_data_.get(); }
  void DropPreviousData() { previous_data_.reset(); }

  void Print(const Isolate* isolate) const;
  size_t GetCurrentMemoryUsage() const;

 private:
  explicit Data(int capacity);

  // Returns probe entry.
  inline static InternalIndex FirstProbe(uint32_t hash, uint32_t size) {
    return InternalIndex(hash & (size - 1));
  }

  inline static InternalIndex NextProbe(InternalIndex last, uint32_t number,
                                        uint32_t size) {
    return InternalIndex((last.as_uint32() + number) & (size - 1));
  }

 private:
  std::unique_ptr<Data> previous_data_;
  int number_of_elements_;
  int number_of_deleted_elements_;
  int capacity_;
  Tagged_t elements_[1];
};

void* StringTable::Data::operator new(size_t size, int capacity) {
  // Make sure the size given is the size of the Data structure.
  DCHECK_EQ(size, sizeof(StringTable::Data));
  // Make sure that the elements_ array is at the end of Data, with no padding,
  // so that subsequent elements can be accessed as offsets from elements_.
  STATIC_ASSERT(offsetof(StringTable::Data, elements_) ==
                sizeof(StringTable::Data) - sizeof(Tagged_t));
  // Make sure that elements_ is aligned when StringTable::Data is aligned.
  STATIC_ASSERT(
      (alignof(StringTable::Data) + offsetof(StringTable::Data, elements_)) %
          kTaggedSize ==
      0);

  // Subtract 1 from capacity, as the member elements_ already supplies the
  // storage for the first element.
  return AlignedAlloc(size + (capacity - 1) * sizeof(Tagged_t),
                      alignof(StringTable::Data));
}

void StringTable::Data::operator delete(void* table) { AlignedFree(table); }

size_t StringTable::Data::GetCurrentMemoryUsage() const {
  size_t usage = sizeof(*this) + (capacity_ - 1) * sizeof(Tagged_t);
  if (previous_data_) {
    usage += previous_data_->GetCurrentMemoryUsage();
  }
  return usage;
}

StringTable::Data::Data(int capacity)
    : previous_data_(nullptr),
      number_of_elements_(0),
      number_of_deleted_elements_(0),
      capacity_(capacity) {
  OffHeapObjectSlot first_slot = slot(InternalIndex(0));
  MemsetTagged(first_slot, empty_element(), capacity);
}

std::unique_ptr<StringTable::Data> StringTable::Data::New(int capacity) {
  return std::unique_ptr<Data>(new (capacity) Data(capacity));
}

void StringTable::Data::Resize(const Isolate* isolate,
                               std::unique_ptr<Data>& data, int capacity) {
  std::unique_ptr<Data> new_data(new (capacity) Data(capacity));

  DCHECK_LT(data->number_of_elements(), new_data->capacity());
  DCHECK(StringTableHasSufficientCapacityToAdd(
      new_data->capacity(), new_data->number_of_elements(),
      new_data->number_of_deleted_elements(), data->number_of_elements()));

  // Rehash the elements.
  for (InternalIndex i : InternalIndex::Range(data->capacity())) {
    Object element = data->Get(isolate, i);
    if (element == empty_element() || element == deleted_element()) continue;
    String string = String::cast(element);
    uint32_t hash = string.Hash();
    InternalIndex insertion_index = new_data->FindInsertionEntry(isolate, hash);
    new_data->Set(insertion_index, string);
  }
  new_data->number_of_elements_ = data->number_of_elements();

  // unique_ptr swap dance to update `data` with `new_data`, move the old `data`
  // to `new_data->previous_data_`, all while ensuring that `data` is never
  // nullptr.
  std::unique_ptr<Data>& data_storage = new_data->previous_data_;
  data_storage = std::move(new_data);
  std::swap(data_storage, data);
}

template <typename StringTableKey>
InternalIndex StringTable::Data::FindEntry(const Isolate* isolate,
                                           StringTableKey* key,
                                           uint32_t hash) const {
  uint32_t count = 1;
  // EnsureCapacity will guarantee the hash table is never full.
  DCHECK_LT(number_of_elements_, capacity_);
  for (InternalIndex entry = FirstProbe(hash, capacity_);;
       entry = NextProbe(entry, count++, capacity_)) {
    // TODO(leszeks): Consider delaying the decompression until after the
    // comparisons against empty/deleted.
    Object element = Get(isolate, entry);
    if (element == empty_element()) return InternalIndex::NotFound();
    if (element == deleted_element()) continue;
    String string = String::cast(element);
    if (KeyIsMatch(key, string)) return entry;
  }
}

InternalIndex StringTable::Data::FindInsertionEntry(const Isolate* isolate,
                                                    uint32_t hash) const {
  uint32_t count = 1;
  // EnsureCapacity will guarantee the hash table is never full.
  DCHECK_LT(number_of_elements_, capacity_);
  for (InternalIndex entry = FirstProbe(hash, capacity_);;
       entry = NextProbe(entry, count++, capacity_)) {
    // TODO(leszeks): Consider delaying the decompression until after the
    // comparisons against empty/deleted.
    Object element = Get(isolate, entry);
    if (element == empty_element() || element == deleted_element())
      return entry;
  }
}

template <typename StringTableKey>
InternalIndex StringTable::Data::FindEntryOrInsertionEntry(
    const Isolate* isolate, StringTableKey* key, uint32_t hash) const {
  InternalIndex insertion_entry = InternalIndex::NotFound();
  uint32_t count = 1;
  // EnsureCapacity will guarantee the hash table is never full.
  DCHECK_LT(number_of_elements_, capacity_);
  for (InternalIndex entry = FirstProbe(hash, capacity_);;
       entry = NextProbe(entry, count++, capacity_)) {
    // TODO(leszeks): Consider delaying the decompression until after the
    // comparisons against empty/deleted.
    Object element = Get(isolate, entry);
    if (element == empty_element()) {
      // Empty entry, it's our insertion entry if there was no previous Hole.
      if (insertion_entry.is_not_found()) return entry;
      return insertion_entry;
    }

    if (element == deleted_element()) {
      // Holes are potential insertion candidates, but we continue the search
      // in case we find the actual matching entry.
      if (insertion_entry.is_not_found()) insertion_entry = entry;
      continue;
    }

    String string = String::cast(element);
    if (KeyIsMatch(key, string)) return entry;
  }
}

void StringTable::Data::IterateElements(RootVisitor* visitor) {
  OffHeapObjectSlot first_slot = slot(InternalIndex(0));
  OffHeapObjectSlot end_slot = slot(InternalIndex(capacity_));
  visitor->VisitRootPointers(Root::kStringTable, nullptr, first_slot, end_slot);
}

void StringTable::Data::Print(const Isolate* isolate) const {
  OFStream os(stdout);
  os << "StringTable {" << std::endl;
  for (InternalIndex i : InternalIndex::Range(capacity_)) {
    os << "  " << i.as_uint32() << ": " << Brief(Get(isolate, i)) << std::endl;
  }
  os << "}" << std::endl;
}

StringTable::StringTable() : data_(Data::New(kStringTableMinCapacity)) {}
StringTable::~StringTable() = default;

int StringTable::Capacity() const { return data_->capacity(); }
int StringTable::NumberOfElements() const {
  return data_->number_of_elements();
}

// InternalizedStringKey carries a string/internalized-string object as key.
class InternalizedStringKey final : public StringTableKey {
 public:
  explicit InternalizedStringKey(Handle<String> string)
      : StringTableKey(0, string->length()), string_(string) {
    DCHECK(!string->IsInternalizedString());
    DCHECK(string->IsFlat());
    // Make sure hash_field is computed.
    string->Hash();
    set_hash_field(string->hash_field());
  }

  bool IsMatch(String string) override { return string_->SlowEquals(string); }

  Handle<String> AsHandle(Isolate* isolate) {
    // Internalize the string if possible.
    MaybeHandle<Map> maybe_map =
        isolate->factory()->InternalizedStringMapForString(string_);
    Handle<Map> map;
    if (maybe_map.ToHandle(&map)) {
      string_->set_map_no_write_barrier(*map);
      DCHECK(string_->IsInternalizedString());
      return string_;
    }
    if (FLAG_thin_strings) {
      // External strings get special treatment, to avoid copying their
      // contents.
      if (string_->IsExternalOneByteString()) {
        return isolate->factory()
            ->InternalizeExternalString<ExternalOneByteString>(string_);
      } else if (string_->IsExternalTwoByteString()) {
        return isolate->factory()
            ->InternalizeExternalString<ExternalTwoByteString>(string_);
      }
    }
    // Otherwise allocate a new internalized string.
    return isolate->factory()->NewInternalizedStringImpl(
        string_, string_->length(), string_->hash_field());
  }

 private:
  Handle<String> string_;
};

Handle<String> StringTable::LookupString(Isolate* isolate,
                                         Handle<String> string) {
  string = String::Flatten(isolate, string);
  if (string->IsInternalizedString()) return string;

  InternalizedStringKey key(string);
  Handle<String> result = LookupKey(isolate, &key);

  if (FLAG_thin_strings) {
    if (!string->IsInternalizedString()) {
      string->MakeThin(isolate, *result);
    }
  } else {  // !FLAG_thin_strings
    if (string->IsConsString()) {
      Handle<ConsString> cons = Handle<ConsString>::cast(string);
      cons->set_first(*result);
      cons->set_second(ReadOnlyRoots(isolate).empty_string());
    } else if (string->IsSlicedString()) {
      STATIC_ASSERT(static_cast<int>(ConsString::kSize) ==
                    static_cast<int>(SlicedString::kSize));
      DisallowHeapAllocation no_gc;
      bool one_byte = result->IsOneByteRepresentation();
      Handle<Map> map = one_byte
                            ? isolate->factory()->cons_one_byte_string_map()
                            : isolate->factory()->cons_string_map();
      string->set_map(*map);
      Handle<ConsString> cons = Handle<ConsString>::cast(string);
      cons->set_first(*result);
      cons->set_second(ReadOnlyRoots(isolate).empty_string());
    }
  }
  return result;
}

template <typename StringTableKey, typename LocalIsolate>
Handle<String> StringTable::LookupKey(LocalIsolate* isolate,
                                      StringTableKey* key) {
  // String table lookups are allowed to be concurrent, assuming that:
  //
  //   - The Heap access is allowed to be concurrent (using LocalHeap or
  //     similar),
  //   - All writes to the string table are guarded by the Isolate string table
  //     mutex,
  //   - Resizes of the string table first copies the old contents to the new
  //     table, and only then sets the new string table pointer to the new
  //     table,
  //   - Only GCs can remove elements from the string table.
  //
  // These assumptions allow us to make the following statement:
  //
  //   "Reads are allowed when not holding the lock, as long as false negatives
  //    (misses) are ok. We will never get a false positive (hit of an entry no
  //    longer in the table)"
  //
  // This is because we _know_ that if we find an entry in the string table, any
  // entry will also be in all reallocations of that tables. This is required
  // for strong consistency of internalized string equality implying reference
  // equality.
  //
  // We therefore try to optimistically read from the string table without
  // taking the lock (both here and in the NoAllocate version of the lookup),
  // and on a miss we take the lock and try to write the entry, with a second
  // read lookup in case the non-locked read missed a write.
  //
  // One complication is allocation -- we don't want to allocate while holding
  // the string table lock. This applies to both allocation of new strings, and
  // re-allocation of the string table on resize. So, we optimistically allocate
  // (without copying values) outside the lock, and potentially discard the
  // allocation if another write also did an allocation. This assumes that
  // writes are rarer than reads.

  const Isolate* ptr_cmp_isolate = GetIsolateForPtrCompr(isolate);

  Handle<String> new_string;
  while (true) {
    // Load the current string table data, in case another thread updates the
    // data while we're reading.
    const StringTable::Data* data = data_.get();

    // First try to find the string in the table. This is safe to do even if the
    // table is now reallocated; we won't find a stale entry in the old table
    // because the new table won't delete it's corresponding entry until the
    // string is dead, in which case it will die in this table too and worst
    // case we'll have a false miss.
    InternalIndex entry = data->FindEntry(ptr_cmp_isolate, key, key->hash());
    if (entry.is_found()) {
      return handle(String::cast(data->Get(ptr_cmp_isolate, entry)), isolate);
    }

    // No entry found, so adding new string.

    // Allocate the string before the first insertion attempt, reuse this
    // allocated value on insertion retries. If another thread concurrently
    // allocates the same string, the insert will fail, the lookup above will
    // succeed, and this string will be discarded.
    if (new_string.is_null()) new_string = key->AsHandle(isolate);

    {
      base::MutexGuard table_write_guard(&write_mutex_);

      EnsureCapacity(ptr_cmp_isolate, 1);
      // Reload the data pointer in case EnsureCapacity changed it.
      StringTable::Data* data = data_.get();

      // Check one last time if the key is present in the table, in case it was
      // added after the check.
      InternalIndex entry =
          data->FindEntryOrInsertionEntry(ptr_cmp_isolate, key, key->hash());

      Object element = data->Get(ptr_cmp_isolate, entry);
      if (element == empty_element()) {
        // This entry is empty, so write it and register that we added an
        // element.
        data->Set(entry, *new_string);
        data->ElementAdded();
        return new_string;
      } else if (element == deleted_element()) {
        // This entry was deleted, so overwrite it and register that we
        // overwrote a deleted element.
        data->Set(entry, *new_string);
        data->DeletedElementOverwritten();
        return new_string;
      } else {
        // Return the existing string as a handle.
        return handle(String::cast(element), isolate);
      }
    }
  }
}

template Handle<String> StringTable::LookupKey(Isolate* isolate,
                                               OneByteStringKey* key);
template Handle<String> StringTable::LookupKey(Isolate* isolate,
                                               TwoByteStringKey* key);
template Handle<String> StringTable::LookupKey(Isolate* isolate,
                                               SeqOneByteSubStringKey* key);
template Handle<String> StringTable::LookupKey(Isolate* isolate,
                                               SeqTwoByteSubStringKey* key);

template Handle<String> StringTable::LookupKey(LocalIsolate* isolate,
                                               OneByteStringKey* key);
template Handle<String> StringTable::LookupKey(LocalIsolate* isolate,
                                               TwoByteStringKey* key);
template Handle<String> StringTable::LookupKey(LocalIsolate* isolate,
                                               SeqOneByteSubStringKey* key);
template Handle<String> StringTable::LookupKey(LocalIsolate* isolate,
                                               SeqTwoByteSubStringKey* key);

template Handle<String> StringTable::LookupKey(Isolate* isolate,
                                               StringTableInsertionKey* key);

void StringTable::EnsureCapacity(const Isolate* isolate,
                                 int additional_elements) {
  // This call is only allowed while the write mutex is held.
  write_mutex_.AssertHeld();

  StringTable::Data* data = data_.get();

  // Grow or shrink table if needed. We first try to shrink the table, if it
  // is sufficiently empty; otherwise we make sure to grow it so that it has
  // enough space.
  int current_capacity = data->capacity();
  int current_nof = data->number_of_elements();
  int capacity_after_shrinking =
      ComputeStringTableCapacityWithShrink(current_capacity, current_nof + 1);

  int new_capacity = -1;
  if (capacity_after_shrinking < current_capacity) {
    DCHECK(StringTableHasSufficientCapacityToAdd(capacity_after_shrinking,
                                                 current_nof, 0, 1));
    new_capacity = capacity_after_shrinking;
  } else if (!StringTableHasSufficientCapacityToAdd(
                 current_capacity, current_nof,
                 data->number_of_deleted_elements(), 1)) {
    new_capacity = ComputeStringTableCapacity(current_nof + 1);
  }

  if (new_capacity != -1) {
    Data::Resize(isolate, data_, new_capacity);
  }
}

// static
template <typename Char>
Address StringTable::Data::TryStringToIndexOrLookupExisting(Isolate* isolate,
                                                            String string,
                                                            String source,
                                                            size_t start) {
  // TODO(leszeks): This method doesn't really belong on StringTable::Data.
  // Ideally it would be a free function in an anonymous namespace, but that
  // causes issues around method and class visibility.

  DisallowHeapAllocation no_gc;
  uint64_t seed = HashSeed(isolate);

  int length = string.length();

  std::unique_ptr<Char[]> buffer;
  const Char* chars;

  if (source.IsConsString()) {
    DCHECK(!source.IsFlat());
    buffer.reset(new Char[length]);
    String::WriteToFlat(source, buffer.get(), 0, length);
    chars = buffer.get();
  } else {
    chars = source.GetChars<Char>(no_gc) + start;
  }
  // TODO(verwaest): Internalize to one-byte when possible.
  SequentialStringKey<Char> key(Vector<const Char>(chars, length), seed);

  // String could be an array index.
  uint32_t hash_field = key.hash_field();

  if (Name::ContainsCachedArrayIndex(hash_field)) {
    return Smi::FromInt(String::ArrayIndexValueBits::decode(hash_field)).ptr();
  }

  if ((hash_field & Name::kIsNotIntegerIndexMask) == 0) {
    // It is an index, but it's not cached.
    return Smi::FromInt(ResultSentinel::kUnsupported).ptr();
  }

  StringTable::Data* string_table_data = isolate->string_table()->data_.get();

  InternalIndex entry = string_table_data->FindEntry(isolate, &key, key.hash());
  if (entry.is_not_found()) {
    // A string that's not an array index, and not in the string table,
    // cannot have been used as a property name before.
    return Smi::FromInt(ResultSentinel::kNotFound).ptr();
  }

  String internalized = String::cast(string_table_data->Get(isolate, entry));
  if (FLAG_thin_strings) {
    string.MakeThin(isolate, internalized);
  }
  return internalized.ptr();
}

// static
Address StringTable::TryStringToIndexOrLookupExisting(Isolate* isolate,
                                                      Address raw_string) {
  String string = String::cast(Object(raw_string));
  DCHECK(!string.IsInternalizedString());

  // Valid array indices are >= 0, so they cannot be mixed up with any of
  // the result sentinels, which are negative.
  STATIC_ASSERT(
      !String::ArrayIndexValueBits::is_valid(ResultSentinel::kUnsupported));
  STATIC_ASSERT(
      !String::ArrayIndexValueBits::is_valid(ResultSentinel::kNotFound));

  size_t start = 0;
  String source = string;
  if (source.IsSlicedString()) {
    SlicedString sliced = SlicedString::cast(source);
    start = sliced.offset();
    source = sliced.parent();
  } else if (source.IsConsString() && source.IsFlat()) {
    source = ConsString::cast(source).first();
  }
  if (source.IsThinString()) {
    source = ThinString::cast(source).actual();
    if (string.length() == source.length()) {
      return source.ptr();
    }
  }

  StringTable* table = isolate->string_table();
  if (source.IsOneByteRepresentation()) {
    return table->data_->TryStringToIndexOrLookupExisting<uint8_t>(
        isolate, string, source, start);
  }
  return table->data_->TryStringToIndexOrLookupExisting<uint16_t>(
      isolate, string, source, start);
}

void StringTable::Print(const Isolate* isolate) const { data_->Print(isolate); }

size_t StringTable::GetCurrentMemoryUsage() const {
  return sizeof(*this) + data_->GetCurrentMemoryUsage();
}

void StringTable::IterateElements(RootVisitor* visitor) {
  data_->IterateElements(visitor);
}

void StringTable::DropOldData() { data_->DropPreviousData(); }

void StringTable::NotifyElementsRemoved(int count) {
  data_->ElementsRemoved(count);
}

}  // namespace internal
}  // namespace v8
