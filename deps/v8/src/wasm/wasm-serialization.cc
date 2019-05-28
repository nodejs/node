// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-serialization.h"

#include "src/assembler-inl.h"
#include "src/external-reference-table.h"
#include "src/objects-inl.h"
#include "src/objects.h"
#include "src/ostreams.h"
#include "src/runtime/runtime.h"
#include "src/snapshot/code-serializer.h"
#include "src/snapshot/serializer-common.h"
#include "src/utils.h"
#include "src/version.h"
#include "src/wasm/function-compiler.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-result.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace {

// TODO(bbudge) Try to unify the various implementations of readers and writers
// in WASM, e.g. StreamProcessor and ZoneBuffer, with these.
class Writer {
 public:
  explicit Writer(Vector<byte> buffer)
      : start_(buffer.start()), end_(buffer.end()), pos_(buffer.start()) {}

  size_t bytes_written() const { return pos_ - start_; }
  byte* current_location() const { return pos_; }
  size_t current_size() const { return end_ - pos_; }
  Vector<byte> current_buffer() const {
    return {current_location(), current_size()};
  }

  template <typename T>
  void Write(const T& value) {
    DCHECK_GE(current_size(), sizeof(T));
    WriteUnalignedValue(reinterpret_cast<Address>(current_location()), value);
    pos_ += sizeof(T);
    if (FLAG_trace_wasm_serialization) {
      StdoutStream{} << "wrote: " << static_cast<size_t>(value)
                     << " sized: " << sizeof(T) << std::endl;
    }
  }

  void WriteVector(const Vector<const byte> v) {
    DCHECK_GE(current_size(), v.size());
    if (v.size() > 0) {
      memcpy(current_location(), v.start(), v.size());
      pos_ += v.size();
    }
    if (FLAG_trace_wasm_serialization) {
      StdoutStream{} << "wrote vector of " << v.size() << " elements"
                     << std::endl;
    }
  }

  void Skip(size_t size) { pos_ += size; }

 private:
  byte* const start_;
  byte* const end_;
  byte* pos_;
};

class Reader {
 public:
  explicit Reader(Vector<const byte> buffer)
      : start_(buffer.start()), end_(buffer.end()), pos_(buffer.start()) {}

  size_t bytes_read() const { return pos_ - start_; }
  const byte* current_location() const { return pos_; }
  size_t current_size() const { return end_ - pos_; }
  Vector<const byte> current_buffer() const {
    return {current_location(), current_size()};
  }

  template <typename T>
  T Read() {
    DCHECK_GE(current_size(), sizeof(T));
    T value =
        ReadUnalignedValue<T>(reinterpret_cast<Address>(current_location()));
    pos_ += sizeof(T);
    if (FLAG_trace_wasm_serialization) {
      StdoutStream{} << "read: " << static_cast<size_t>(value)
                     << " sized: " << sizeof(T) << std::endl;
    }
    return value;
  }

  void ReadVector(Vector<byte> v) {
    if (v.size() > 0) {
      DCHECK_GE(current_size(), v.size());
      memcpy(v.start(), current_location(), v.size());
      pos_ += v.size();
    }
    if (FLAG_trace_wasm_serialization) {
      StdoutStream{} << "read vector of " << v.size() << " elements"
                     << std::endl;
    }
  }

  void Skip(size_t size) { pos_ += size; }

 private:
  const byte* const start_;
  const byte* const end_;
  const byte* pos_;
};

constexpr size_t kVersionSize = 4 * sizeof(uint32_t);

void WriteVersion(Writer* writer) {
  writer->Write(SerializedData::kMagicNumber);
  writer->Write(Version::Hash());
  writer->Write(static_cast<uint32_t>(CpuFeatures::SupportedFeatures()));
  writer->Write(FlagList::Hash());
}

