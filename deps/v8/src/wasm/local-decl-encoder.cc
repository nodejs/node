// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/local-decl-encoder.h"

#include "src/codegen/signature.h"
#include "src/wasm/leb-helper.h"

namespace v8 {
namespace internal {
namespace wasm {

size_t LocalDeclEncoder::Emit(uint8_t* buffer) const {
  uint8_t* pos = buffer;
  LEBHelper::write_u32v(&pos, static_cast<uint32_t>(local_decls_.size()));
  for (auto& local_decl : local_decls_) {
    uint32_t locals_count = local_decl.first;
    ValueType locals_type = local_decl.second;
    LEBHelper::write_u32v(&pos, locals_count);
    *pos = locals_type.value_type_code();
    ++pos;
    if (locals_type.encoding_needs_shared()) {
      *pos = kSharedFlagCode;
      ++pos;
    }
    if (locals_type.encoding_needs_heap_type()) {
      if (locals_type.encoding_needs_exact()) {
        *pos = kExactCode;
        ++pos;
      }
      LEBHelper::write_i32v(&pos, locals_type.heap_type().code());
    }
  }
  DCHECK_EQ(Size(), pos - buffer);
  return static_cast<size_t>(pos - buffer);
}

uint32_t LocalDeclEncoder::AddLocals(uint32_t count, ValueType type) {
  uint32_t result =
      static_cast<uint32_t>(total_ + (sig_ ? sig_->parameter_count() : 0));
  total_ += count;
  if (!local_decls_.empty() && local_decls_.back().second == type) {
    count += local_decls_.back().first;
    local_decls_.pop_back();
  }
  local_decls_.push_back(std::pair<uint32_t, ValueType>(count, type));
  return result;
}

// Size = (size of locals count) +
// (for each local pair <reps, type>, (size of reps) + (size of type))
size_t LocalDeclEncoder::Size() const {
  size_t size = LEBHelper::sizeof_u32v(local_decls_.size());
  for (auto p : local_decls_) {
    size += LEBHelper::sizeof_u32v(p.first) +  // number of locals
            1 +                                // Opcode
            (p.second.encoding_needs_shared() ? 1 : 0) +
            (p.second.encoding_needs_exact() ? 1 : 0) +
            (p.second.encoding_needs_heap_type()
                 ? LEBHelper::sizeof_i32v(p.second.heap_type().code())
                 : 0);
  }
  return size;
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
