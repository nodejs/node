// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_PREPARSE_DATA_IMPL_H_
#define V8_PARSING_PREPARSE_DATA_IMPL_H_

#include <memory>

#include "src/common/assert-scope.h"
#include "src/parsing/preparse-data.h"

namespace v8 {
namespace internal {

// Classes which are internal to prepared-scope-data.cc, but are exposed in
// a header for tests.

// Wraps a ZoneVector<uint8_t> to have with functions named the same as
// Tagged<PodArray<uint8_t>>.
class ZoneVectorWrapper {
 public:
  class Inner {
   public:
    Inner() = default;
    explicit Inner(ZoneVector<uint8_t>* data) : data_(data) {}

    int data_length() const { return static_cast<int>(data_->size()); }
    uint8_t get(int index) const { return data_->at(index); }

   private:
    ZoneVector<uint8_t>* data_ = nullptr;
  };

  ZoneVectorWrapper() = default;
  explicit ZoneVectorWrapper(ZoneVector<uint8_t>* data) : inner_(data) {}

  const Inner* operator->() const { return &inner_; }

 private:
  Inner inner_;
};

template <class Data>
class BaseConsumedPreparseData : public ConsumedPreparseData {
 public:
  class ByteData : public PreparseByteDataConstants {
   public:
    // Reading from the ByteData is only allowed when a ReadingScope is on the
    // stack. This ensures that we have a DisallowGarbageCollection in place
    // whenever ByteData holds a raw pointer into the heap.
    class V8_NODISCARD ReadingScope {
     public:
      ReadingScope(ByteData* consumed_data, Data data)
          : consumed_data_(consumed_data) {
        consumed_data->data_ = data;
#ifdef DEBUG
        consumed_data->has_data_ = true;
#endif
      }
      explicit ReadingScope(BaseConsumedPreparseData<Data>* parent)
          : ReadingScope(parent->scope_data_.get(), parent->GetScopeData()) {}
      ~ReadingScope() {
#ifdef DEBUG
        consumed_data_->has_data_ = false;
#endif
      }

     private:
      ByteData* consumed_data_;
      DISALLOW_GARBAGE_COLLECTION(no_gc)
    };

    void SetPosition(int position) {
      DCHECK_LE(position, data_->data_length());
      index_ = position;
    }

    size_t RemainingBytes() const {
      DCHECK(has_data_);
      DCHECK_LE(index_, data_->data_length());
      return data_->data_length() - index_;
    }

    bool HasRemainingBytes(size_t bytes) const {
      DCHECK(has_data_);
      return index_ <= data_->data_length() && bytes <= RemainingBytes();
    }

    int32_t ReadUint32() {
      DCHECK(has_data_);
      DCHECK(HasRemainingBytes(kUint32Size));
      // Check that there indeed is an integer following.
      DCHECK_EQ(data_->get(index_++), kUint32Size);
      int32_t result = data_->get(index_) + (data_->get(index_ + 1) << 8) +
                       (data_->get(index_ + 2) << 16) +
                       (data_->get(index_ + 3) << 24);
      index_ += 4;
      stored_quarters_ = 0;
      return result;
    }

    int32_t ReadVarint32() {
      DCHECK(HasRemainingBytes(kVarint32MinSize));
      DCHECK_EQ(data_->get(index_++), kVarint32MinSize);
      int32_t value = 0;
      bool has_another_byte;
      unsigned shift = 0;
      do {
        uint8_t byte = data_->get(index_++);
        value |= static_cast<int32_t>(byte & 0x7F) << shift;
        shift += 7;
        has_another_byte = byte & 0x80;
      } while (has_another_byte);
      DCHECK_EQ(data_->get(index_++), kVarint32EndMarker);
      stored_quarters_ = 0;
      return value;
    }

    uint8_t ReadUint8() {
      DCHECK(has_data_);
      DCHECK(HasRemainingBytes(kUint8Size));
      // Check that there indeed is a byte following.
      DCHECK_EQ(data_->get(index_++), kUint8Size);
      stored_quarters_ = 0;
      return data_->get(index_++);
    }

