// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_EXTERNAL_REFERENCE_ENCODER_H_
#define V8_CODEGEN_EXTERNAL_REFERENCE_ENCODER_H_

#include <vector>

#include "src/base/bit-field.h"
#include "src/common/globals.h"
#include "src/utils/address-map.h"

namespace v8 {
namespace internal {

class Isolate;

class ExternalReferenceEncoder {
 public:
  class Value {
   public:
    explicit Value(uint32_t raw) : value_(raw) {}
    Value() : value_(0) {}
    static uint32_t Encode(uint32_t index, bool is_from_api) {
      return Index::encode(index) | IsFromAPI::encode(is_from_api);
    }

    bool is_from_api() const { return IsFromAPI::decode(value_); }
    uint32_t index() const { return Index::decode(value_); }

   private:
    using Index = base::BitField<uint32_t, 0, 31>;
    using IsFromAPI = base::BitField<bool, 31, 1>;
    uint32_t value_;
  };

  explicit ExternalReferenceEncoder(Isolate* isolate);
  ExternalReferenceEncoder(const ExternalReferenceEncoder&) = delete;
  ExternalReferenceEncoder& operator=(const ExternalReferenceEncoder&) = delete;
#ifdef DEBUG
  ~ExternalReferenceEncoder();
#endif  // DEBUG

  Value Encode(Address key) const;
  Maybe<Value> TryEncode(Address key) const;

  const char* NameOfAddress(Isolate* isolate, Address address) const;

 private:
  AddressToIndexHashMap* map_;

#ifdef DEBUG
  mutable std::vector<int> count_;
  mutable const intptr_t* api_references_;
#endif  // DEBUG
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_EXTERNAL_REFERENCE_ENCODER_H_
