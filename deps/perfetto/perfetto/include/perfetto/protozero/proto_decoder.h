/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INCLUDE_PERFETTO_PROTOZERO_PROTO_DECODER_H_
#define INCLUDE_PERFETTO_PROTOZERO_PROTO_DECODER_H_

#include <stdint.h>
#include <array>
#include <memory>
#include <vector>

#include "perfetto/base/compiler.h"
#include "perfetto/base/logging.h"
#include "perfetto/protozero/field.h"
#include "perfetto/protozero/proto_utils.h"

namespace protozero {

// A generic protobuf decoder. Doesn't require any knowledge about the proto
// schema. It tokenizes fields, retrieves their ID and type and exposes
// accessors to retrieve its values.
// It does NOT recurse in nested submessages, instead it just computes their
// boundaries, recursion is left to the caller.
// This class is designed to be used in perf-sensitive contexts. It does not
// allocate and does not perform any proto semantic checks (e.g. repeated /
// required / optional). It's supposedly safe wrt out-of-bounds memory accesses
// (see proto_decoder_fuzzer.cc).
// This class serves also as a building block for TypedProtoDecoder, used when
// the schema is known at compile time.
class PERFETTO_EXPORT ProtoDecoder {
 public:
  // Creates a ProtoDecoder using the given |buffer| with size |length| bytes.
  ProtoDecoder(const void* buffer, size_t length)
      : begin_(reinterpret_cast<const uint8_t*>(buffer)),
        end_(begin_ + length),
        read_ptr_(begin_) {}
  ProtoDecoder(const std::string& str) : ProtoDecoder(str.data(), str.size()) {}
  ProtoDecoder(const ConstBytes& cb) : ProtoDecoder(cb.data, cb.size) {}

  // Reads the next field from the buffer and advances the read cursor. If a
  // full field cannot be read, the returned Field will be invalid (i.e.
  // field.valid() == false).
  Field ReadField();

  // Finds the first field with the given id. Doesn't affect the read cursor.
  Field FindField(uint32_t field_id);

  // Resets the read cursor to the start of the buffer.
  void Reset() { read_ptr_ = begin_; }

  // Resets the read cursor to the given position (must be within the buffer).
  void Reset(const uint8_t* pos) {
    PERFETTO_DCHECK(pos >= begin_ && pos < end_);
    read_ptr_ = pos;
  }

  // Returns the position of read cursor, relative to the start of the buffer.
  size_t read_offset() const { return static_cast<size_t>(read_ptr_ - begin_); }

  size_t bytes_left() const {
    PERFETTO_DCHECK(read_ptr_ <= end_);
    return static_cast<size_t>(end_ - read_ptr_);
  }

  const uint8_t* begin() const { return begin_; }
  const uint8_t* end() const { return end_; }

 protected:
  const uint8_t* const begin_;
  const uint8_t* const end_;
  const uint8_t* read_ptr_ = nullptr;
};

// An iterator-like class used to iterate through repeated fields. Used by
// TypedProtoDecoder. The iteration sequence is a bit counter-intuitive due to
// the fact that fields_[field_id] holds the *last* value of the field, not the
// first, but the remaining storage holds repeated fields in FIFO order.
// Assume that we push the 10,11,12 into a repeated field with ID=1.
//
// Decoder memory layout:  [  fields storage  ] [ repeated fields storage ]
// 1st iteration:           10
// 2nd iteration:           11                   10
// 3rd iteration:           12                   10 11
//
// We start the iteration @ fields_[num_fields], which is the start of the
// repeated fields storage, proceed until the end and lastly jump @ fields_[id].
template <typename T>
class RepeatedFieldIterator {
 public:
  RepeatedFieldIterator(uint32_t field_id,
                        const Field* begin,
                        const Field* end,
                        const Field* last)
      : field_id_(field_id), iter_(begin), end_(end), last_(last) {
    FindNextMatchingId();
  }

  // Constructs an invalid iterator.
  RepeatedFieldIterator()
      : field_id_(0u), iter_(nullptr), end_(nullptr), last_(nullptr) {}

