// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/embedded/embedded-data.h"

#include "src/codegen/assembler-inl.h"
#include "src/codegen/callable.h"
#include "src/objects/objects-inl.h"
#include "src/snapshot/snapshot.h"

namespace v8 {
namespace internal {

// static
bool InstructionStream::PcIsOffHeap(Isolate* isolate, Address pc) {
  if (FLAG_embedded_builtins) {
    const Address start = reinterpret_cast<Address>(isolate->embedded_blob());
    return start <= pc && pc < start + isolate->embedded_blob_size();
  } else {
    return false;
  }
}

// static
Code InstructionStream::TryLookupCode(Isolate* isolate, Address address) {
  if (!PcIsOffHeap(isolate, address)) return Code();

  EmbeddedData d = EmbeddedData::FromBlob();
  if (address < d.InstructionStartOfBuiltin(0)) return Code();

  // Note: Addresses within the padding section between builtins (i.e. within
  // start + size <= address < start + padded_size) are interpreted as belonging
  // to the preceding builtin.

  int l = 0, r = Builtins::builtin_count;
  while (l < r) {
    const int mid = (l + r) / 2;
    Address start = d.InstructionStartOfBuiltin(mid);
    Address end = start + d.PaddedInstructionSizeOfBuiltin(mid);

    if (address < start) {
      r = mid;
    } else if (address >= end) {
      l = mid + 1;
    } else {
      return isolate->builtins()->builtin(mid);
    }
  }

  UNREACHABLE();
}

// static
void InstructionStream::CreateOffHeapInstructionStream(Isolate* isolate,
                                                       uint8_t** data,
                                                       uint32_t* size) {
  EmbeddedData d = EmbeddedData::FromIsolate(isolate);

  v8::PageAllocator* page_allocator = v8::internal::GetPlatformPageAllocator();
  const uint32_t page_size =
      static_cast<uint32_t>(page_allocator->AllocatePageSize());
  const uint32_t allocated_size = RoundUp(d.size(), page_size);

  uint8_t* allocated_bytes = static_cast<uint8_t*>(
      AllocatePages(page_allocator, isolate->heap()->GetRandomMmapAddr(),
                    allocated_size, page_size, PageAllocator::kReadWrite));
  CHECK_NOT_NULL(allocated_bytes);

  std::memcpy(allocated_bytes, d.data(), d.size());
  CHECK(SetPermissions(page_allocator, allocated_bytes, allocated_size,
                       PageAllocator::kReadExecute));

  *data = allocated_bytes;
  *size = d.size();

  d.Dispose();
}

// static
void InstructionStream::FreeOffHeapInstructionStream(uint8_t* data,
                                                     uint32_t size) {
  v8::PageAllocator* page_allocator = v8::internal::GetPlatformPageAllocator();
  const uint32_t page_size =
      static_cast<uint32_t>(page_allocator->AllocatePageSize());
  CHECK(FreePages(page_allocator, data, RoundUp(size, page_size)));
}

namespace {

bool BuiltinAliasesOffHeapTrampolineRegister(Isolate* isolate, Code code) {
  DCHECK(Builtins::IsIsolateIndependent(code.builtin_index()));
  switch (Builtins::KindOf(code.builtin_index())) {
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

  Callable callable = Builtins::CallableFor(
      isolate, static_cast<Builtins::Name>(code.builtin_index()));
  CallInterfaceDescriptor descriptor = callable.descriptor();

  if (descriptor.ContextRegister() == kOffHeapTrampolineRegister) {
    return true;
  }

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

  for (int i = 0; i < Builtins::builtin_count; i++) {
    if (!Builtins::IsIsolateIndependent(i)) continue;

    Code code = isolate->builtins()->builtin(i);
    RelocIterator on_heap_it(code, kRelocMask);
    RelocIterator off_heap_it(blob, code, kRelocMask);

#if defined(V8_TARGET_ARCH_X64) || defined(V8_TARGET_ARCH_ARM64) || \
    defined(V8_TARGET_ARCH_ARM) || defined(V8_TARGET_ARCH_MIPS) ||  \
    defined(V8_TARGET_ARCH_IA32) || defined(V8_TARGET_ARCH_S390)
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
      off_heap_it.rinfo()->set_target_address(
          blob->InstructionStartOfBuiltin(target.builtin_index()),
          SKIP_WRITE_BARRIER);

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
#endif  // defined(V8_TARGET_ARCH_X64) || defined(V8_TARGET_ARCH_ARM64)
  }
}

}  // namespace

// static
EmbeddedData EmbeddedData::FromIsolate(Isolate* isolate) {
  Builtins* builtins = isolate->builtins();

  // Store instruction stream lengths and offsets.
  std::vector<struct Metadata> metadata(kTableSize);

  bool saw_unsafe_builtin = false;
  uint32_t raw_data_size = 0;
  for (int i = 0; i < Builtins::builtin_count; i++) {
    Code code = builtins->builtin(i);

    if (Builtins::IsIsolateIndependent(i)) {
      // Sanity-check that the given builtin is isolate-independent and does not
      // use the trampoline register in its calling convention.
      if (!code.IsIsolateIndependent(isolate)) {
        saw_unsafe_builtin = true;
        fprintf(stderr, "%s is not isolate-independent.\n", Builtins::name(i));
      }
      if (Builtins::IsWasmRuntimeStub(i) &&
          RelocInfo::RequiresRelocation(code)) {
        // Wasm additionally requires that its runtime stubs must be
        // individually PIC (i.e. we must be able to copy each stub outside the
        // embedded area without relocations). In particular, that means
        // pc-relative calls to other builtins are disallowed.
        saw_unsafe_builtin = true;
        fprintf(stderr, "%s is a wasm runtime stub but needs relocation.\n",
                Builtins::name(i));
      }
      if (BuiltinAliasesOffHeapTrampolineRegister(isolate, code)) {
        saw_unsafe_builtin = true;
        fprintf(stderr, "%s aliases the off-heap trampoline register.\n",
                Builtins::name(i));
      }

      uint32_t length = static_cast<uint32_t>(code.raw_instruction_size());

      DCHECK_EQ(0, raw_data_size % kCodeAlignment);
      metadata[i].instructions_offset = raw_data_size;
      metadata[i].instructions_length = length;

      // Align the start of each instruction stream.
      raw_data_size += PadAndAlign(length);
    } else {
      metadata[i].instructions_offset = raw_data_size;
    }
  }
  CHECK_WITH_MSG(
      !saw_unsafe_builtin,
      "One or more builtins marked as isolate-independent either contains "
      "isolate-dependent code or aliases the off-heap trampoline register. "
      "If in doubt, ask jgruber@");

  const uint32_t blob_size = RawDataOffset() + raw_data_size;
  uint8_t* const blob = new uint8_t[blob_size];
  uint8_t* const raw_data_start = blob + RawDataOffset();

  // Initially zap the entire blob, effectively padding the alignment area
  // between two builtins with int3's (on x64/ia32).
  ZapCode(reinterpret_cast<Address>(blob), blob_size);

  // Hash relevant parts of the Isolate's heap and store the result.
  {
    STATIC_ASSERT(IsolateHashSize() == kSizetSize);
    const size_t hash = isolate->HashIsolateForEmbeddedBlob();
    std::memcpy(blob + IsolateHashOffset(), &hash, IsolateHashSize());
  }

  // Write the metadata tables.
  DCHECK_EQ(MetadataSize(), sizeof(metadata[0]) * metadata.size());
  std::memcpy(blob + MetadataOffset(), metadata.data(), MetadataSize());

  // Write the raw data section.
  for (int i = 0; i < Builtins::builtin_count; i++) {
    if (!Builtins::IsIsolateIndependent(i)) continue;
    Code code = builtins->builtin(i);
    uint32_t offset = metadata[i].instructions_offset;
    uint8_t* dst = raw_data_start + offset;
    DCHECK_LE(RawDataOffset() + offset + code.raw_instruction_size(),
              blob_size);
    std::memcpy(dst, reinterpret_cast<uint8_t*>(code.raw_instruction_start()),
                code.raw_instruction_size());
  }

  EmbeddedData d(blob, blob_size);

  // Fix up call targets that point to other embedded builtins.
  FinalizeEmbeddedCodeTargets(isolate, &d);

  // Hash the blob and store the result.
  {
    STATIC_ASSERT(EmbeddedBlobHashSize() == kSizetSize);
    const size_t hash = d.CreateEmbeddedBlobHash();
    std::memcpy(blob + EmbeddedBlobHashOffset(), &hash, EmbeddedBlobHashSize());

    DCHECK_EQ(hash, d.CreateEmbeddedBlobHash());
    DCHECK_EQ(hash, d.EmbeddedBlobHash());
  }

  if (FLAG_serialization_statistics) d.PrintStatistics();

  return d;
}

Address EmbeddedData::InstructionStartOfBuiltin(int i) const {
  DCHECK(Builtins::IsBuiltinId(i));
  const struct Metadata* metadata = Metadata();
  const uint8_t* result = RawData() + metadata[i].instructions_offset;
  DCHECK_LE(result, data_ + size_);
  DCHECK_IMPLIES(result == data_ + size_, InstructionSizeOfBuiltin(i) == 0);
  return reinterpret_cast<Address>(result);
}

uint32_t EmbeddedData::InstructionSizeOfBuiltin(int i) const {
  DCHECK(Builtins::IsBuiltinId(i));
  const struct Metadata* metadata = Metadata();
  return metadata[i].instructions_length;
}

Address EmbeddedData::InstructionStartOfBytecodeHandlers() const {
  return InstructionStartOfBuiltin(Builtins::kFirstBytecodeHandler);
}

Address EmbeddedData::InstructionEndOfBytecodeHandlers() const {
  STATIC_ASSERT(Builtins::kFirstBytecodeHandler + kNumberOfBytecodeHandlers +
                    2 * kNumberOfWideBytecodeHandlers ==
                Builtins::builtin_count);
  int lastBytecodeHandler = Builtins::builtin_count - 1;
  return InstructionStartOfBuiltin(lastBytecodeHandler) +
         InstructionSizeOfBuiltin(lastBytecodeHandler);
}

size_t EmbeddedData::CreateEmbeddedBlobHash() const {
  STATIC_ASSERT(EmbeddedBlobHashOffset() == 0);
  STATIC_ASSERT(EmbeddedBlobHashSize() == kSizetSize);
  return base::hash_range(data_ + EmbeddedBlobHashSize(), data_ + size_);
}

void EmbeddedData::PrintStatistics() const {
  DCHECK(FLAG_serialization_statistics);

  constexpr int kCount = Builtins::builtin_count;

  int embedded_count = 0;
  int instruction_size = 0;
  int sizes[kCount];
  for (int i = 0; i < kCount; i++) {
    if (!Builtins::IsIsolateIndependent(i)) continue;
    const int size = InstructionSizeOfBuiltin(i);
    instruction_size += size;
    sizes[embedded_count] = size;
    embedded_count++;
  }

  // Sort for percentiles.
  std::sort(&sizes[0], &sizes[embedded_count]);

  const int k50th = embedded_count * 0.5;
  const int k75th = embedded_count * 0.75;
  const int k90th = embedded_count * 0.90;
  const int k99th = embedded_count * 0.99;

  const int metadata_size = static_cast<int>(
      EmbeddedBlobHashSize() + IsolateHashSize() + MetadataSize());

  PrintF("EmbeddedData:\n");
  PrintF("  Total size:                         %d\n",
         static_cast<int>(size()));
  PrintF("  Metadata size:                      %d\n", metadata_size);
  PrintF("  Instruction size:                   %d\n", instruction_size);
  PrintF("  Padding:                            %d\n",
         static_cast<int>(size() - metadata_size - instruction_size));
  PrintF("  Embedded builtin count:             %d\n", embedded_count);
  PrintF("  Instruction size (50th percentile): %d\n", sizes[k50th]);
  PrintF("  Instruction size (75th percentile): %d\n", sizes[k75th]);
  PrintF("  Instruction size (90th percentile): %d\n", sizes[k90th]);
  PrintF("  Instruction size (99th percentile): %d\n", sizes[k99th]);
  PrintF("\n");
}

}  // namespace internal
}  // namespace v8
