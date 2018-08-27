// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-serialization.h"

#include "src/assembler-inl.h"
#include "src/code-stubs.h"
#include "src/external-reference-table.h"
#include "src/objects-inl.h"
#include "src/objects.h"
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
    if (FLAG_wasm_trace_serialization) {
      OFStream os(stdout);
      os << "wrote: " << (size_t)value << " sized: " << sizeof(T) << std::endl;
    }
  }

  void WriteVector(const Vector<const byte> v) {
    DCHECK_GE(current_size(), v.size());
    if (v.size() > 0) {
      memcpy(current_location(), v.start(), v.size());
      pos_ += v.size();
    }
    if (FLAG_wasm_trace_serialization) {
      OFStream os(stdout);
      os << "wrote vector of " << v.size() << " elements" << std::endl;
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
    if (FLAG_wasm_trace_serialization) {
      OFStream os(stdout);
      os << "read: " << (size_t)value << " sized: " << sizeof(T) << std::endl;
    }
    return value;
  }

  void ReadVector(Vector<byte> v) {
    if (v.size() > 0) {
      DCHECK_GE(current_size(), v.size());
      memcpy(v.start(), current_location(), v.size());
      pos_ += v.size();
    }
    if (FLAG_wasm_trace_serialization) {
      OFStream os(stdout);
      os << "read vector of " << v.size() << " elements" << std::endl;
    }
  }

  void Skip(size_t size) { pos_ += size; }

 private:
  const byte* const start_;
  const byte* const end_;
  const byte* pos_;
};

constexpr size_t kVersionSize = 4 * sizeof(uint32_t);

void WriteVersion(Isolate* isolate, Writer* writer) {
  writer->Write(SerializedData::ComputeMagicNumber(
      isolate->heap()->external_reference_table()));
  writer->Write(Version::Hash());
  writer->Write(static_cast<uint32_t>(CpuFeatures::SupportedFeatures()));
  writer->Write(FlagList::Hash());
}

bool IsSupportedVersion(Isolate* isolate, const Vector<const byte> version) {
  if (version.size() < kVersionSize) return false;
  byte current_version[kVersionSize];
  Writer writer({current_version, kVersionSize});
  WriteVersion(isolate, &writer);
  return memcmp(version.start(), current_version, kVersionSize) == 0;
}

// On Intel, call sites are encoded as a displacement. For linking and for
// serialization/deserialization, we want to store/retrieve a tag (the function
// index). On Intel, that means accessing the raw displacement.
// On ARM64, call sites are encoded as either a literal load or a direct branch.
// Other platforms simply require accessing the target address.
void SetWasmCalleeTag(RelocInfo* rinfo, uint32_t tag) {
#if V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_IA32
  *(reinterpret_cast<uint32_t*>(rinfo->target_address_address())) = tag;
#elif V8_TARGET_ARCH_ARM64
  Instruction* instr = reinterpret_cast<Instruction*>(rinfo->pc());
  if (instr->IsLdrLiteralX()) {
    Memory::Address_at(rinfo->constant_pool_entry_address()) =
        static_cast<Address>(tag);
  } else {
    DCHECK(instr->IsBranchAndLink() || instr->IsUnconditionalBranch());
    instr->SetBranchImmTarget(
        reinterpret_cast<Instruction*>(rinfo->pc() + tag * kInstructionSize));
  }
#else
  Address addr = static_cast<Address>(tag);
  if (rinfo->rmode() == RelocInfo::EXTERNAL_REFERENCE) {
    rinfo->set_target_external_reference(addr, SKIP_ICACHE_FLUSH);
  } else {
    rinfo->set_target_address(addr, SKIP_WRITE_BARRIER, SKIP_ICACHE_FLUSH);
  }
#endif
}

uint32_t GetWasmCalleeTag(RelocInfo* rinfo) {
#if V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_IA32
  return *(reinterpret_cast<uint32_t*>(rinfo->target_address_address()));
#elif V8_TARGET_ARCH_ARM64
  Instruction* instr = reinterpret_cast<Instruction*>(rinfo->pc());
  if (instr->IsLdrLiteralX()) {
    return static_cast<uint32_t>(
        Memory::Address_at(rinfo->constant_pool_entry_address()));
  } else {
    DCHECK(instr->IsBranchAndLink() || instr->IsUnconditionalBranch());
    return static_cast<uint32_t>(instr->ImmPCOffset() / kInstructionSize);
  }
#else
  Address addr = rinfo->rmode() == RelocInfo::EXTERNAL_REFERENCE
                     ? rinfo->target_external_reference()
                     : rinfo->target_address();
  return static_cast<uint32_t>(addr);
#endif
}