  explicit operator bool() const { return iter_ != end_; }
  const Field& field() const { return *iter_; }

  T operator*() const {
    T val{};
    iter_->get(&val);
    return val;
  }
  const Field* operator->() const { return iter_; }

  RepeatedFieldIterator& operator++() {
    PERFETTO_DCHECK(iter_ != end_);
    if (iter_ == last_) {
      iter_ = end_;
      return *this;
    }
    ++iter_;
    FindNextMatchingId();
    return *this;
  }

  RepeatedFieldIterator operator++(int) {
    PERFETTO_DCHECK(iter_ != end_);
    RepeatedFieldIterator it(*this);
    ++(*this);
    return it;
  }

 private:
  void FindNextMatchingId() {
    PERFETTO_DCHECK(iter_ != last_);
    for (; iter_ != end_; ++iter_) {
      if (iter_->id() == field_id_)
        return;
    }
    iter_ = last_->valid() ? last_ : end_;
  }

  uint32_t field_id_;

  // Initially points to the beginning of the repeated field storage, then is
  // incremented as we call operator++().
  const Field* iter_;

  // Always points to fields_[size_], i.e. past the end of the storage.
  const Field* end_;

  // Always points to fields_[field_id].
  const Field* last_;
};

// As RepeatedFieldIterator, but allows iterating over a packed repeated field
// (which will be initially stored as a single length-delimited field).
// See |GetPackedRepeatedField| for details.
//
// Assumes little endianness, and that the input buffers are well formed -
// containing an exact multiple of encoded elements.
template <proto_utils::ProtoWireType wire_type, typename CppType>
class PackedRepeatedFieldIterator {
 public:
  PackedRepeatedFieldIterator(const uint8_t* data_begin,
                              size_t size,
                              bool* parse_error_ptr)
      : data_end_(data_begin ? data_begin + size : nullptr),
        read_ptr_(data_begin),
        parse_error_(parse_error_ptr) {
    using proto_utils::ProtoWireType;
    static_assert(wire_type == ProtoWireType::kVarInt ||
                      wire_type == ProtoWireType::kFixed32 ||
                      wire_type == ProtoWireType::kFixed64,
                  "invalid type");

    PERFETTO_DCHECK(parse_error_ptr);

    // Either the field is unset (and there are no data pointer), or the field
    // is set with a zero length payload. Mark the iterator as invalid in both
    // cases.
    if (size == 0) {
      curr_value_valid_ = false;
      return;
    }

    if ((wire_type == ProtoWireType::kFixed32 && (size % 4) != 0) ||
        (wire_type == ProtoWireType::kFixed64 && (size % 8) != 0)) {
      *parse_error_ = true;
      curr_value_valid_ = false;
      return;
    }

    ++(*this);
  }

  const CppType operator*() const { return curr_value_; }
  explicit operator bool() const { return curr_value_valid_; }

  PackedRepeatedFieldIterator& operator++() {
    using proto_utils::ProtoWireType;

    if (PERFETTO_UNLIKELY(!curr_value_valid_))
      return *this;

    if (PERFETTO_UNLIKELY(read_ptr_ == data_end_)) {
      curr_value_valid_ = false;
      return *this;
    }

    if (wire_type == ProtoWireType::kVarInt) {
      uint64_t new_value = 0;
      const uint8_t* new_pos =
          proto_utils::ParseVarInt(read_ptr_, data_end_, &new_value);

      if (PERFETTO_UNLIKELY(new_pos == read_ptr_)) {
        // Failed to decode the varint (probably incomplete buffer).
        *parse_error_ = true;
        curr_value_valid_ = false;
      } else {
        read_ptr_ = new_pos;
        curr_value_ = static_cast<CppType>(new_value);
      }
    } else {  // kFixed32 or kFixed64
      constexpr size_t kStep = wire_type == ProtoWireType::kFixed32 ? 4 : 8;

      // NB: the raw buffer is not guaranteed to be aligned, so neither are
      // these copies.
      memcpy(&curr_value_, read_ptr_, sizeof(CppType));
      read_ptr_ += kStep;
    }

    return *this;
  }

