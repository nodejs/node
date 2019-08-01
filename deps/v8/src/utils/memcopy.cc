// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/utils/memcopy.h"

#include "src/snapshot/embedded/embedded-data.h"

namespace v8 {
namespace internal {

#if V8_TARGET_ARCH_IA32
static void MemMoveWrapper(void* dest, const void* src, size_t size) {
  memmove(dest, src, size);
}

// Initialize to library version so we can call this at any time during startup.
static MemMoveFunction memmove_function = &MemMoveWrapper;

// Copy memory area to disjoint memory area.
V8_EXPORT_PRIVATE void MemMove(void* dest, const void* src, size_t size) {
  if (size == 0) return;
  // Note: here we rely on dependent reads being ordered. This is true
  // on all architectures we currently support.
  (*memmove_function)(dest, src, size);
}
#elif V8_OS_POSIX && V8_HOST_ARCH_ARM
void MemCopyUint16Uint8Wrapper(uint16_t* dest, const uint8_t* src,
                               size_t chars) {
  uint16_t* limit = dest + chars;
  while (dest < limit) {
    *dest++ = static_cast<uint16_t>(*src++);
  }
}

V8_EXPORT_PRIVATE MemCopyUint8Function memcopy_uint8_function =
    &MemCopyUint8Wrapper;
MemCopyUint16Uint8Function memcopy_uint16_uint8_function =
    &MemCopyUint16Uint8Wrapper;
#elif V8_OS_POSIX && V8_HOST_ARCH_MIPS
V8_EXPORT_PRIVATE MemCopyUint8Function memcopy_uint8_function =
    &MemCopyUint8Wrapper;
#endif

void init_memcopy_functions() {
#if V8_TARGET_ARCH_IA32
  if (Isolate::CurrentEmbeddedBlobIsBinaryEmbedded()) {
    EmbeddedData d = EmbeddedData::FromBlob();
    memmove_function = reinterpret_cast<MemMoveFunction>(
        d.InstructionStartOfBuiltin(Builtins::kMemMove));
  }
#elif V8_OS_POSIX && V8_HOST_ARCH_ARM
  if (Isolate::CurrentEmbeddedBlobIsBinaryEmbedded()) {
    EmbeddedData d = EmbeddedData::FromBlob();
    memcopy_uint8_function = reinterpret_cast<MemCopyUint8Function>(
        d.InstructionStartOfBuiltin(Builtins::kMemCopyUint8Uint8));
    memcopy_uint16_uint8_function =
        reinterpret_cast<MemCopyUint16Uint8Function>(
            d.InstructionStartOfBuiltin(Builtins::kMemCopyUint16Uint8));
  }
#elif V8_OS_POSIX && V8_HOST_ARCH_MIPS
  if (Isolate::CurrentEmbeddedBlobIsBinaryEmbedded()) {
    EmbeddedData d = EmbeddedData::FromBlob();
    memcopy_uint8_function = reinterpret_cast<MemCopyUint8Function>(
        d.InstructionStartOfBuiltin(Builtins::kMemCopyUint8Uint8));
  }
#endif
}

}  // namespace internal
}  // namespace v8