constexpr size_t kHeaderSize =
    sizeof(uint32_t) +  // total wasm function count
    sizeof(uint32_t);  // imported functions - i.e. index of first wasm function

constexpr size_t kCodeHeaderSize =
    sizeof(size_t) +         // size of code section
    sizeof(size_t) +         // offset of constant pool
    sizeof(size_t) +         // offset of safepoint table
    sizeof(size_t) +         // offset of handler table
    sizeof(uint32_t) +       // stack slots
    sizeof(size_t) +         // code size
    sizeof(size_t) +         // reloc size
    sizeof(size_t) +         // source positions size
    sizeof(size_t) +         // protected instructions size
    sizeof(WasmCode::Tier);  // tier

// Bitfields used for encoding stub and builtin ids in a tag. We only use 26
// bits total as ARM64 can only encode 26 bits in branch immediate instructions.
class IsStubIdField : public BitField<bool, 0, 1> {};
class StubOrBuiltinIdField
    : public BitField<uint32_t, IsStubIdField::kNext, 25> {};
static_assert(StubOrBuiltinIdField::kNext == 26,
              "ARM64 only supports 26 bits for this field");

}  // namespace

class V8_EXPORT_PRIVATE NativeModuleSerializer {
 public:
  NativeModuleSerializer() = delete;
  NativeModuleSerializer(Isolate*, const NativeModule*);

  size_t Measure() const;
  bool Write(Writer* writer);

 private:
  size_t MeasureCopiedStubs() const;
  size_t MeasureCode(const WasmCode*) const;

  void WriteHeader(Writer* writer);
  void WriteCopiedStubs(Writer* writer);
  void WriteCode(const WasmCode*, Writer* writer);

  uint32_t EncodeBuiltinOrStub(Address);

  Isolate* const isolate_;
  const NativeModule* const native_module_;
  bool write_called_;

  // wasm and copied stubs reverse lookup
  std::map<Address, uint32_t> wasm_targets_lookup_;
  // immovable builtins and runtime entries lookup
  std::map<Address, uint32_t> reference_table_lookup_;
  std::map<Address, uint32_t> stub_lookup_;
  std::map<Address, uint32_t> builtin_lookup_;

  DISALLOW_COPY_AND_ASSIGN(NativeModuleSerializer);
};

NativeModuleSerializer::NativeModuleSerializer(Isolate* isolate,
                                               const NativeModule* module)
    : isolate_(isolate), native_module_(module), write_called_(false) {
  DCHECK_NOT_NULL(isolate_);
  DCHECK_NOT_NULL(native_module_);
  // TODO(mtrofin): persist the export wrappers. Ideally, we'd only persist
  // the unique ones, i.e. the cache.
  ExternalReferenceTable* table = isolate_->heap()->external_reference_table();
  for (uint32_t i = 0; i < table->size(); ++i) {
    Address addr = table->address(i);
    reference_table_lookup_.insert(std::make_pair(addr, i));
  }
  // Defer populating stub_lookup_ to when we write the stubs.
  for (auto pair : native_module_->trampolines_) {
    v8::internal::Code* code = Code::GetCodeFromTargetAddress(pair.first);
    int builtin_index = code->builtin_index();
    if (builtin_index >= 0) {
      uint32_t tag = static_cast<uint32_t>(builtin_index);
      builtin_lookup_.insert(std::make_pair(pair.second, tag));
    }
  }
}

size_t NativeModuleSerializer::MeasureCopiedStubs() const {
  size_t size = sizeof(uint32_t);  // number of stubs
  for (auto pair : native_module_->trampolines_) {
    v8::internal::Code* code = Code::GetCodeFromTargetAddress(pair.first);
    int builtin_index = code->builtin_index();
    if (builtin_index < 0) size += sizeof(uint32_t);  // stub key
  }
  return size;
}

size_t NativeModuleSerializer::MeasureCode(const WasmCode* code) const {
  return code->instructions().size() + code->reloc_info().size() +
         code->source_positions().size() +
         code->protected_instructions().size() *
             sizeof(trap_handler::ProtectedInstructionData);
}

