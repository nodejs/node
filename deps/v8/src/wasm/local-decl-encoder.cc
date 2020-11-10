// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/local-decl-encoder.h"

#include "src/codegen/signature.h"
#include "src/wasm/leb-helper.h"

namespace v8 {
namespace internal {
namespace wasm {

void LocalDeclEncoder::Prepend(Zone* zone, const byte** start,
                               const byte** end) const {
  size_t size = (*end - *start);
  byte* buffer = reinterpret_cast<byte*>(zone->New(Size() + size));
  size_t pos = Emit(buffer);
  if (size > 0) {
    memcpy(buffer + pos, *start, size);
  }
  pos += size;
  *start = buffer;
  *end = buffer + pos;
}

size_t LocalDeclEncoder::Emit(byte* buffer) const {
  byte* pos = buffer;
  LEBHelper::write_u32v(&pos, static_cast<uint32_t>(local_decls.size()));
  for (auto& local_decl : local_decls) {
    LEBHelper::write_u32v(&pos, local_decl.first);
    *pos = local_decl.second.value_type_code();
    ++pos;
    if (local_decl.second.has_immediate()) {
      LEBHelper::write_u32v(&pos, local_decl.second.ref_index());
    }
  }
  DCHECK_EQ(Size(), pos - buffer);
  return static_cast<size_t>(pos - buffer);
}

uint32_t LocalDeclEncoder::AddLocals(uint32_t count, ValueType type) {
  uint32_t result =
      static_cast<uint32_t>(total + (sig ? sig->parameter_count() : 0));
  total += count;
  if (local_decls.size() > 0 && local_decls.back().second == type) {
    count += local_decls.back().first;
    local_decls.pop_back();
  }
  local_decls.push_back(std::pair<uint32_t, ValueType>(count, type));
  return result;
}

// Size = (size of locals count) +
// (for each local pair <reps, type>, (size of reps) + (size of type))
size_t LocalDeclEncoder::Size() const {
  size_t size = LEBHelper::sizeof_u32v(local_decls.size());
  for (auto p : local_decls) {
    size +=
        LEBHelper::sizeof_u32v(p.first) +  // number of locals
        1 +                                // Opcode
        (p.second.has_immediate() ? LEBHelper::sizeof_u32v(p.second.ref_index())
                                  : 0);  // immediate
  }
  return size;
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