// On Intel, call sites are encoded as a displacement. For linking and for
// serialization/deserialization, we want to store/retrieve a tag (the function
// index). On Intel, that means accessing the raw displacement.
// On ARM64, call sites are encoded as either a literal load or a direct branch.
// Other platforms simply require accessing the target address.
void SetWasmCalleeTag(RelocInfo* rinfo, uint32_t tag) {
#if V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_IA32
  DCHECK(rinfo->HasTargetAddressAddress());
  WriteUnalignedValue(rinfo->target_address_address(), tag);
#elif V8_TARGET_ARCH_ARM64
  Instruction* instr = reinterpret_cast<Instruction*>(rinfo->pc());
  if (instr->IsLdrLiteralX()) {
    WriteUnalignedValue(rinfo->constant_pool_entry_address(),
                        static_cast<Address>(tag));
  } else {
    DCHECK(instr->IsBranchAndLink() || instr->IsUnconditionalBranch());
    instr->SetBranchImmTarget(
        reinterpret_cast<Instruction*>(rinfo->pc() + tag * kInstrSize));
  }
#else
  Address addr = static_cast<Address>(tag);
  if (rinfo->rmode() == RelocInfo::EXTERNAL_REFERENCE) {
    rinfo->set_target_external_reference(addr, SKIP_ICACHE_FLUSH);
  } else if (rinfo->rmode() == RelocInfo::WASM_STUB_CALL) {
    rinfo->set_wasm_stub_call_address(addr, SKIP_ICACHE_FLUSH);
  } else {
    rinfo->set_target_address(addr, SKIP_WRITE_BARRIER, SKIP_ICACHE_FLUSH);
  }
#endif
}

uint32_t GetWasmCalleeTag(RelocInfo* rinfo) {
#if V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_IA32
  return ReadUnalignedValue<uint32_t>(rinfo->target_address_address());
#elif V8_TARGET_ARCH_ARM64
  Instruction* instr = reinterpret_cast<Instruction*>(rinfo->pc());
  if (instr->IsLdrLiteralX()) {
    return ReadUnalignedValue<uint32_t>(rinfo->constant_pool_entry_address());
  } else {
    DCHECK(instr->IsBranchAndLink() || instr->IsUnconditionalBranch());
    return static_cast<uint32_t>(instr->ImmPCOffset() / kInstrSize);
  }
#else
  Address addr;
  if (rinfo->rmode() == RelocInfo::EXTERNAL_REFERENCE) {
    addr = rinfo->target_external_reference();
  } else if (rinfo->rmode() == RelocInfo::WASM_STUB_CALL) {
    addr = rinfo->wasm_stub_call_address();
  } else {
    addr = rinfo->target_address();
  }
  return static_cast<uint32_t>(addr);
#endif
}

constexpr size_t kHeaderSize =
    sizeof(uint32_t) +  // total wasm function count
    sizeof(uint32_t);   // imported functions (index of first wasm function)

constexpr size_t kCodeHeaderSize =
    sizeof(size_t) +          // size of code section
    sizeof(size_t) +          // offset of constant pool
    sizeof(size_t) +          // offset of safepoint table
    sizeof(size_t) +          // offset of handler table
    sizeof(size_t) +          // offset of code comments
    sizeof(size_t) +          // unpadded binary size
    sizeof(uint32_t) +        // stack slots
    sizeof(uint32_t) +        // tagged parameter slots
    sizeof(size_t) +          // code size
    sizeof(size_t) +          // reloc size
    sizeof(size_t) +          // source positions size
    sizeof(size_t) +          // protected instructions size
    sizeof(WasmCode::Kind) +  // code kind
    sizeof(ExecutionTier);    // tier

// A List of all isolate-independent external references. This is used to create
// a tag from the Address of an external reference and vice versa.
class ExternalReferenceList {
 public:
  uint32_t tag_from_address(Address ext_ref_address) const {
    auto tag_addr_less_than = [this](uint32_t tag, Address searched_addr) {
      return external_reference_by_tag_[tag] < searched_addr;
    };
    auto it = std::lower_bound(std::begin(tags_ordered_by_address_),
                               std::end(tags_ordered_by_address_),
                               ext_ref_address, tag_addr_less_than);
    DCHECK_NE(std::end(tags_ordered_by_address_), it);
    uint32_t tag = *it;
    DCHECK_EQ(address_from_tag(tag), ext_ref_address);
    return tag;
  }

  Address address_from_tag(uint32_t tag) const {
    DCHECK_GT(kNumExternalReferences, tag);
    return external_reference_by_tag_[tag];
  }

  static const ExternalReferenceList& Get() {
    static ExternalReferenceList list;  // Lazily initialized.
    return list;
  }