size_t NativeModuleSerializer::Measure() const {
  size_t size = kHeaderSize + MeasureCopiedStubs();
  uint32_t first_wasm_fn = native_module_->num_imported_functions();
  uint32_t total_fns = native_module_->function_count();
  for (uint32_t i = first_wasm_fn; i < total_fns; ++i) {
    size += kCodeHeaderSize;
    size += MeasureCode(native_module_->code(i));
  }
  return size;
}

void NativeModuleSerializer::WriteHeader(Writer* writer) {
  writer->Write(native_module_->function_count());
  writer->Write(native_module_->num_imported_functions());
}

void NativeModuleSerializer::WriteCopiedStubs(Writer* writer) {
  // Write the number of stubs and their keys.
  // TODO(all) Serialize the stubs as WasmCode.
  size_t stubs_size = MeasureCopiedStubs();
  // Get the stub count from the number of keys.
  size_t num_stubs = (stubs_size - sizeof(uint32_t)) / sizeof(uint32_t);
  writer->Write(static_cast<uint32_t>(num_stubs));
  uint32_t stub_id = 0;

  for (auto pair : native_module_->trampolines_) {
    v8::internal::Code* code = Code::GetCodeFromTargetAddress(pair.first);
    int builtin_index = code->builtin_index();
    if (builtin_index < 0) {
      stub_lookup_.insert(std::make_pair(pair.second, stub_id));
      writer->Write(code->stub_key());
      ++stub_id;
    }
  }
}