  PackedRepeatedFieldIterator operator++(int) {
    PackedRepeatedFieldIterator it(*this);
    ++(*this);
    return it;
  }

 private:
  // Might be null if the backing proto field isn't set.
  const uint8_t* const data_end_;

  // The iterator looks ahead by an element, so |curr_value| holds the value
  // to be returned when the caller dereferences the iterator, and |read_ptr_|
  // points at the start of the next element to be decoded.
  // |read_ptr_| might be null if the backing proto field isn't set.
  const uint8_t* read_ptr_;
  CppType curr_value_ = 0;

  // Set to false once we've exhausted the iterator, or encountered an error.
  bool curr_value_valid_ = true;

  // Where to set parsing errors, supplied by the caller.
  bool* const parse_error_;
};

// This decoder loads all fields upfront, without recursing in nested messages.
// It is used as a base class for typed decoders generated by the pbzero plugin.
// The split between TypedProtoDecoderBase and TypedProtoDecoder<> is to have
// unique definition of functions like ParseAllFields() and ExpandHeapStorage().
// The storage (either on-stack or on-heap) for this class is organized as
// follows:
// |-------------------------- fields_ ----------------------|
// [ field 0 (invalid) ] [ fields 1 .. N ] [ repeated fields ]
//                                        ^                  ^
//                                        num_fields_        size_
class PERFETTO_EXPORT TypedProtoDecoderBase : public ProtoDecoder {
 public:
  // If the field |id| is known at compile time, prefer the templated
  // specialization at<kFieldNumber>().
  const Field& Get(uint32_t id) const {
    return PERFETTO_LIKELY(id < num_fields_) ? fields_[id] : fields_[0];
  }

  // Returns an object that allows to iterate over all instances of a repeated
  // field given its id. Example usage:
  //   for (auto it = decoder.GetRepeated<int32_t>(N); it; ++it) { ... }
  template <typename T>
  RepeatedFieldIterator<T> GetRepeated(uint32_t field_id) const {
    return RepeatedFieldIterator<T>(field_id, &fields_[num_fields_],
                                    &fields_[size_], &fields_[field_id]);
  }

  // Returns an objects that allows to iterate over all entries of a packed
  // repeated field given its id and type. The |wire_type| is necessary for
  // decoding the packed field, the |cpp_type| is for convenience & stronger
  // typing.
  //
  // The caller must also supply a pointer to a bool that is set to true if the
  // packed buffer is found to be malformed while iterating (so you need to
  // exhaust the iterator if you want to check the full extent of the buffer).
  //
  // Note that unlike standard protobuf parsers, protozero does not allow
  // treating of packed repeated fields as non-packed and vice-versa (therefore
  // not making the packed option forwards and backwards compatible). So
  // the caller needs to use the right accessor for correct results.
  template <proto_utils::ProtoWireType wire_type, typename cpp_type>
  PackedRepeatedFieldIterator<wire_type, cpp_type> GetPackedRepeated(
      uint32_t field_id,
      bool* parse_error_location) const {
    const Field& field = Get(field_id);
    if (field.valid()) {
      return PackedRepeatedFieldIterator<wire_type, cpp_type>(
          field.data(), field.size(), parse_error_location);
    } else {
      return PackedRepeatedFieldIterator<wire_type, cpp_type>(
          nullptr, 0, parse_error_location);
    }
  }

 protected:
  TypedProtoDecoderBase(Field* storage,
                        uint32_t num_fields,
                        uint32_t capacity,
                        const uint8_t* buffer,
                        size_t length)
      : ProtoDecoder(buffer, length),
        fields_(storage),
        num_fields_(num_fields),
        size_(num_fields),
        capacity_(capacity) {
    // The reason why Field needs to be trivially de/constructible is to avoid
    // implicit initializers on all the ~1000 entries. We need it to initialize
    // only on the first |max_field_id| fields, the remaining capacity doesn't
    // require initialization.
    static_assert(std::is_trivially_constructible<Field>::value &&
                      std::is_trivially_destructible<Field>::value &&
                      std::is_trivial<Field>::value,
                  "Field must be a trivial aggregate type");
    memset(fields_, 0, sizeof(Field) * num_fields_);
  }