 private:
  // Private constructor. There will only be a single instance of this object.
  ExternalReferenceList() {
    for (uint32_t i = 0; i < kNumExternalReferences; ++i) {
      tags_ordered_by_address_[i] = i;
    }
    auto addr_by_tag_less_than = [this](uint32_t a, uint32_t b) {
      return external_reference_by_tag_[a] < external_reference_by_tag_[b];
    };
    std::sort(std::begin(tags_ordered_by_address_),
              std::end(tags_ordered_by_address_), addr_by_tag_less_than);
  }

#define COUNT_EXTERNAL_REFERENCE(name, ...) +1
  static constexpr uint32_t kNumExternalReferencesList =
      EXTERNAL_REFERENCE_LIST(COUNT_EXTERNAL_REFERENCE);
  static constexpr uint32_t kNumExternalReferencesIntrinsics =
      FOR_EACH_INTRINSIC(COUNT_EXTERNAL_REFERENCE);
  static constexpr uint32_t kNumExternalReferences =
      kNumExternalReferencesList + kNumExternalReferencesIntrinsics;
#undef COUNT_EXTERNAL_REFERENCE

  Address external_reference_by_tag_[kNumExternalReferences] = {
#define EXT_REF_ADDR(name, desc) ExternalReference::name().address(),
      EXTERNAL_REFERENCE_LIST(EXT_REF_ADDR)
#undef EXT_REF_ADDR
#define RUNTIME_ADDR(name, ...) \
  ExternalReference::Create(Runtime::k##name).address(),
          FOR_EACH_INTRINSIC(RUNTIME_ADDR)
#undef RUNTIME_ADDR
  };
  uint32_t tags_ordered_by_address_[kNumExternalReferences];
  DISALLOW_COPY_AND_ASSIGN(ExternalReferenceList);
};

static_assert(std::is_trivially_destructible<ExternalReferenceList>::value,
              "static destructors not allowed");

}  // namespace

class V8_EXPORT_PRIVATE NativeModuleSerializer {
 public:
  NativeModuleSerializer() = delete;
  NativeModuleSerializer(const NativeModule*, Vector<WasmCode* const>);

  size_t Measure() const;
  bool Write(Writer* writer);

 private:
  size_t MeasureCode(const WasmCode*) const;
  void WriteHeader(Writer* writer);
  void WriteCode(const WasmCode*, Writer* writer);

  const NativeModule* const native_module_;
  Vector<WasmCode* const> code_table_;
  bool write_called_;

  // Reverse lookup tables for embedded addresses.
  std::map<Address, uint32_t> wasm_stub_targets_lookup_;

  DISALLOW_COPY_AND_ASSIGN(NativeModuleSerializer);
};

NativeModuleSerializer::NativeModuleSerializer(
    const NativeModule* module, Vector<WasmCode* const> code_table)
    : native_module_(module), code_table_(code_table), write_called_(false) {
  DCHECK_NOT_NULL(native_module_);
  // TODO(mtrofin): persist the export wrappers. Ideally, we'd only persist
  // the unique ones, i.e. the cache.
  for (uint32_t i = 0; i < WasmCode::kRuntimeStubCount; ++i) {
    Address addr = native_module_->runtime_stub_entry(
        static_cast<WasmCode::RuntimeStubId>(i));
    wasm_stub_targets_lookup_.insert(std::make_pair(addr, i));
  }
}

size_t NativeModuleSerializer::MeasureCode(const WasmCode* code) const {
  if (code == nullptr) return sizeof(size_t);
  DCHECK(code->kind() == WasmCode::kFunction ||
         code->kind() == WasmCode::kInterpreterEntry);
  return kCodeHeaderSize + code->instructions().size() +
         code->reloc_info().size() + code->source_positions().size() +
         code->protected_instructions().size() *
             sizeof(trap_handler::ProtectedInstructionData);
}

size_t NativeModuleSerializer::Measure() const {
  size_t size = kHeaderSize;
  for (WasmCode* code : code_table_) {
    size += MeasureCode(code);
  }
  return size;
}

void NativeModuleSerializer::WriteHeader(Writer* writer) {
  // TODO(eholk): We need to properly preserve the flag whether the trap
  // handler was used or not when serializing.

  writer->Write(native_module_->num_functions());
  writer->Write(native_module_->num_imported_functions());
}