void NativeModuleSerializer::WriteCode(const WasmCode* code, Writer* writer) {
  // Write the size of the entire code section, followed by the code header.
  writer->Write(MeasureCode(code));
  writer->Write(code->constant_pool_offset());
  writer->Write(code->safepoint_table_offset());
  writer->Write(code->handler_table_offset());
  writer->Write(code->stack_slots());
  writer->Write(code->instructions().size());
  writer->Write(code->reloc_info().size());
  writer->Write(code->source_positions().size());
  writer->Write(code->protected_instructions().size());
  writer->Write(code->tier());

  // Get a pointer to the destination buffer, to hold relocated code.
  byte* serialized_code_start = writer->current_buffer().start();
  byte* code_start = serialized_code_start;
  size_t code_size = code->instructions().size();
  writer->Skip(code_size);
  // Write the reloc info, source positions, and protected code.
  writer->WriteVector(code->reloc_info());
  writer->WriteVector(code->source_positions());
  writer->WriteVector(
      {reinterpret_cast<const byte*>(code->protected_instructions().data()),
       sizeof(trap_handler::ProtectedInstructionData) *
           code->protected_instructions().size()});
#if V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_ARM
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
  int mask = RelocInfo::ModeMask(RelocInfo::CODE_TARGET) |
             RelocInfo::ModeMask(RelocInfo::WASM_CALL) |
             RelocInfo::ModeMask(RelocInfo::RUNTIME_ENTRY) |
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
      case RelocInfo::CODE_TARGET: {
        Address orig_target = orig_iter.rinfo()->target_address();
        uint32_t tag = EncodeBuiltinOrStub(orig_target);
        SetWasmCalleeTag(iter.rinfo(), tag);
      } break;
      case RelocInfo::WASM_CALL: {
        Address orig_target = orig_iter.rinfo()->wasm_call_address();
        uint32_t tag = wasm_targets_lookup_[orig_target];
        SetWasmCalleeTag(iter.rinfo(), tag);
      } break;
      case RelocInfo::RUNTIME_ENTRY: {
        Address orig_target = orig_iter.rinfo()->target_address();
        auto ref_iter = reference_table_lookup_.find(orig_target);
        DCHECK(ref_iter != reference_table_lookup_.end());
        uint32_t tag = ref_iter->second;
        SetWasmCalleeTag(iter.rinfo(), tag);
      } break;
      case RelocInfo::EXTERNAL_REFERENCE: {
        Address orig_target = orig_iter.rinfo()->target_external_reference();
        auto ref_iter = reference_table_lookup_.find(orig_target);
        DCHECK(ref_iter != reference_table_lookup_.end());
        uint32_t tag = ref_iter->second;
        SetWasmCalleeTag(iter.rinfo(), tag);
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

uint32_t NativeModuleSerializer::EncodeBuiltinOrStub(Address address) {
  auto builtin_iter = builtin_lookup_.find(address);
  uint32_t tag = 0;
  if (builtin_iter != builtin_lookup_.end()) {
    uint32_t id = builtin_iter->second;
    DCHECK_LT(id, std::numeric_limits<uint16_t>::max());
    tag = IsStubIdField::encode(false) | StubOrBuiltinIdField::encode(id);
  } else {
    auto stub_iter = stub_lookup_.find(address);
    DCHECK(stub_iter != stub_lookup_.end());
    uint32_t id = stub_iter->second;
    tag = IsStubIdField::encode(true) | StubOrBuiltinIdField::encode(id);
  }
  return tag;
}

bool NativeModuleSerializer::Write(Writer* writer) {
  DCHECK(!write_called_);
  write_called_ = true;

  WriteHeader(writer);
  WriteCopiedStubs(writer);

  uint32_t total_fns = native_module_->function_count();
  uint32_t first_wasm_fn = native_module_->num_imported_functions();
  for (uint32_t i = first_wasm_fn; i < total_fns; ++i) {
    const WasmCode* code = native_module_->code(i);
    WriteCode(code, writer);
  }
  return true;
}

size_t GetSerializedNativeModuleSize(
    Isolate* isolate, Handle<WasmCompiledModule> compiled_module) {
  NativeModule* native_module = compiled_module->GetNativeModule();
  NativeModuleSerializer serializer(isolate, native_module);
  return kVersionSize + serializer.Measure();
}

bool SerializeNativeModule(Isolate* isolate,
                           Handle<WasmCompiledModule> compiled_module,
                           Vector<byte> buffer) {
  NativeModule* native_module = compiled_module->GetNativeModule();
  NativeModuleSerializer serializer(isolate, native_module);
  size_t measured_size = serializer.Measure();
  if (buffer.size() < measured_size) return false;

  Writer writer(buffer);
  WriteVersion(isolate, &writer);

  return serializer.Write(&writer);
}

class V8_EXPORT_PRIVATE NativeModuleDeserializer {
 public:
  NativeModuleDeserializer() = delete;
  NativeModuleDeserializer(Isolate*, NativeModule*);

  bool Read(Reader* reader);

 private:
  bool ReadHeader(Reader* reader);
  bool ReadCode(uint32_t fn_index, Reader* reader);
  bool ReadStubs(Reader* reader);
  Address GetTrampolineOrStubFromTag(uint32_t);

  Isolate* const isolate_;
  NativeModule* const native_module_;

  std::vector<Address> stubs_;
  bool read_called_;

  DISALLOW_COPY_AND_ASSIGN(NativeModuleDeserializer);
};

NativeModuleDeserializer::NativeModuleDeserializer(Isolate* isolate,
                                                   NativeModule* native_module)
    : isolate_(isolate), native_module_(native_module), read_called_(false) {}

bool NativeModuleDeserializer::Read(Reader* reader) {
  DCHECK(!read_called_);
  read_called_ = true;

  if (!ReadHeader(reader)) return false;
  if (!ReadStubs(reader)) return false;
  uint32_t total_fns = native_module_->function_count();
  uint32_t first_wasm_fn = native_module_->num_imported_functions();
  for (uint32_t i = first_wasm_fn; i < total_fns; ++i) {
    if (!ReadCode(i, reader)) return false;
  }
  return reader->current_size() == 0;
}

bool NativeModuleDeserializer::ReadHeader(Reader* reader) {
  size_t functions = reader->Read<uint32_t>();
  size_t imports = reader->Read<uint32_t>();
  return functions == native_module_->function_count() &&
         imports == native_module_->num_imported_functions();
}

bool NativeModuleDeserializer::ReadStubs(Reader* reader) {
  size_t num_stubs = reader->Read<uint32_t>();
  stubs_.reserve(num_stubs);
  for (size_t i = 0; i < num_stubs; ++i) {
    uint32_t key = reader->Read<uint32_t>();
    v8::internal::Code* stub =
        *(v8::internal::CodeStub::GetCode(isolate_, key).ToHandleChecked());
    stubs_.push_back(native_module_->GetLocalAddressFor(handle(stub)));
  }
  return true;
}

bool NativeModuleDeserializer::ReadCode(uint32_t fn_index, Reader* reader) {
  size_t code_section_size = reader->Read<size_t>();
  USE(code_section_size);
  size_t constant_pool_offset = reader->Read<size_t>();
  size_t safepoint_table_offset = reader->Read<size_t>();
  size_t handler_table_offset = reader->Read<size_t>();
  uint32_t stack_slot_count = reader->Read<uint32_t>();
  size_t code_size = reader->Read<size_t>();
  size_t reloc_size = reader->Read<size_t>();
  size_t source_position_size = reader->Read<size_t>();
  size_t protected_instructions_size = reader->Read<size_t>();
  WasmCode::Tier tier = reader->Read<WasmCode::Tier>();

  Vector<const byte> code_buffer = {reader->current_location(), code_size};
  reader->Skip(code_size);

  std::unique_ptr<byte[]> reloc_info;
  if (reloc_size > 0) {
    reloc_info.reset(new byte[reloc_size]);
    reader->ReadVector({reloc_info.get(), reloc_size});
  }
  std::unique_ptr<byte[]> source_pos;
  if (source_position_size > 0) {
    source_pos.reset(new byte[source_position_size]);
    reader->ReadVector({source_pos.get(), source_position_size});
  }
  std::unique_ptr<ProtectedInstructions> protected_instructions(
      new ProtectedInstructions(protected_instructions_size));
  if (protected_instructions_size > 0) {
    size_t size = sizeof(trap_handler::ProtectedInstructionData) *
                  protected_instructions->size();
    Vector<byte> data(reinterpret_cast<byte*>(protected_instructions->data()),
                      size);
    reader->ReadVector(data);
  }
  WasmCode* ret = native_module_->AddOwnedCode(
      code_buffer, std::move(reloc_info), reloc_size, std::move(source_pos),
      source_position_size, Just(fn_index), WasmCode::kFunction,
      constant_pool_offset, stack_slot_count, safepoint_table_offset,
      handler_table_offset, std::move(protected_instructions), tier,
      WasmCode::kNoFlushICache);
  native_module_->code_table_[fn_index] = ret;

  // now relocate the code
  int mask = RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT) |
             RelocInfo::ModeMask(RelocInfo::CODE_TARGET) |
             RelocInfo::ModeMask(RelocInfo::RUNTIME_ENTRY) |
             RelocInfo::ModeMask(RelocInfo::EXTERNAL_REFERENCE) |
             RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE) |
             RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE_ENCODED) |
             RelocInfo::ModeMask(RelocInfo::WASM_CODE_TABLE_ENTRY);
  for (RelocIterator iter(ret->instructions(), ret->reloc_info(),
                          ret->constant_pool(), mask);
       !iter.done(); iter.next()) {
    RelocInfo::Mode mode = iter.rinfo()->rmode();
    switch (mode) {
      case RelocInfo::EMBEDDED_OBJECT: {
        // We only expect {undefined}. We check for that when we add code.
        iter.rinfo()->set_target_object(isolate_->heap()->undefined_value(),
                                        SKIP_WRITE_BARRIER, SKIP_ICACHE_FLUSH);
        break;
      }
      case RelocInfo::CODE_TARGET: {
        uint32_t tag = GetWasmCalleeTag(iter.rinfo());
        Address target = GetTrampolineOrStubFromTag(tag);
        iter.rinfo()->set_target_address(target, SKIP_WRITE_BARRIER,
                                         SKIP_ICACHE_FLUSH);
        break;
      }
      case RelocInfo::RUNTIME_ENTRY: {
        uint32_t tag = GetWasmCalleeTag(iter.rinfo());
        Address address =
            isolate_->heap()->external_reference_table()->address(tag);
        iter.rinfo()->set_target_runtime_entry(address, SKIP_WRITE_BARRIER,
                                               SKIP_ICACHE_FLUSH);
        break;
      }
      case RelocInfo::EXTERNAL_REFERENCE: {
        uint32_t tag = GetWasmCalleeTag(iter.rinfo());
        Address address =
            isolate_->heap()->external_reference_table()->address(tag);
        iter.rinfo()->set_target_external_reference(address, SKIP_ICACHE_FLUSH);
        break;
      }
      case RelocInfo::INTERNAL_REFERENCE:
      case RelocInfo::INTERNAL_REFERENCE_ENCODED: {
        Address offset = iter.rinfo()->target_internal_reference();
        Address target = ret->instruction_start() + offset;
        Assembler::deserialization_set_target_internal_reference_at(
            iter.rinfo()->pc(), target, mode);
        break;
      }
      case RelocInfo::WASM_CODE_TABLE_ENTRY: {
        DCHECK(FLAG_wasm_tier_up);
        DCHECK(ret->is_liftoff());
        WasmCode* const* code_table_entry =
            native_module_->code_table().data() + ret->index();
        iter.rinfo()->set_wasm_code_table_entry(
            reinterpret_cast<Address>(code_table_entry), SKIP_ICACHE_FLUSH);
        break;
      }
      default:
        UNREACHABLE();
    }
  }
  // Flush the i-cache here instead of in AddOwnedCode, to include the changes
  // made while iterating over the RelocInfo above.
  Assembler::FlushICache(ret->instructions().start(),
                         ret->instructions().size());

  return true;
}

