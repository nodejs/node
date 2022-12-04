// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/embedded/embedded-data.h"

#include "src/codegen/assembler-inl.h"
#include "src/codegen/callable.h"
#include "src/snapshot/embedded/embedded-data-inl.h"
#include "src/snapshot/snapshot-utils.h"

namespace v8 {
namespace internal {

namespace {

Builtin TryLookupCode(const EmbeddedData& d, Address address) {
  if (!d.IsInCodeRange(address)) return Builtin::kNoBuiltinId;

  if (address < d.InstructionStartOfBuiltin(static_cast<Builtin>(0))) {
    return Builtin::kNoBuiltinId;
  }

  // Note: Addresses within the padding section between builtins (i.e. within
  // start + size <= address < start + padded_size) are interpreted as belonging
  // to the preceding builtin.

  int l = 0, r = Builtins::kBuiltinCount;
  while (l < r) {
    const int mid = (l + r) / 2;
    const Builtin builtin = Builtins::FromInt(mid);
    Address start = d.InstructionStartOfBuiltin(builtin);
    Address end = start + d.PaddedInstructionSizeOfBuiltin(builtin);

    if (address < start) {
      r = mid;
    } else if (address >= end) {
      l = mid + 1;
    } else {
      return builtin;
    }
  }

  UNREACHABLE();
}

}  // namespace

// static
bool OffHeapInstructionStream::PcIsOffHeap(Isolate* isolate, Address pc) {
  // Mksnapshot calls this while the embedded blob is not available yet.
  if (isolate->embedded_blob_code() == nullptr) return false;
  DCHECK_NOT_NULL(Isolate::CurrentEmbeddedBlobCode());

  if (EmbeddedData::FromBlob(isolate).IsInCodeRange(pc)) return true;
  return isolate->is_short_builtin_calls_enabled() &&
         EmbeddedData::FromBlob().IsInCodeRange(pc);
}

// static
bool OffHeapInstructionStream::TryGetAddressForHashing(
    Isolate* isolate, Address address, uint32_t* hashable_address) {
  // Mksnapshot calls this while the embedded blob is not available yet.
  if (isolate->embedded_blob_code() == nullptr) return false;
  DCHECK_NOT_NULL(Isolate::CurrentEmbeddedBlobCode());

  EmbeddedData d = EmbeddedData::FromBlob(isolate);
  if (d.IsInCodeRange(address)) {
    *hashable_address = d.AddressForHashing(address);
    return true;
  }

  if (isolate->is_short_builtin_calls_enabled()) {
    d = EmbeddedData::FromBlob();
    if (d.IsInCodeRange(address)) {
      *hashable_address = d.AddressForHashing(address);
      return true;
    }
  }
  return false;
}

// static
Builtin OffHeapInstructionStream::TryLookupCode(Isolate* isolate,
                                                Address address) {
  // Mksnapshot calls this while the embedded blob is not available yet.
  if (isolate->embedded_blob_code() == nullptr) return Builtin::kNoBuiltinId;
  DCHECK_NOT_NULL(Isolate::CurrentEmbeddedBlobCode());

  Builtin builtin = i::TryLookupCode(EmbeddedData::FromBlob(isolate), address);

  if (isolate->is_short_builtin_calls_enabled() &&
      !Builtins::IsBuiltinId(builtin)) {
    builtin = i::TryLookupCode(EmbeddedData::FromBlob(), address);
  }

#ifdef V8_COMPRESS_POINTERS_IN_SHARED_CAGE
  if (V8_SHORT_BUILTIN_CALLS_BOOL && !Builtins::IsBuiltinId(builtin)) {
    // When shared pointer compression cage is enabled and it has the embedded
    // code blob copy then it could have been used regardless of whether the
    // isolate uses it or knows about it or not (see
    // Code::OffHeapInstructionStart()).
    // So, this blob has to be checked too.
    CodeRange* code_range = CodeRange::GetProcessWideCodeRange().get();
    if (code_range && code_range->embedded_blob_code_copy() != nullptr) {
      builtin = i::TryLookupCode(EmbeddedData::FromBlob(code_range), address);
    }
  }
#endif
  return builtin;
}

// static
void OffHeapInstructionStream::CreateOffHeapOffHeapInstructionStream(
    Isolate* isolate, uint8_t** code, uint32_t* code_size, uint8_t** data,
    uint32_t* data_size) {
  // Create the embedded blob from scratch using the current Isolate's heap.
  EmbeddedData d = EmbeddedData::FromIsolate(isolate);

  // Allocate the backing store that will contain the embedded blob in this
  // Isolate. The backing store is on the native heap, *not* on V8's garbage-
  // collected heap.
  v8::PageAllocator* page_allocator = v8::internal::GetPlatformPageAllocator();
  const uint32_t alignment =
      static_cast<uint32_t>(page_allocator->AllocatePageSize());

  void* const requested_allocation_code_address =
      AlignedAddress(isolate->heap()->GetRandomMmapAddr(), alignment);
  const uint32_t allocation_code_size = RoundUp(d.code_size(), alignment);
  uint8_t* allocated_code_bytes = static_cast<uint8_t*>(AllocatePages(
      page_allocator, requested_allocation_code_address, allocation_code_size,
      alignment, PageAllocator::kReadWrite));
  CHECK_NOT_NULL(allocated_code_bytes);

  void* const requested_allocation_data_address =
      AlignedAddress(isolate->heap()->GetRandomMmapAddr(), alignment);
  const uint32_t allocation_data_size = RoundUp(d.data_size(), alignment);
  uint8_t* allocated_data_bytes = static_cast<uint8_t*>(AllocatePages(
      page_allocator, requested_allocation_data_address, allocation_data_size,
      alignment, PageAllocator::kReadWrite));
  CHECK_NOT_NULL(allocated_data_bytes);

  // Copy the embedded blob into the newly allocated backing store. Switch
  // permissions to read-execute since builtin code is immutable from now on
  // and must be executable in case any JS execution is triggered.
  //
  // Once this backing store is set as the current_embedded_blob, V8 cannot tell
  // the difference between a 'real' embedded build (where the blob is embedded
  // in the binary) and what we are currently setting up here (where the blob is
  // on the native heap).
  std::memcpy(allocated_code_bytes, d.code(), d.code_size());
  if (v8_flags.experimental_flush_embedded_blob_icache) {
    FlushInstructionCache(allocated_code_bytes, d.code_size());
  }
  CHECK(SetPermissions(page_allocator, allocated_code_bytes,
                       allocation_code_size, PageAllocator::kReadExecute));

  std::memcpy(allocated_data_bytes, d.data(), d.data_size());
  CHECK(SetPermissions(page_allocator, allocated_data_bytes,
                       allocation_data_size, PageAllocator::kRead));

  *code = allocated_code_bytes;
  *code_size = d.code_size();
  *data = allocated_data_bytes;
  *data_size = d.data_size();

  d.Dispose();
}

// static
void OffHeapInstructionStream::FreeOffHeapOffHeapInstructionStream(
    uint8_t* code, uint32_t code_size, uint8_t* data, uint32_t data_size) {
  v8::PageAllocator* page_allocator = v8::internal::GetPlatformPageAllocator();
  const uint32_t page_size =
      static_cast<uint32_t>(page_allocator->AllocatePageSize());
  FreePages(page_allocator, code, RoundUp(code_size, page_size));
  FreePages(page_allocator, data, RoundUp(data_size, page_size));
}

namespace {

bool BuiltinAliasesOffHeapTrampolineRegister(Isolate* isolate, Code code) {
  DCHECK(Builtins::IsIsolateIndependent(code.builtin_id()));
  switch (Builtins::KindOf(code.builtin_id())) {
    case Builtins::CPP:
    case Builtins::TFC:
    case Builtins::TFH:
    case Builtins::TFJ:
    case Builtins::TFS:
      break;

    // Bytecode handlers will only ever be used by the interpreter and so there
    // will never be a need to use trampolines with them.
    case Builtins::BCH:
    case Builtins::ASM:
      // TODO(jgruber): Extend checks to remaining kinds.
      return false;
  }

  static_assert(CallInterfaceDescriptor::ContextRegister() !=
                kOffHeapTrampolineRegister);

  Callable callable = Builtins::CallableFor(isolate, code.builtin_id());
  CallInterfaceDescriptor descriptor = callable.descriptor();

  for (int i = 0; i < descriptor.GetRegisterParameterCount(); i++) {
    Register reg = descriptor.GetRegisterParameter(i);
    if (reg == kOffHeapTrampolineRegister) return true;
  }

  return false;
}

void FinalizeEmbeddedCodeTargets(Isolate* isolate, EmbeddedData* blob) {
  static const int kRelocMask =
      RelocInfo::ModeMask(RelocInfo::CODE_TARGET) |
      RelocInfo::ModeMask(RelocInfo::RELATIVE_CODE_TARGET);

  static_assert(Builtins::kAllBuiltinsAreIsolateIndependent);
  for (Builtin builtin = Builtins::kFirst; builtin <= Builtins::kLast;
       ++builtin) {
    Code code = FromCodeT(isolate->builtins()->code(builtin));
    RelocIterator on_heap_it(code, kRelocMask);
    RelocIterator off_heap_it(blob, code, kRelocMask);

#if defined(V8_TARGET_ARCH_X64) || defined(V8_TARGET_ARCH_ARM64) ||    \
    defined(V8_TARGET_ARCH_ARM) || defined(V8_TARGET_ARCH_IA32) ||     \
    defined(V8_TARGET_ARCH_S390) || defined(V8_TARGET_ARCH_RISCV64) || \
    defined(V8_TARGET_ARCH_LOONG64) || defined(V8_TARGET_ARCH_RISCV32)
    // On these platforms we emit relative builtin-to-builtin
    // jumps for isolate independent builtins in the snapshot. This fixes up the
    // relative jumps to the right offsets in the snapshot.
    // See also: Code::IsIsolateIndependent.
    while (!on_heap_it.done()) {
      DCHECK(!off_heap_it.done());

      RelocInfo* rinfo = on_heap_it.rinfo();
      DCHECK_EQ(rinfo->rmode(), off_heap_it.rinfo()->rmode());
      Code target = Code::GetCodeFromTargetAddress(rinfo->target_address());
      CHECK(Builtins::IsIsolateIndependentBuiltin(target));

      // Do not emit write-barrier for off-heap writes.
      off_heap_it.rinfo()->set_off_heap_target_address(
          blob->InstructionStartOfBuiltin(target.builtin_id()));

      on_heap_it.next();
      off_heap_it.next();
    }
    DCHECK(off_heap_it.done());
#else
    // Architectures other than x64 and arm/arm64 do not use pc-relative calls
    // and thus must not contain embedded code targets. Instead, we use an
    // indirection through the root register.
    CHECK(on_heap_it.done());
    CHECK(off_heap_it.done());
#endif
  }
}

void EnsureRelocatable(CodeT codet) {
  Code code = FromCodeT(codet);
  if (code.relocation_size() == 0) return;

  // On some architectures (arm) the builtin might have a non-empty reloc
  // info containing a CONST_POOL entry. These entries don't have to be
  // updated when Code object is relocated, so it's safe to drop the reloc
  // info alltogether. If it wasn't the case then we'd have to store it
  // in the metadata.
  for (RelocIterator it(code); !it.done(); it.next()) {
    CHECK_EQ(it.rinfo()->rmode(), RelocInfo::CONST_POOL);
  }
}

}  // namespace

// static
EmbeddedData EmbeddedData::FromIsolate(Isolate* isolate) {
  Builtins* builtins = isolate->builtins();

  // Store instruction stream lengths and offsets.
  std::vector<struct LayoutDescription> layout_descriptions(kTableSize);

  bool saw_unsafe_builtin = false;
  uint32_t raw_code_size = 0;
  uint32_t raw_data_size = 0;
  static_assert(Builtins::kAllBuiltinsAreIsolateIndependent);
  for (Builtin builtin = Builtins::kFirst; builtin <= Builtins::kLast;
       ++builtin) {
    Code code = FromCodeT(builtins->code(builtin));

    // Sanity-check that the given builtin is isolate-independent and does not
    // use the trampoline register in its calling convention.
    if (!code.IsIsolateIndependent(isolate)) {
      saw_unsafe_builtin = true;
      fprintf(stderr, "%s is not isolate-independent.\n",
              Builtins::name(builtin));
    }
    if (BuiltinAliasesOffHeapTrampolineRegister(isolate, code)) {
      saw_unsafe_builtin = true;
      fprintf(stderr, "%s aliases the off-heap trampoline register.\n",
              Builtins::name(builtin));
    }

    uint32_t instruction_size =
        static_cast<uint32_t>(code.raw_instruction_size());
    uint32_t metadata_size = static_cast<uint32_t>(code.raw_metadata_size());

    DCHECK_EQ(0, raw_code_size % kCodeAlignment);
    {
      const int builtin_index = static_cast<int>(builtin);
      struct LayoutDescription& layout_desc =
          layout_descriptions[builtin_index];
      layout_desc.instruction_offset = raw_code_size;
      layout_desc.instruction_length = instruction_size;
      layout_desc.metadata_offset = raw_data_size;
      layout_desc.metadata_length = metadata_size;

      layout_desc.handler_table_offset =
          raw_data_size + static_cast<uint32_t>(code.handler_table_offset());
#if V8_EMBEDDED_CONSTANT_POOL_BOOL
      layout_desc.constant_pool_offset =
          raw_data_size + static_cast<uint32_t>(code.constant_pool_offset());
#endif
      layout_desc.code_comments_offset_offset =
          raw_data_size + static_cast<uint32_t>(code.code_comments_offset());
      layout_desc.unwinding_info_offset_offset =
          raw_data_size + static_cast<uint32_t>(code.unwinding_info_offset());
      layout_desc.stack_slots = static_cast<uint32_t>(code.stack_slots());

      CHECK_EQ(code.deoptimization_data().length(), 0);
    }
    // Align the start of each section.
    raw_code_size += PadAndAlignCode(instruction_size);
    raw_data_size += PadAndAlignData(metadata_size);
  }
  CHECK_WITH_MSG(
      !saw_unsafe_builtin,
      "One or more builtins marked as isolate-independent either contains "
      "isolate-dependent code or aliases the off-heap trampoline register. "
      "If in doubt, ask jgruber@");

  // Allocate space for the code section, value-initialized to 0.
  static_assert(RawCodeOffset() == 0);
  const uint32_t blob_code_size = RawCodeOffset() + raw_code_size;
  uint8_t* const blob_code = new uint8_t[blob_code_size]();

  // Allocate space for the data section, value-initialized to 0.
  static_assert(IsAligned(FixedDataSize(), Code::kMetadataAlignment));
  const uint32_t blob_data_size = FixedDataSize() + raw_data_size;
  uint8_t* const blob_data = new uint8_t[blob_data_size]();

  // Initially zap the entire blob, effectively padding the alignment area
  // between two builtins with int3's (on x64/ia32).
  ZapCode(reinterpret_cast<Address>(blob_code), blob_code_size);

  // Hash relevant parts of the Isolate's heap and store the result.
  {
    static_assert(IsolateHashSize() == kSizetSize);
    const size_t hash = isolate->HashIsolateForEmbeddedBlob();
    std::memcpy(blob_data + IsolateHashOffset(), &hash, IsolateHashSize());
  }

  // Write the layout_descriptions tables.
  DCHECK_EQ(LayoutDescriptionTableSize(),
            sizeof(layout_descriptions[0]) * layout_descriptions.size());
  std::memcpy(blob_data + LayoutDescriptionTableOffset(),
              layout_descriptions.data(), LayoutDescriptionTableSize());

  // .. and the variable-size data section.
  uint8_t* const raw_metadata_start = blob_data + RawMetadataOffset();
  static_assert(Builtins::kAllBuiltinsAreIsolateIndependent);
  for (Builtin builtin = Builtins::kFirst; builtin <= Builtins::kLast;
       ++builtin) {
    Code code = FromCodeT(builtins->code(builtin));
    uint32_t offset =
        layout_descriptions[static_cast<int>(builtin)].metadata_offset;
    uint8_t* dst = raw_metadata_start + offset;
    DCHECK_LE(RawMetadataOffset() + offset + code.raw_metadata_size(),
              blob_data_size);
    std::memcpy(dst, reinterpret_cast<uint8_t*>(code.raw_metadata_start()),
                code.raw_metadata_size());
  }
  CHECK_IMPLIES(
      kMaxPCRelativeCodeRangeInMB,
      static_cast<size_t>(raw_code_size) <= kMaxPCRelativeCodeRangeInMB * MB);

  // .. and the variable-size code section.
  uint8_t* const raw_code_start = blob_code + RawCodeOffset();
  static_assert(Builtins::kAllBuiltinsAreIsolateIndependent);
  for (Builtin builtin = Builtins::kFirst; builtin <= Builtins::kLast;
       ++builtin) {
    Code code = FromCodeT(builtins->code(builtin));
    uint32_t offset =
        layout_descriptions[static_cast<int>(builtin)].instruction_offset;
    uint8_t* dst = raw_code_start + offset;
    DCHECK_LE(RawCodeOffset() + offset + code.raw_instruction_size(),
              blob_code_size);
    std::memcpy(dst, reinterpret_cast<uint8_t*>(code.raw_instruction_start()),
                code.raw_instruction_size());
  }

  EmbeddedData d(blob_code, blob_code_size, blob_data, blob_data_size);

  // Fix up call targets that point to other embedded builtins.
  FinalizeEmbeddedCodeTargets(isolate, &d);

  // Hash the blob and store the result.
  {
    static_assert(EmbeddedBlobDataHashSize() == kSizetSize);
    const size_t data_hash = d.CreateEmbeddedBlobDataHash();
    std::memcpy(blob_data + EmbeddedBlobDataHashOffset(), &data_hash,
                EmbeddedBlobDataHashSize());

    static_assert(EmbeddedBlobCodeHashSize() == kSizetSize);
    const size_t code_hash = d.CreateEmbeddedBlobCodeHash();
    std::memcpy(blob_data + EmbeddedBlobCodeHashOffset(), &code_hash,
                EmbeddedBlobCodeHashSize());

    DCHECK_EQ(data_hash, d.CreateEmbeddedBlobDataHash());
    DCHECK_EQ(data_hash, d.EmbeddedBlobDataHash());
    DCHECK_EQ(code_hash, d.CreateEmbeddedBlobCodeHash());
    DCHECK_EQ(code_hash, d.EmbeddedBlobCodeHash());
  }

  if (DEBUG_BOOL) {
    for (Builtin builtin = Builtins::kFirst; builtin <= Builtins::kLast;
         ++builtin) {
      Code code = FromCodeT(builtins->code(builtin));

      CHECK_EQ(d.InstructionSizeOfBuiltin(builtin), code.InstructionSize());
      CHECK_EQ(d.MetadataSizeOfBuiltin(builtin), code.MetadataSize());

      CHECK_EQ(d.SafepointTableSizeOf(builtin), code.safepoint_table_size());
      CHECK_EQ(d.HandlerTableSizeOf(builtin), code.handler_table_size());
      CHECK_EQ(d.ConstantPoolSizeOf(builtin), code.constant_pool_size());
      CHECK_EQ(d.CodeCommentsSizeOf(builtin), code.code_comments_size());
      CHECK_EQ(d.UnwindingInfoSizeOf(builtin), code.unwinding_info_size());
      CHECK_EQ(d.StackSlotsOf(builtin), code.stack_slots());
    }
  }
  // Ensure that InterpreterEntryTrampolineForProfiling is relocatable.
  // See v8_flags.interpreted_frames_native_stack for details.
  EnsureRelocatable(
      builtins->code(Builtin::kInterpreterEntryTrampolineForProfiling));

  if (v8_flags.serialization_statistics) d.PrintStatistics();

  return d;
}

size_t EmbeddedData::CreateEmbeddedBlobDataHash() const {
  static_assert(EmbeddedBlobDataHashOffset() == 0);
  static_assert(EmbeddedBlobCodeHashOffset() == EmbeddedBlobDataHashSize());
  static_assert(IsolateHashOffset() ==
                EmbeddedBlobCodeHashOffset() + EmbeddedBlobCodeHashSize());
  static constexpr uint32_t kFirstHashedDataOffset = IsolateHashOffset();
  // Hash the entire data section except the embedded blob hash fields
  // themselves.
  base::Vector<const byte> payload(data_ + kFirstHashedDataOffset,
                                   data_size_ - kFirstHashedDataOffset);
  return Checksum(payload);
}

size_t EmbeddedData::CreateEmbeddedBlobCodeHash() const {
  CHECK(v8_flags.text_is_readable);
  base::Vector<const byte> payload(code_, code_size_);
  return Checksum(payload);
}

void EmbeddedData::PrintStatistics() const {
  DCHECK(v8_flags.serialization_statistics);

  constexpr int kCount = Builtins::kBuiltinCount;
  int sizes[kCount];
  static_assert(Builtins::kAllBuiltinsAreIsolateIndependent);
  for (int i = 0; i < kCount; i++) {
    sizes[i] = InstructionSizeOfBuiltin(Builtins::FromInt(i));
  }

  // Sort for percentiles.
  std::sort(&sizes[0], &sizes[kCount]);

  const int k50th = kCount * 0.5;
  const int k75th = kCount * 0.75;
  const int k90th = kCount * 0.90;
  const int k99th = kCount * 0.99;

  PrintF("EmbeddedData:\n");
  PrintF("  Total size:                  %d\n",
         static_cast<int>(code_size() + data_size()));
  PrintF("  Data size:                   %d\n", static_cast<int>(data_size()));
  PrintF("  Code size:                   %d\n", static_cast<int>(code_size()));
  PrintF("  Instruction size (50th percentile): %d\n", sizes[k50th]);
  PrintF("  Instruction size (75th percentile): %d\n", sizes[k75th]);
  PrintF("  Instruction size (90th percentile): %d\n", sizes[k90th]);
  PrintF("  Instruction size (99th percentile): %d\n", sizes[k99th]);
  PrintF("\n");
}

}  // namespace internal
}  // namespace v8