void NativeModuleSerializer::WriteCode(const WasmCode* code, Writer* writer) {
  if (code == nullptr) {
    writer->Write(size_t{0});
    return;
  }
  DCHECK(code->kind() == WasmCode::kFunction ||
         code->kind() == WasmCode::kInterpreterEntry);
  // Write the size of the entire code section, followed by the code header.
  writer->Write(MeasureCode(code));
  writer->Write(code->constant_pool_offset());
  writer->Write(code->safepoint_table_offset());
  writer->Write(code->handler_table_offset());
  writer->Write(code->code_comments_offset());
  writer->Write(code->unpadded_binary_size());
  writer->Write(code->stack_slots());
  writer->Write(code->tagged_parameter_slots());
  writer->Write(code->instructions().size());
  writer->Write(code->reloc_info().size());
  writer->Write(code->source_positions().size());
  writer->Write(code->protected_instructions().size());
  writer->Write(code->kind());
  writer->Write(code->tier());

  // Get a pointer to the destination buffer, to hold relocated code.
  byte* serialized_code_start = writer->current_buffer().start();
  byte* code_start = serialized_code_start;
  size_t code_size = code->instructions().size();
  writer->Skip(code_size);
  // Write the reloc info, source positions, and protected code.
  writer->WriteVector(code->reloc_info());
  writer->WriteVector(code->source_positions());
  writer->WriteVector(Vector<byte>::cast(code->protected_instructions()));
#if V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_ARM || \
    V8_TARGET_ARCH_PPC
  // On platforms that don't support misaligned word stores, copy to an aligned
  // buffer if necessary so we can relocate the serialized code.
  std::unique_ptr<byte[]> aligned_buffer;
  if (!IsAligned(reinterpret_cast<Address>(serialized_code_start),
                 kInt32Size)) {
    aligned_buffer.reset(new byte[code_size]);
    code_start = aligned_buffer.get();
  }
#endif
  memcpy(code_start, code->instructions().start(), code_size);
  // Relocate the code.
  int mask = RelocInfo::ModeMask(RelocInfo::WASM_CALL) |
             RelocInfo::ModeMask(RelocInfo::WASM_STUB_CALL) |
             RelocInfo::ModeMask(RelocInfo::EXTERNAL_REFERENCE) |
             RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE) |
             RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE_ENCODED);
  RelocIterator orig_iter(code->instructions(), code->reloc_info(),
                          code->constant_pool(), mask);
  for (RelocIterator iter(
           {code_start, code->instructions().size()}, code->reloc_info(),
           reinterpret_cast<Address>(code_start) + code->constant_pool_offset(),
           mask);
       !iter.done(); iter.next(), orig_iter.next()) {
    RelocInfo::Mode mode = orig_iter.rinfo()->rmode();
    switch (mode) {
      case RelocInfo::WASM_CALL: {
        Address orig_target = orig_iter.rinfo()->wasm_call_address();
        uint32_t tag =
            native_module_->GetFunctionIndexFromJumpTableSlot(orig_target);
        SetWasmCalleeTag(iter.rinfo(), tag);
      } break;
      case RelocInfo::WASM_STUB_CALL: {
        Address orig_target = orig_iter.rinfo()->wasm_stub_call_address();
        auto stub_iter = wasm_stub_targets_lookup_.find(orig_target);
        DCHECK(stub_iter != wasm_stub_targets_lookup_.end());
        uint32_t tag = stub_iter->second;
        SetWasmCalleeTag(iter.rinfo(), tag);
      } break;
      case RelocInfo::EXTERNAL_REFERENCE: {
        Address orig_target = orig_iter.rinfo()->target_external_reference();
        uint32_t ext_ref_tag =
            ExternalReferenceList::Get().tag_from_address(orig_target);
        SetWasmCalleeTag(iter.rinfo(), ext_ref_tag);
      } break;
      case RelocInfo::INTERNAL_REFERENCE:
      case RelocInfo::INTERNAL_REFERENCE_ENCODED: {
        Address orig_target = orig_iter.rinfo()->target_internal_reference();
        Address offset = orig_target - code->instruction_start();
        Assembler::deserialization_set_target_internal_reference_at(
            iter.rinfo()->pc(), offset, mode);
      } break;
      default:
        UNREACHABLE();
    }
  }
  // If we copied to an aligned buffer, copy code into serialized buffer.
  if (code_start != serialized_code_start) {
    memcpy(serialized_code_start, code_start, code_size);
  }
}

bool NativeModuleSerializer::Write(Writer* writer) {
  DCHECK(!write_called_);
  write_called_ = true;

  WriteHeader(writer);

  for (WasmCode* code : code_table_) {
    WriteCode(code, writer);
  }
  return true;
}

WasmSerializer::WasmSerializer(NativeModule* native_module)
    : native_module_(native_module),
      code_table_(native_module->SnapshotCodeTable()) {}