    uint8_t ReadQuarter() {
      DCHECK(has_data_);
      if (stored_quarters_ == 0) {
        DCHECK(HasRemainingBytes(kUint8Size));
        // Check that there indeed are quarters following.
        DCHECK_EQ(data_->get(index_++), kQuarterMarker);
        stored_byte_ = data_->get(index_++);
        stored_quarters_ = 4;
      }
      // Read the first 2 bits from stored_byte_.
      uint8_t result = (stored_byte_ >> 6) & 3;
      DCHECK_LE(result, 3);
      --stored_quarters_;
      stored_byte_ <<= 2;
      return result;
    }

   private:
    Data data_ = {};
    int index_ = 0;
    uint8_t stored_quarters_ = 0;
    uint8_t stored_byte_ = 0;
#ifdef DEBUG
    bool has_data_ = false;
#endif
  };

  BaseConsumedPreparseData() : scope_data_(new ByteData()), child_index_(0) {}
  BaseConsumedPreparseData(const BaseConsumedPreparseData&) = delete;
  BaseConsumedPreparseData& operator=(const BaseConsumedPreparseData&) = delete;

  virtual Data GetScopeData() = 0;

  virtual ProducedPreparseData* GetChildData(Zone* zone, int child_index) = 0;

  ProducedPreparseData* GetDataForSkippableFunction(
      Zone* zone, int start_position, int* end_position, int* num_parameters,
      int* function_length, int* num_inner_functions, bool* uses_super_property,
      LanguageMode* language_mode) final;

  void RestoreScopeAllocationData(DeclarationScope* scope,
                                  AstValueFactory* ast_value_factory,
                                  Zone* zone) final;

#ifdef DEBUG
  bool VerifyDataStart();
#endif

 private:
  void RestoreDataForScope(Scope* scope, AstValueFactory* ast_value_factory,
                           Zone* zone);
  void RestoreDataForVariable(Variable* var);
  void RestoreDataForInnerScopes(Scope* scope,
                                 AstValueFactory* ast_value_factory,
                                 Zone* zone);

  std::unique_ptr<ByteData> scope_data_;
  // When consuming the data, these indexes point to the data we're going to
  // consume next.
  int child_index_;
};

// Implementation of ConsumedPreparseData for on-heap data.
class OnHeapConsumedPreparseData final
    : public BaseConsumedPreparseData<Tagged<PreparseData>> {
 public:
  OnHeapConsumedPreparseData(LocalIsolate* isolate, Handle<PreparseData> data);

  Tagged<PreparseData> GetScopeData() final;
  ProducedPreparseData* GetChildData(Zone* zone, int child_index) final;

 private:
  LocalIsolate* isolate_;
  Handle<PreparseData> data_;
};

// A serialized PreparseData in zone memory (as apposed to being on-heap).
class ZonePreparseData : public ZoneObject {
 public:
  V8_EXPORT_PRIVATE ZonePreparseData(Zone* zone,
                                     base::Vector<uint8_t>* byte_data,
                                     int child_length);

  ZonePreparseData(const ZonePreparseData&) = delete;
  ZonePreparseData& operator=(const ZonePreparseData&) = delete;

  Handle<PreparseData> Serialize(Isolate* isolate);
  Handle<PreparseData> Serialize(LocalIsolate* isolate);

  int children_length() const { return static_cast<int>(children_.size()); }

  ZonePreparseData* get_child(int index) { return children_[index]; }

  void set_child(int index, ZonePreparseData* child) {
    DCHECK_NOT_NULL(child);
    children_[index] = child;
  }

  ZoneVector<uint8_t>* byte_data() { return &byte_data_; }

 private:
  ZoneVector<uint8_t> byte_data_;
  ZoneVector<ZonePreparseData*> children_;
};

ZonePreparseData* PreparseDataBuilder::ByteData::CopyToZone(
    Zone* zone, int children_length) {
  DCHECK(is_finalized_);
  return zone->New<ZonePreparseData>(zone, &zone_byte_data_, children_length);
}

// Implementation of ConsumedPreparseData for PreparseData
// serialized into zone memory.
class ZoneConsumedPreparseData final
    : public BaseConsumedPreparseData<ZoneVectorWrapper> {
 public:
  ZoneConsumedPreparseData(Zone* zone, ZonePreparseData* data);

  ZoneVectorWrapper GetScopeData() final;
  ProducedPreparseData* GetChildData(Zone* zone, int child_index) final;

 private:
  ZonePreparseData* data_;
  ZoneVectorWrapper scope_data_wrapper_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_PREPARSE_DATA_IMPL_H_