Address NativeModuleDeserializer::GetTrampolineOrStubFromTag(uint32_t tag) {
  uint32_t id = StubOrBuiltinIdField::decode(tag);
  if (IsStubIdField::decode(tag)) {
    return stubs_[id];
  } else {
    v8::internal::Code* builtin = isolate_->builtins()->builtin(id);
    return native_module_->GetLocalAddressFor(handle(builtin));
  }
}

MaybeHandle<WasmModuleObject> DeserializeNativeModule(
    Isolate* isolate, Vector<const byte> data, Vector<const byte> wire_bytes) {
  if (!IsWasmCodegenAllowed(isolate, isolate->native_context())) {
    return {};
  }
  if (!IsSupportedVersion(isolate, data)) {
    return {};
  }
  ModuleResult decode_result =
      SyncDecodeWasmModule(isolate, wire_bytes.start(), wire_bytes.end(), false,
                           i::wasm::kWasmOrigin);
  if (!decode_result.ok()) return {};
  CHECK_NOT_NULL(decode_result.val);
  Handle<String> module_bytes =
      isolate->factory()
          ->NewStringFromOneByte(
              {wire_bytes.start(), static_cast<size_t>(wire_bytes.length())},
              TENURED)
          .ToHandleChecked();
  DCHECK(module_bytes->IsSeqOneByteString());
  // The {managed_module} will take ownership of the {WasmModule} object,
  // and it will be destroyed when the GC reclaims the wrapper object.
  Handle<Managed<WasmModule>> managed_module =
      Managed<WasmModule>::FromUniquePtr(isolate, std::move(decode_result.val));
  Handle<Script> script = CreateWasmScript(isolate, wire_bytes);
  Handle<WasmSharedModuleData> shared = WasmSharedModuleData::New(
      isolate, managed_module, Handle<SeqOneByteString>::cast(module_bytes),
      script, Handle<ByteArray>::null());
  int export_wrappers_size =
      static_cast<int>(shared->module()->num_exported_functions);
  Handle<FixedArray> export_wrappers = isolate->factory()->NewFixedArray(
      static_cast<int>(export_wrappers_size), TENURED);

  // TODO(eholk): We need to properly preserve the flag whether the trap
  // handler was used or not when serializing.
  UseTrapHandler use_trap_handler =
      trap_handler::IsTrapHandlerEnabled() ? kUseTrapHandler : kNoTrapHandler;
  wasm::ModuleEnv env(shared->module(), use_trap_handler,
                      wasm::RuntimeExceptionSupport::kRuntimeExceptionSupport);
  Handle<WasmCompiledModule> compiled_module =
      WasmCompiledModule::New(isolate, shared->module(), env);
  compiled_module->GetNativeModule()->SetSharedModuleData(shared);
  NativeModuleDeserializer deserializer(isolate,
                                        compiled_module->GetNativeModule());

  Reader reader(data + kVersionSize);
  if (!deserializer.Read(&reader)) return {};

  Handle<WasmModuleObject> module_object =
      WasmModuleObject::New(isolate, compiled_module, export_wrappers, shared);

  // TODO(6792): Wrappers below might be cloned using {Factory::CopyCode}. This
  // requires unlocking the code space here. This should eventually be moved
  // into the allocator.
  CodeSpaceMemoryModificationScope modification_scope(isolate->heap());
  CompileJsToWasmWrappers(isolate, module_object, isolate->counters());

  // There are no instances for this module yet, which means we need to reset
  // the module into a state as if the last instance was collected.
  WasmCompiledModule::Reset(isolate, *compiled_module);

  return module_object;
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