size_t WasmSerializer::GetSerializedNativeModuleSize() const {
  NativeModuleSerializer serializer(native_module_, VectorOf(code_table_));
  return kVersionSize + serializer.Measure();
}

bool WasmSerializer::SerializeNativeModule(Vector<byte> buffer) const {
  NativeModuleSerializer serializer(native_module_, VectorOf(code_table_));
  size_t measured_size = kVersionSize + serializer.Measure();
  if (buffer.size() < measured_size) return false;

  Writer writer(buffer);
  WriteVersion(&writer);

  if (!serializer.Write(&writer)) return false;
  DCHECK_EQ(measured_size, writer.bytes_written());
  return true;
}

class V8_EXPORT_PRIVATE NativeModuleDeserializer {
 public:
  NativeModuleDeserializer() = delete;
  explicit NativeModuleDeserializer(NativeModule*);

  bool Read(Reader* reader);

 private:
  bool ReadHeader(Reader* reader);
  bool ReadCode(uint32_t fn_index, Reader* reader);

  NativeModule* const native_module_;
  bool read_called_;

  DISALLOW_COPY_AND_ASSIGN(NativeModuleDeserializer);
};

NativeModuleDeserializer::NativeModuleDeserializer(NativeModule* native_module)
    : native_module_(native_module), read_called_(false) {}

bool NativeModuleDeserializer::Read(Reader* reader) {
  DCHECK(!read_called_);
  read_called_ = true;

  if (!ReadHeader(reader)) return false;
  uint32_t total_fns = native_module_->num_functions();
  uint32_t first_wasm_fn = native_module_->num_imported_functions();
  for (uint32_t i = first_wasm_fn; i < total_fns; ++i) {
    if (!ReadCode(i, reader)) return false;
  }
  return reader->current_size() == 0;
}

bool NativeModuleDeserializer::ReadHeader(Reader* reader) {
  size_t functions = reader->Read<uint32_t>();
  size_t imports = reader->Read<uint32_t>();
  return functions == native_module_->num_functions() &&
         imports == native_module_->num_imported_functions();
}

bool NativeModuleDeserializer::ReadCode(uint32_t fn_index, Reader* reader) {
  size_t code_section_size = reader->Read<size_t>();
  if (code_section_size == 0) {
    DCHECK(FLAG_wasm_lazy_compilation ||
           native_module_->enabled_features().compilation_hints);
    native_module_->UseLazyStub(fn_index);
    return true;
  }
  size_t constant_pool_offset = reader->Read<size_t>();
  size_t safepoint_table_offset = reader->Read<size_t>();
  size_t handler_table_offset = reader->Read<size_t>();
  size_t code_comment_offset = reader->Read<size_t>();
  size_t unpadded_binary_size = reader->Read<size_t>();
  uint32_t stack_slot_count = reader->Read<uint32_t>();
  uint32_t tagged_parameter_slots = reader->Read<uint32_t>();
  size_t code_size = reader->Read<size_t>();
  size_t reloc_size = reader->Read<size_t>();
  size_t source_position_size = reader->Read<size_t>();
  size_t protected_instructions_size = reader->Read<size_t>();
  WasmCode::Kind kind = reader->Read<WasmCode::Kind>();
  ExecutionTier tier = reader->Read<ExecutionTier>();

  Vector<const byte> code_buffer = {reader->current_location(), code_size};
  reader->Skip(code_size);

  OwnedVector<byte> reloc_info = OwnedVector<byte>::New(reloc_size);
  reader->ReadVector(reloc_info.as_vector());
  OwnedVector<byte> source_pos = OwnedVector<byte>::New(source_position_size);
  reader->ReadVector(source_pos.as_vector());
  auto protected_instructions =
      OwnedVector<trap_handler::ProtectedInstructionData>::New(
          protected_instructions_size);
  reader->ReadVector(Vector<byte>::cast(protected_instructions.as_vector()));

  WasmCode* code = native_module_->AddDeserializedCode(
      fn_index, code_buffer, stack_slot_count, tagged_parameter_slots,
      safepoint_table_offset, handler_table_offset, constant_pool_offset,
      code_comment_offset, unpadded_binary_size,
      std::move(protected_instructions), std::move(reloc_info),
      std::move(source_pos), kind, tier);

  // Relocate the code.
  int mask = RelocInfo::ModeMask(RelocInfo::WASM_CALL) |
             RelocInfo::ModeMask(RelocInfo::WASM_STUB_CALL) |
             RelocInfo::ModeMask(RelocInfo::EXTERNAL_REFERENCE) |
             RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE) |
             RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE_ENCODED);
  for (RelocIterator iter(code->instructions(), code->reloc_info(),
                          code->constant_pool(), mask);
       !iter.done(); iter.next()) {
    RelocInfo::Mode mode = iter.rinfo()->rmode();
    switch (mode) {
      case RelocInfo::WASM_CALL: {
        uint32_t tag = GetWasmCalleeTag(iter.rinfo());
        Address target = native_module_->GetCallTargetForFunction(tag);
        iter.rinfo()->set_wasm_call_address(target, SKIP_ICACHE_FLUSH);
        break;
      }
      case RelocInfo::WASM_STUB_CALL: {
        uint32_t tag = GetWasmCalleeTag(iter.rinfo());
        DCHECK_LT(tag, WasmCode::kRuntimeStubCount);
        Address target = native_module_->runtime_stub_entry(
            static_cast<WasmCode::RuntimeStubId>(tag));
        iter.rinfo()->set_wasm_stub_call_address(target, SKIP_ICACHE_FLUSH);
        break;
      }
      case RelocInfo::EXTERNAL_REFERENCE: {
        uint32_t tag = GetWasmCalleeTag(iter.rinfo());
        Address address = ExternalReferenceList::Get().address_from_tag(tag);
        iter.rinfo()->set_target_external_reference(address, SKIP_ICACHE_FLUSH);
        break;
      }
      case RelocInfo::INTERNAL_REFERENCE:
      case RelocInfo::INTERNAL_REFERENCE_ENCODED: {
        Address offset = iter.rinfo()->target_internal_reference();
        Address target = code->instruction_start() + offset;
        Assembler::deserialization_set_target_internal_reference_at(
            iter.rinfo()->pc(), target, mode);
        break;
      }
      default:
        UNREACHABLE();
    }
  }

  code->MaybePrint();
  code->Validate();

  // Finally, flush the icache for that code.
  FlushInstructionCache(code->instructions().start(),
                        code->instructions().size());

  return true;
}

