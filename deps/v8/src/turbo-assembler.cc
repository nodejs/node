// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/turbo-assembler.h"

#include "src/builtins/builtins.h"
#include "src/builtins/constants-table-builder.h"
#include "src/heap/heap-inl.h"
#include "src/lsan.h"
#include "src/snapshot/serializer-common.h"

namespace v8 {
namespace internal {

TurboAssemblerBase::TurboAssemblerBase(Isolate* isolate,
                                       const AssemblerOptions& options,
                                       void* buffer, int buffer_size,
                                       CodeObjectRequired create_code_object)
    : Assembler(options, buffer, buffer_size), isolate_(isolate) {
  if (create_code_object == CodeObjectRequired::kYes) {
    code_object_ = Handle<HeapObject>::New(
        ReadOnlyRoots(isolate).self_reference_marker(), isolate);
  }
}

void TurboAssemblerBase::IndirectLoadConstant(Register destination,
                                              Handle<HeapObject> object) {
  CHECK(root_array_available_);

  // Before falling back to the (fairly slow) lookup from the constants table,
  // check if any of the fast paths can be applied.

  int builtin_index;
  RootIndex root_index;
  if (isolate()->heap()->IsRootHandle(object, &root_index)) {
    // Roots are loaded relative to the root register.
    LoadRoot(destination, root_index);
  } else if (isolate()->builtins()->IsBuiltinHandle(object, &builtin_index)) {
    // Similar to roots, builtins may be loaded from the builtins table.
    LoadRootRelative(destination,
                     RootRegisterOffsetForBuiltinIndex(builtin_index));
  } else if (object.is_identical_to(code_object_) &&
             Builtins::IsBuiltinId(maybe_builtin_index_)) {
    // The self-reference loaded through Codevalue() may also be a builtin
    // and thus viable for a fast load.
    LoadRootRelative(destination,
                     RootRegisterOffsetForBuiltinIndex(maybe_builtin_index_));
  } else {
    CHECK(isolate()->ShouldLoadConstantsFromRootList());
    // Ensure the given object is in the builtins constants table and fetch its
    // index.
    BuiltinsConstantsTableBuilder* builder =
        isolate()->builtins_constants_table_builder();
    uint32_t index = builder->AddObject(object);

    // Slow load from the constants table.
    LoadFromConstantsTable(destination, index);
  }
}

void TurboAssemblerBase::IndirectLoadExternalReference(
    Register destination, ExternalReference reference) {
  CHECK(root_array_available_);

  if (IsAddressableThroughRootRegister(isolate(), reference)) {
    // Some external references can be efficiently loaded as an offset from
    // kRootRegister.
    intptr_t offset =
        RootRegisterOffsetForExternalReference(isolate(), reference);
    LoadRootRegisterOffset(destination, offset);
  } else {
    // Otherwise, do a memory load from the external reference table.

    // Encode as an index into the external reference table stored on the
    // isolate.
    ExternalReferenceEncoder encoder(isolate());
    ExternalReferenceEncoder::Value v = encoder.Encode(reference.address());
    CHECK(!v.is_from_api());

    LoadRootRelative(destination,
                     RootRegisterOffsetForExternalReferenceIndex(v.index()));
  }
}

// static
int32_t TurboAssemblerBase::RootRegisterOffset(RootIndex root_index) {
  return (static_cast<int32_t>(root_index) << kPointerSizeLog2) -
         kRootRegisterBias;
}

// static
int32_t TurboAssemblerBase::RootRegisterOffsetForExternalReferenceIndex(
    int reference_index) {
  return Heap::roots_to_external_reference_table_offset() - kRootRegisterBias +
         ExternalReferenceTable::OffsetOfEntry(reference_index);
}

// static
intptr_t TurboAssemblerBase::RootRegisterOffsetForExternalReference(
    Isolate* isolate, const ExternalReference& reference) {
  return static_cast<intptr_t>(reference.address()) - kRootRegisterBias -
         reinterpret_cast<intptr_t>(isolate->heap()->roots_array_start());
}

// static
bool TurboAssemblerBase::IsAddressableThroughRootRegister(
    Isolate* isolate, const ExternalReference& reference) {
  Address address = reference.address();
  return isolate->root_register_addressable_region().contains(address);
}

// static
int32_t TurboAssemblerBase::RootRegisterOffsetForBuiltinIndex(
    int builtin_index) {
  return Heap::roots_to_builtins_offset() - kRootRegisterBias +
         builtin_index * kPointerSize;
}

void TurboAssemblerBase::RecordCommentForOffHeapTrampoline(int builtin_index) {
  if (!FLAG_code_comments) return;
  size_t len = strlen("-- Inlined Trampoline to  --") +
               strlen(Builtins::name(builtin_index)) + 1;
  Vector<char> buffer = Vector<char>::New(static_cast<int>(len));
  char* buffer_start = buffer.start();
  LSAN_IGNORE_OBJECT(buffer_start);
  SNPrintF(buffer, "-- Inlined Trampoline to %s --",
           Builtins::name(builtin_index));
  RecordComment(buffer_start);
}

}  // namespace internal
}  // namespace v8
