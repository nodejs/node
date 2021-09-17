// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/string-table.h"

#include <atomic>

#include "src/base/atomicops.h"
#include "src/base/macros.h"
#include "src/common/assert-scope.h"
#include "src/common/globals.h"
#include "src/common/ptr-compr-inl.h"
#include "src/execution/isolate-utils-inl.h"
#include "src/heap/safepoint.h"
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
  return std::max(capacity, kStringTableMinCapacity);
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

template <typename IsolateT, typename StringTableKey>
bool KeyIsMatch(IsolateT* isolate, StringTableKey* key, String string) {
  if (string.hash() != key->hash()) return false;
  if (string.length() != key->length()) return false;
  return key->IsMatch(isolate, string);
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
  static std::unique_ptr<Data> Resize(PtrComprCageBase cage_base,
                                      std::unique_ptr<Data> data, int capacity);

  OffHeapObjectSlot slot(InternalIndex index) const {
    return OffHeapObjectSlot(&elements_[index.as_uint32()]);
  }

  Object Get(PtrComprCageBase cage_base, InternalIndex index) const {
    return slot(index).Acquire_Load(cage_base);
  }

  void Set(InternalIndex index, String entry) {
    slot(index).Release_Store(entry);
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

  template <typename IsolateT, typename StringTableKey>
  InternalIndex FindEntry(IsolateT* isolate, StringTableKey* key,
                          uint32_t hash) const;

  InternalIndex FindInsertionEntry(PtrComprCageBase cage_base,
                                   uint32_t hash) const;

  template <typename IsolateT, typename StringTableKey>
  InternalIndex FindEntryOrInsertionEntry(IsolateT* isolate,
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

  void Print(PtrComprCageBase cage_base) const;
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
  const int capacity_;
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

std::unique_ptr<StringTable::Data> StringTable::Data::Resize(
    PtrComprCageBase cage_base, std::unique_ptr<Data> data, int capacity) {
  std::unique_ptr<Data> new_data(new (capacity) Data(capacity));

  DCHECK_LT(data->number_of_elements(), new_data->capacity());
  DCHECK(StringTableHasSufficientCapacityToAdd(
      new_data->capacity(), new_data->number_of_elements(),
      new_data->number_of_deleted_elements(), data->number_of_elements()));

  // Rehash the elements.
  for (InternalIndex i : InternalIndex::Range(data->capacity())) {
    Object element = data->Get(cage_base, i);
    if (element == empty_element() || element == deleted_element()) continue;
    String string = String::cast(element);
    uint32_t hash = string.hash();
    InternalIndex insertion_index =
        new_data->FindInsertionEntry(cage_base, hash);
    new_data->Set(insertion_index, string);
  }
  new_data->number_of_elements_ = data->number_of_elements();

  new_data->previous_data_ = std::move(data);
  return new_data;
}

template <typename IsolateT, typename StringTableKey>
InternalIndex StringTable::Data::FindEntry(IsolateT* isolate,
                                           StringTableKey* key,
                                           uint32_t hash) const {
  uint32_t count = 1;
  // EnsureCapacity will guarantee the hash table is never full.
  for (InternalIndex entry = FirstProbe(hash, capacity_);;
       entry = NextProbe(entry, count++, capacity_)) {
    // TODO(leszeks): Consider delaying the decompression until after the
    // comparisons against empty/deleted.
    Object element = Get(isolate, entry);
    if (element == empty_element()) return InternalIndex::NotFound();
    if (element == deleted_element()) continue;
    String string = String::cast(element);
    if (KeyIsMatch(isolate, key, string)) return entry;
  }
}

InternalIndex StringTable::Data::FindInsertionEntry(PtrComprCageBase cage_base,
                                                    uint32_t hash) const {
  uint32_t count = 1;
  // EnsureCapacity will guarantee the hash table is never full.
  for (InternalIndex entry = FirstProbe(hash, capacity_);;
       entry = NextProbe(entry, count++, capacity_)) {
    // TODO(leszeks): Consider delaying the decompression until after the
    // comparisons against empty/deleted.
    Object element = Get(cage_base, entry);
    if (element == empty_element() || element == deleted_element())
      return entry;
  }
}

template <typename IsolateT, typename StringTableKey>
InternalIndex StringTable::Data::FindEntryOrInsertionEntry(
    IsolateT* isolate, StringTableKey* key, uint32_t hash) const {
  InternalIndex insertion_entry = InternalIndex::NotFound();
  uint32_t count = 1;
  // EnsureCapacity will guarantee the hash table is never full.
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
    if (KeyIsMatch(isolate, key, string)) return entry;
  }
}

void StringTable::Data::IterateElements(RootVisitor* visitor) {
  OffHeapObjectSlot first_slot = slot(InternalIndex(0));
  OffHeapObjectSlot end_slot = slot(InternalIndex(capacity_));
  visitor->VisitRootPointers(Root::kStringTable, nullptr, first_slot, end_slot);
}

void StringTable::Data::Print(PtrComprCageBase cage_base) const {
  OFStream os(stdout);
  os << "StringTable {" << std::endl;
  for (InternalIndex i : InternalIndex::Range(capacity_)) {
    os << "  " << i.as_uint32() << ": " << Brief(Get(cage_base, i))
       << std::endl;
  }
  os << "}" << std::endl;
}

StringTable::StringTable(Isolate* isolate)
    : data_(Data::New(kStringTableMinCapacity).release())
#ifdef DEBUG
      ,
      isolate_(isolate)
#endif
{
}
StringTable::~StringTable() { delete data_; }

int StringTable::Capacity() const {
  return data_.load(std::memory_order_acquire)->capacity();
}
int StringTable::NumberOfElements() const {
  {
    base::MutexGuard table_write_guard(&write_mutex_);
    return data_.load(std::memory_order_relaxed)->number_of_elements();
  }
}

// InternalizedStringKey carries a string/internalized-string object as key.
class InternalizedStringKey final : public StringTableKey {
 public:
  explicit InternalizedStringKey(Handle<String> string)
      : StringTableKey(0, string->length()), string_(string) {
    DCHECK(!string->IsInternalizedString());
    DCHECK(string->IsFlat());
    // Make sure hash_field is computed.
    string->EnsureHash();
    set_raw_hash_field(string->raw_hash_field());
  }

  bool IsMatch(Isolate* isolate, String string) {
    DCHECK(!SharedStringAccessGuardIfNeeded::IsNeeded(string));
    return string_->SlowEquals(string);
  }

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
    // External strings get special treatment, to avoid copying their
    // contents as long as they are not uncached.
    StringShape shape(*string_);
    if (shape.IsExternalOneByte() && !shape.IsUncachedExternal()) {
      return isolate->factory()
          ->InternalizeExternalString<ExternalOneByteString>(string_);
    } else if (shape.IsExternalTwoByte() && !shape.IsUncachedExternal()) {
      return isolate->factory()
          ->InternalizeExternalString<ExternalTwoByteString>(string_);
    } else {
      // Otherwise allocate a new internalized string.
      return isolate->factory()->NewInternalizedStringImpl(
          string_, string_->length(), string_->raw_hash_field());
    }
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

  if (!string->IsInternalizedString()) {
    string->MakeThin(isolate, *result);
  }
  return result;
}

template <typename StringTableKey, typename IsolateT>
Handle<String> StringTable::LookupKey(IsolateT* isolate, StringTableKey* key) {
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

  // Load the current string table data, in case another thread updates the
  // data while we're reading.
  const Data* data = data_.load(std::memory_order_acquire);

  // First try to find the string in the table. This is safe to do even if the
  // table is now reallocated; we won't find a stale entry in the old table
  // because the new table won't delete it's corresponding entry until the
  // string is dead, in which case it will die in this table too and worst
  // case we'll have a false miss.
  InternalIndex entry = data->FindEntry(isolate, key, key->hash());
  if (entry.is_found()) {
    return handle(String::cast(data->Get(isolate, entry)), isolate);
  }

  // No entry found, so adding new string.

  // Allocate the string before the first insertion attempt, reuse this
  // allocated value on insertion retries. If another thread concurrently
  // allocates the same string, the insert will fail, the lookup above will
  // succeed, and this string will be discarded.
  Handle<String> new_string = key->AsHandle(isolate);

  {
    base::MutexGuard table_write_guard(&write_mutex_);

    Data* data = EnsureCapacity(isolate, 1);

    // Check one last time if the key is present in the table, in case it was
    // added after the check.
    entry = data->FindEntryOrInsertionEntry(isolate, key, key->hash());

    Object element = data->Get(isolate, entry);
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

template Handle<String> StringTable::LookupKey(Isolate* isolate,
                                               StringTableInsertionKey* key);
template Handle<String> StringTable::LookupKey(LocalIsolate* isolate,
                                               StringTableInsertionKey* key);

StringTable::Data* StringTable::EnsureCapacity(PtrComprCageBase cage_base,
                                               int additional_elements) {
  // This call is only allowed while the write mutex is held.
  write_mutex_.AssertHeld();

  // This load can be relaxed as the table pointer can only be modified while
  // the lock is held.
  Data* data = data_.load(std::memory_order_relaxed);

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
    std::unique_ptr<Data> new_data =
        Data::Resize(cage_base, std::unique_ptr<Data>(data), new_capacity);
    // `new_data` is the new owner of `data`.
    DCHECK_EQ(new_data->PreviousData(), data);
    // Release-store the new data pointer as `data_`, so that it can be
    // acquire-loaded by other threads. This string table becomes the owner of
    // the pointer.
    data = new_data.release();
    data_.store(data, std::memory_order_release);
  }

  return data;
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

  DisallowGarbageCollection no_gc;
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
  SequentialStringKey<Char> key(base::Vector<const Char>(chars, length), seed);

  // String could be an array index.
  uint32_t raw_hash_field = key.raw_hash_field();

  if (Name::ContainsCachedArrayIndex(raw_hash_field)) {
    return Smi::FromInt(String::ArrayIndexValueBits::decode(raw_hash_field))
        .ptr();
  }

  if ((raw_hash_field & Name::kIsNotIntegerIndexMask) == 0) {
    // It is an index, but it's not cached.
    return Smi::FromInt(ResultSentinel::kUnsupported).ptr();
  }

  Data* string_table_data =
      isolate->string_table()->data_.load(std::memory_order_acquire);

  InternalIndex entry = string_table_data->FindEntry(isolate, &key, key.hash());
  if (entry.is_not_found()) {
    // A string that's not an array index, and not in the string table,
    // cannot have been used as a property name before.
    return Smi::FromInt(ResultSentinel::kNotFound).ptr();
  }

  String internalized = String::cast(string_table_data->Get(isolate, entry));
  string.MakeThin(isolate, internalized);
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

  if (source.IsOneByteRepresentation()) {
    return StringTable::Data::TryStringToIndexOrLookupExisting<uint8_t>(
        isolate, string, source, start);
  }
  return StringTable::Data::TryStringToIndexOrLookupExisting<uint16_t>(
      isolate, string, source, start);
}

void StringTable::Print(PtrComprCageBase cage_base) const {
  data_.load(std::memory_order_acquire)->Print(cage_base);
}

size_t StringTable::GetCurrentMemoryUsage() const {
  return sizeof(*this) +
         data_.load(std::memory_order_acquire)->GetCurrentMemoryUsage();
}

void StringTable::IterateElements(RootVisitor* visitor) {
  // This should only happen during garbage collection when background threads
  // are paused, so the load can be relaxed.
  DCHECK(isolate_->heap()->safepoint()->IsActive());
  data_.load(std::memory_order_relaxed)->IterateElements(visitor);
}

void StringTable::DropOldData() {
  // This should only happen during garbage collection when background threads
  // are paused, so the load can be relaxed.
  DCHECK(isolate_->heap()->safepoint()->IsActive());
  DCHECK_NE(isolate_->heap()->gc_state(), Heap::NOT_IN_GC);
  data_.load(std::memory_order_relaxed)->DropPreviousData();
}

void StringTable::NotifyElementsRemoved(int count) {
  // This should only happen during garbage collection when background threads
  // are paused, so the load can be relaxed.
  DCHECK(isolate_->heap()->safepoint()->IsActive());
  DCHECK_NE(isolate_->heap()->gc_state(), Heap::NOT_IN_GC);
  data_.load(std::memory_order_relaxed)->ElementsRemoved(count);
}

}  // namespace internal
}  // namespace v8