bool IsSupportedVersion(Vector<const byte> version) {
  if (version.size() < kVersionSize) return false;
  byte current_version[kVersionSize];
  Writer writer({current_version, kVersionSize});
  WriteVersion(&writer);
  return memcmp(version.start(), current_version, kVersionSize) == 0;
}

MaybeHandle<WasmModuleObject> DeserializeNativeModule(
    Isolate* isolate, Vector<const byte> data,
    Vector<const byte> wire_bytes_vec) {
  if (!IsWasmCodegenAllowed(isolate, isolate->native_context())) return {};
  if (!IsSupportedVersion(data)) return {};

  ModuleWireBytes wire_bytes(wire_bytes_vec);
  // TODO(titzer): module features should be part of the serialization format.
  WasmFeatures enabled_features = WasmFeaturesFromIsolate(isolate);
  ModuleResult decode_result =
      DecodeWasmModule(enabled_features, wire_bytes.start(), wire_bytes.end(),
                       false, i::wasm::kWasmOrigin, isolate->counters(),
                       isolate->wasm_engine()->allocator());
  if (decode_result.failed()) return {};
  CHECK_NOT_NULL(decode_result.value());
  WasmModule* module = decode_result.value().get();
  Handle<Script> script =
      CreateWasmScript(isolate, wire_bytes, module->source_map_url);

  OwnedVector<uint8_t> wire_bytes_copy =
      OwnedVector<uint8_t>::Of(wire_bytes_vec);

  Handle<WasmModuleObject> module_object = WasmModuleObject::New(
      isolate, enabled_features, std::move(decode_result).value(),
      std::move(wire_bytes_copy), script, Handle<ByteArray>::null());
  NativeModule* native_module = module_object->native_module();

  native_module->set_lazy_compilation(FLAG_wasm_lazy_compilation);

  NativeModuleDeserializer deserializer(native_module);
  WasmCodeRefScope wasm_code_ref_scope;

  Reader reader(data + kVersionSize);
  if (!deserializer.Read(&reader)) return {};

  CompileJsToWasmWrappers(isolate, native_module->module(),
                          handle(module_object->export_wrappers(), isolate));

  // Log the code within the generated module for profiling.
  native_module->LogWasmCodes(isolate);

  return module_object;
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
