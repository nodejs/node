// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/assembler-buffer-cache.h"

#include <algorithm>

#include "src/codegen/assembler.h"
#include "src/wasm/wasm-engine.h"

namespace v8::internal::wasm {

class CachedAssemblerBuffer final : public AssemblerBuffer {
 public:
  CachedAssemblerBuffer(AssemblerBufferCache* cache, base::AddressRegion region)
      : cache_(cache), region_(region) {}

  ~CachedAssemblerBuffer() override { cache_->Return(region_); }

  uint8_t* start() const override {
    return reinterpret_cast<uint8_t*>(region_.begin());
  }

  int size() const override { return static_cast<int>(region_.size()); }

  std::unique_ptr<AssemblerBuffer> Grow(int new_size) override {
    return cache_->GetAssemblerBuffer(new_size);
  }

 private:
  AssemblerBufferCache* const cache_;
  const base::AddressRegion region_;
};

AssemblerBufferCache::~AssemblerBufferCache() {
  for (base::AddressRegion region : available_memory_.regions()) {
    GetWasmCodeManager()->FreeAssemblerBufferSpace(region);
  }
}

std::unique_ptr<AssemblerBuffer> AssemblerBufferCache::GetAssemblerBuffer(
    int size) {
  DCHECK_LT(0, size);
  base::AddressRegion region = available_memory_.Allocate(size);
  if (region.is_empty()) {
    static constexpr int kMinimumReservation = 64 * KB;
    int minimum_allocation =
        std::max(kMinimumReservation, std::max(total_allocated_ / 4, size));
    base::AddressRegion new_space =
        GetWasmCodeManager()->AllocateAssemblerBufferSpace(minimum_allocation);
    available_memory_.Merge(new_space);
    CHECK_GE(kMaxInt - total_allocated_, new_space.size());
    total_allocated_ += new_space.size();

    region = available_memory_.Allocate(size);
    DCHECK(!region.is_empty());
  }
  return std::make_unique<CachedAssemblerBuffer>(this, region);
}

void AssemblerBufferCache::Return(base::AddressRegion region) {
  available_memory_.Merge(region);
}

}  // namespace v8::internal::wasm