  void ParseAllFields();

  // Called when the default on-stack storage is exhausted and new repeated
  // fields need to be pushed.
  void ExpandHeapStorage();

  // Used only in presence of a large number of repeated fields, when the
  // default on-stack storage is exhausted.
  std::unique_ptr<Field[]> heap_storage_;

  // Points to the storage, either on-stack (default, provided by the template
  // specialization) or |heap_storage_| after ExpandHeapStorage() is called, in
  // case of a large number of repeated fields.
  Field* fields_;

  // Number of fields without accounting repeated storage. This is equal to
  // MAX_FIELD_ID + 1 (to account for the invalid 0th field).
  // This value is always <= size_ (and hence <= capacity);
  uint32_t num_fields_;

  // Number of active |fields_| entries. This is initially equal to the highest
  // number of fields for the message (num_fields_ == MAX_FIELD_ID + 1) and can
  // grow up to |capacity_| in the case of repeated fields.
  uint32_t size_;

  // Initially equal to kFieldsCapacity of the TypedProtoDecoder
  // specialization. Can grow when falling back on heap-based storage, in which
  // case it represents the size (#fields with each entry of a repeated field
  // counted individually) of the |heap_storage_| array.
  uint32_t capacity_;
};

// Template class instantiated by the auto-generated decoder classes declared in
// xxx.pbzero.h files.
template <int MAX_FIELD_ID, bool HAS_NONPACKED_REPEATED_FIELDS>
class TypedProtoDecoder : public TypedProtoDecoderBase {
 public:
  TypedProtoDecoder(const uint8_t* buffer, size_t length)
      : TypedProtoDecoderBase(on_stack_storage_,
                              /*num_fields=*/MAX_FIELD_ID + 1,
                              kCapacity,
                              buffer,
                              length) {
    static_assert(MAX_FIELD_ID <= kMaxDecoderFieldId, "Field ordinal too high");
    TypedProtoDecoderBase::ParseAllFields();
  }

  template <uint32_t FIELD_ID>
  const Field& at() const {
    static_assert(FIELD_ID <= MAX_FIELD_ID, "FIELD_ID > MAX_FIELD_ID");
    return fields_[FIELD_ID];
  }

  TypedProtoDecoder(TypedProtoDecoder&& other) noexcept
      : TypedProtoDecoderBase(std::move(other)) {
    // If the moved-from decoder was using on-stack storage, we need to update
    // our pointer to point to this decoder's on-stack storage.
    if (fields_ == other.on_stack_storage_) {
      fields_ = on_stack_storage_;
      memcpy(on_stack_storage_, other.on_stack_storage_,
             sizeof(on_stack_storage_));
    }
  }

 private:
  // In the case of non-repeated fields, this constant defines the highest field
  // id we are able to decode. This is to limit the on-stack storage.
  // In the case of repeated fields, this constant defines the max number of
  // repeated fields that we'll be able to store before falling back on the
  // heap. Keep this value in sync with the one in protozero_generator.cc.
  static constexpr size_t kMaxDecoderFieldId = 999;

  // If we the message has no repeated fields we need at most N Field entries
  // in the on-stack storage, where N is the highest field id.
  // Otherwise we need some room to store repeated fields.
  static constexpr size_t kCapacity =
      1 + (HAS_NONPACKED_REPEATED_FIELDS ? kMaxDecoderFieldId : MAX_FIELD_ID);

  Field on_stack_storage_[kCapacity];
};

}  // namespace protozero

#endif  // INCLUDE_PERFETTO_PROTOZERO_PROTO_DECODER_H_
