// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/factory-base.h"

#include "src/ast/ast.h"
#include "src/handles/handles-inl.h"
#include "src/heap/factory.h"
#include "src/heap/off-thread-factory.h"
#include "src/objects/string-inl.h"
#include "src/utils/memcopy.h"

namespace v8 {
namespace internal {

template <typename Impl>
FactoryHandle<Impl, SeqOneByteString>
FactoryBase<Impl>::NewOneByteInternalizedString(
    const Vector<const uint8_t>& str, uint32_t hash_field) {
  FactoryHandle<Impl, SeqOneByteString> result =
      AllocateRawOneByteInternalizedString(str.length(), hash_field);
  DisallowHeapAllocation no_gc;
  MemCopy(result->GetChars(no_gc), str.begin(), str.length());
  return result;
}

template <typename Impl>
FactoryHandle<Impl, SeqTwoByteString>
FactoryBase<Impl>::NewTwoByteInternalizedString(const Vector<const uc16>& str,
                                                uint32_t hash_field) {
  FactoryHandle<Impl, SeqTwoByteString> result =
      AllocateRawTwoByteInternalizedString(str.length(), hash_field);
  DisallowHeapAllocation no_gc;
  MemCopy(result->GetChars(no_gc), str.begin(), str.length() * kUC16Size);
  return result;
}

template <typename Impl>
FactoryMaybeHandle<Impl, SeqOneByteString>
FactoryBase<Impl>::NewRawOneByteString(int length, AllocationType allocation) {
  if (length > String::kMaxLength || length < 0) {
    return impl()->template Throw<SeqOneByteString>(
        impl()->NewInvalidStringLengthError());
  }
  DCHECK_GT(length, 0);  // Use Factory::empty_string() instead.
  int size = SeqOneByteString::SizeFor(length);
  DCHECK_GE(SeqOneByteString::kMaxSize, size);

  HeapObject result = AllocateRawWithImmortalMap(
      size, allocation, read_only_roots().one_byte_string_map());
  FactoryHandle<Impl, SeqOneByteString> string =
      handle(SeqOneByteString::cast(result), impl());
  string->set_length(length);
  string->set_hash_field(String::kEmptyHashField);
  DCHECK_EQ(size, string->Size());
  return string;
}

template <typename Impl>
FactoryMaybeHandle<Impl, SeqTwoByteString>
FactoryBase<Impl>::NewRawTwoByteString(int length, AllocationType allocation) {
  if (length > String::kMaxLength || length < 0) {
    return impl()->template Throw<SeqTwoByteString>(
        impl()->NewInvalidStringLengthError());
  }
  DCHECK_GT(length, 0);  // Use Factory::empty_string() instead.
  int size = SeqTwoByteString::SizeFor(length);
  DCHECK_GE(SeqTwoByteString::kMaxSize, size);

  HeapObject result = AllocateRawWithImmortalMap(
      size, allocation, read_only_roots().string_map());
  FactoryHandle<Impl, SeqTwoByteString> string =
      handle(SeqTwoByteString::cast(result), impl());
  string->set_length(length);
  string->set_hash_field(String::kEmptyHashField);
  DCHECK_EQ(size, string->Size());
  return string;
}

template <typename Impl>
FactoryMaybeHandle<Impl, String> FactoryBase<Impl>::NewConsString(
    FactoryHandle<Impl, String> left, FactoryHandle<Impl, String> right,
    AllocationType allocation) {
  if (left->IsThinString()) {
    left = handle(ThinString::cast(*left).actual(), impl());
  }
  if (right->IsThinString()) {
    right = handle(ThinString::cast(*right).actual(), impl());
  }
  int left_length = left->length();
  if (left_length == 0) return right;
  int right_length = right->length();
  if (right_length == 0) return left;

  int length = left_length + right_length;

  if (length == 2) {
    uint16_t c1 = left->Get(0);
    uint16_t c2 = right->Get(0);
    return impl()->MakeOrFindTwoCharacterString(c1, c2);
  }

  // Make sure that an out of memory exception is thrown if the length
  // of the new cons string is too large.
  if (length > String::kMaxLength || length < 0) {
    return impl()->template Throw<String>(
        impl()->NewInvalidStringLengthError());
  }

  bool left_is_one_byte = left->IsOneByteRepresentation();
  bool right_is_one_byte = right->IsOneByteRepresentation();
  bool is_one_byte = left_is_one_byte && right_is_one_byte;

  // If the resulting string is small make a flat string.
  if (length < ConsString::kMinLength) {
    // Note that neither of the two inputs can be a slice because:
    STATIC_ASSERT(ConsString::kMinLength <= SlicedString::kMinLength);
    DCHECK(left->IsFlat());
    DCHECK(right->IsFlat());

    STATIC_ASSERT(ConsString::kMinLength <= String::kMaxLength);
    if (is_one_byte) {
      FactoryHandle<Impl, SeqOneByteString> result =
          NewRawOneByteString(length, allocation).ToHandleChecked();
      DisallowHeapAllocation no_gc;
      uint8_t* dest = result->GetChars(no_gc);
      // Copy left part.
      const uint8_t* src = left->template GetChars<uint8_t>(no_gc);
      CopyChars(dest, src, left_length);
      // Copy right part.
      src = right->template GetChars<uint8_t>(no_gc);
      CopyChars(dest + left_length, src, right_length);
      return result;
    }

    FactoryHandle<Impl, SeqTwoByteString> result =
        NewRawTwoByteString(length, allocation).ToHandleChecked();

    DisallowHeapAllocation pointer_stays_valid;
    uc16* sink = result->GetChars(pointer_stays_valid);
    String::WriteToFlat(*left, sink, 0, left->length());
    String::WriteToFlat(*right, sink + left->length(), 0, right->length());
    return result;
  }

  return NewConsString(left, right, length, is_one_byte, allocation);
}

template <typename Impl>
FactoryHandle<Impl, String> FactoryBase<Impl>::NewConsString(
    FactoryHandle<Impl, String> left, FactoryHandle<Impl, String> right,
    int length, bool one_byte, AllocationType allocation) {
  DCHECK(!left->IsThinString());
  DCHECK(!right->IsThinString());
  DCHECK_GE(length, ConsString::kMinLength);
  DCHECK_LE(length, String::kMaxLength);

  FactoryHandle<Impl, ConsString> result = handle(
      ConsString::cast(
          one_byte
              ? NewWithImmortalMap(read_only_roots().cons_one_byte_string_map(),
                                   allocation)
              : NewWithImmortalMap(read_only_roots().cons_string_map(),
                                   allocation)),
      impl());

  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = result->GetWriteBarrierMode(no_gc);

  result->set_hash_field(String::kEmptyHashField);
  result->set_length(length);
  result->set_first(*left, mode);
  result->set_second(*right, mode);
  return result;
}

template <typename Impl>
FactoryHandle<Impl, SeqOneByteString>
FactoryBase<Impl>::AllocateRawOneByteInternalizedString(int length,
                                                        uint32_t hash_field) {
  CHECK_GE(String::kMaxLength, length);
  // The canonical empty_string is the only zero-length string we allow.
  DCHECK_IMPLIES(length == 0, !impl()->EmptyStringRootIsInitialized());

  Map map = read_only_roots().one_byte_internalized_string_map();
  int size = SeqOneByteString::SizeFor(length);
  HeapObject result = AllocateRawWithImmortalMap(
      size,
      impl()->CanAllocateInReadOnlySpace() ? AllocationType::kReadOnly
                                           : AllocationType::kOld,
      map);
  FactoryHandle<Impl, SeqOneByteString> answer =
      handle(SeqOneByteString::cast(result), impl());
  answer->set_length(length);
  answer->set_hash_field(hash_field);
  DCHECK_EQ(size, answer->Size());
  return answer;
}

template <typename Impl>
FactoryHandle<Impl, SeqTwoByteString>
FactoryBase<Impl>::AllocateRawTwoByteInternalizedString(int length,
                                                        uint32_t hash_field) {
  CHECK_GE(String::kMaxLength, length);
  DCHECK_NE(0, length);  // Use Heap::empty_string() instead.

  Map map = read_only_roots().internalized_string_map();
  int size = SeqTwoByteString::SizeFor(length);
  HeapObject result =
      AllocateRawWithImmortalMap(size, AllocationType::kOld, map);
  FactoryHandle<Impl, SeqTwoByteString> answer =
      handle(SeqTwoByteString::cast(result), impl());
  answer->set_length(length);
  answer->set_hash_field(hash_field);
  DCHECK_EQ(size, result.Size());
  return answer;
}

template <typename Impl>
HeapObject FactoryBase<Impl>::NewWithImmortalMap(Map map,
                                                 AllocationType allocation) {
  return AllocateRawWithImmortalMap(map.instance_size(), allocation, map);
}

template <typename Impl>
HeapObject FactoryBase<Impl>::AllocateRawWithImmortalMap(
    int size, AllocationType allocation, Map map,
    AllocationAlignment alignment) {
  HeapObject result = AllocateRaw(size, allocation, alignment);
  result.set_map_after_allocation(map, SKIP_WRITE_BARRIER);
  return result;
}

template <typename Impl>
HeapObject FactoryBase<Impl>::AllocateRaw(int size, AllocationType allocation,
                                          AllocationAlignment alignment) {
  return impl()->AllocateRaw(size, allocation, alignment);
}

// Instantiate FactoryBase for the two variants we want.
template class EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE) FactoryBase<Factory>;
template class EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    FactoryBase<OffThreadFactory>;

}  // namespace internal
}  // namespace v8
