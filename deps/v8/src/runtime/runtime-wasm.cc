// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/assembler.h"
#include "src/compiler/wasm-compiler.h"
#include "src/conversions.h"
#include "src/debug/debug.h"
#include "src/factory.h"
#include "src/frames-inl.h"
#include "src/objects-inl.h"
#include "src/v8memory.h"
#include "src/wasm/wasm-module.h"

namespace v8 {
namespace internal {

namespace {
const int kWasmMemArrayBuffer = 2;
}

RUNTIME_FUNCTION(Runtime_WasmGrowMemory) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  uint32_t delta_pages = 0;
  CHECK(args[0]->ToUint32(&delta_pages));
  Handle<JSObject> module_object;

  {
    // Get the module JSObject
    DisallowHeapAllocation no_allocation;
    const Address entry = Isolate::c_entry_fp(isolate->thread_local_top());
    Address pc =
        Memory::Address_at(entry + StandardFrameConstants::kCallerPCOffset);
    Code* code =
        isolate->inner_pointer_to_code_cache()->GetCacheEntry(pc)->code;
    FixedArray* deopt_data = code->deoptimization_data();
    DCHECK(deopt_data->length() == 2);
    module_object = Handle<JSObject>::cast(handle(deopt_data->get(0), isolate));
    CHECK(!module_object->IsNull(isolate));
  }

  Address old_mem_start, new_mem_start;
  uint32_t old_size, new_size;

  // Get mem buffer associated with module object
  Handle<Object> obj(module_object->GetInternalField(kWasmMemArrayBuffer),
                     isolate);

  if (obj->IsUndefined(isolate)) {
    // If module object does not have linear memory associated with it,
    // Allocate new array buffer of given size.
    old_mem_start = nullptr;
    old_size = 0;
    // TODO(gdeepti): Fix bounds check to take into account size of memtype.
    new_size = delta_pages * wasm::WasmModule::kPageSize;
    if (delta_pages > wasm::WasmModule::kMaxMemPages) {
      return *isolate->factory()->NewNumberFromInt(-1);
    }
    new_mem_start =
        static_cast<Address>(isolate->array_buffer_allocator()->Allocate(
            static_cast<uint32_t>(new_size)));
    if (new_mem_start == NULL) {
      return *isolate->factory()->NewNumberFromInt(-1);
    }
#if DEBUG
    // Double check the API allocator actually zero-initialized the memory.
    for (size_t i = old_size; i < new_size; i++) {
      DCHECK_EQ(0, new_mem_start[i]);
    }
#endif
  } else {
    Handle<JSArrayBuffer> old_buffer = Handle<JSArrayBuffer>::cast(obj);
    old_mem_start = static_cast<Address>(old_buffer->backing_store());
    old_size = old_buffer->byte_length()->Number();
    // If the old memory was zero-sized, we should have been in the
    // "undefined" case above.
    DCHECK_NOT_NULL(old_mem_start);
    DCHECK_NE(0, old_size);

    new_size = old_size + delta_pages * wasm::WasmModule::kPageSize;
    if (new_size >
        wasm::WasmModule::kMaxMemPages * wasm::WasmModule::kPageSize) {
      return *isolate->factory()->NewNumberFromInt(-1);
    }
    new_mem_start = static_cast<Address>(realloc(old_mem_start, new_size));
    if (new_mem_start == NULL) {
      return *isolate->factory()->NewNumberFromInt(-1);
    }
    old_buffer->set_is_external(true);
    isolate->heap()->UnregisterArrayBuffer(*old_buffer);
    // Zero initializing uninitialized memory from realloc
    memset(new_mem_start + old_size, 0, new_size - old_size);
  }

  Handle<JSArrayBuffer> buffer = isolate->factory()->NewJSArrayBuffer();
  JSArrayBuffer::Setup(buffer, isolate, false, new_mem_start, new_size);
  buffer->set_is_neuterable(false);

  // Set new buffer to be wasm memory
  module_object->SetInternalField(kWasmMemArrayBuffer, *buffer);

  CHECK(wasm::UpdateWasmModuleMemory(module_object, old_mem_start,
                                     new_mem_start, old_size, new_size));

  return *isolate->factory()->NewNumberFromInt(old_size /
                                               wasm::WasmModule::kPageSize);
}

RUNTIME_FUNCTION(Runtime_WasmThrowTypeError) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kWasmTrapTypeError));
}
}  // namespace internal
}  // namespace v8
