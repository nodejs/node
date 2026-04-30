// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_EFFECT_HANDLER_H_
#define V8_WASM_EFFECT_HANDLER_H_

#include "src/base/bit-field.h"

namespace v8 {
namespace internal {
namespace wasm {

struct __attribute__((packed)) EffectHandlerTagIndex {
  using IsSwitchField = base::BitField<bool, 0, 1>;
  using IndexField = base::BitField<int, 1, 20>;
  uint32_t tag_and_kind;

  bool is_switch() const { return IsSwitchField::decode(tag_and_kind); }
  int index() const { return IndexField::decode(tag_and_kind); }

  void encode(bool is_switch, int tag_index) {
    tag_and_kind =
        IsSwitchField::encode(is_switch) | IndexField::encode(tag_index);
  }

  uint32_t raw_value() const { return tag_and_kind; }
};
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_EFFECT_HANDLER_H_
