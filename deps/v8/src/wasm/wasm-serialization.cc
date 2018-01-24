// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-serialization.h"

#include "src/assembler-inl.h"
#include "src/code-stubs.h"
#include "src/external-reference-table.h"
#include "src/objects-inl.h"
#include "src/objects.h"
#include "src/snapshot/serializer-common.h"
#include "src/version.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-result.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace {
void SetRawTargetData(RelocInfo* rinfo, uint32_t value) {
  if (rinfo->target_address_size() == sizeof(uint32_t)) {
    *(reinterpret_cast<uint32_t*>(rinfo->target_address_address())) = value;
    return;
  } else {
    DCHECK_EQ(rinfo->target_address_size(), sizeof(intptr_t));
    DCHECK_EQ(rinfo->target_address_size(), 8);
    *(reinterpret_cast<intptr_t*>(rinfo->target_address_address())) =
        static_cast<intptr_t>(value);
    return;
  }
}

class Writer {
 public:
  explicit Writer(Vector<byte> buffer) : buffer_(buffer) {}
  template <typename T>
  void Write(const T& value) {
    if (FLAG_wasm_trace_serialization) {
      OFStream os(stdout);
      os << "wrote: " << (size_t)value << " sized: " << sizeof(T) << std::endl;
    }
    DCHECK_GE(buffer_.size(), sizeof(T));
    memcpy(buffer_.start(), reinterpret_cast<const byte*>(&value), sizeof(T));
    buffer_ = buffer_ + sizeof(T);
  }

  void WriteVector(const Vector<const byte> data) {
    DCHECK_GE(buffer_.size(), data.size());
    if (data.size() > 0) {
      memcpy(buffer_.start(), data.start(), data.size());
      buffer_ = buffer_ + data.size();
    }
    if (FLAG_wasm_trace_serialization) {
      OFStream os(stdout);
      os << "wrote vector of " << data.size() << " elements" << std::endl;
    }
  }
  Vector<byte> current_buffer() const { return buffer_; }

 private:
  Vector<byte> buffer_;
};

class Reader {
 public:
  explicit Reader(Vector<const byte> buffer) : buffer_(buffer) {}

  template <typename T>
  T Read() {
    DCHECK_GE(buffer_.size(), sizeof(T));
    T ret;
    memcpy(reinterpret_cast<byte*>(&ret), buffer_.start(), sizeof(T));
    buffer_ = buffer_ + sizeof(T);
    if (FLAG_wasm_trace_serialization) {
      OFStream os(stdout);
      os << "read: " << (size_t)ret << " sized: " << sizeof(T) << std::endl;
    }
    return ret;
  }

  Vector<const byte> GetSubvector(size_t size) {
    Vector<const byte> ret = {buffer_.start(), size};
    buffer_ = buffer_ + size;
    return ret;
  }

  void ReadIntoVector(const Vector<byte> data) {
    if (data.size() > 0) {
      DCHECK_GE(buffer_.size(), data.size());
      memcpy(data.start(), buffer_.start(), data.size());
      buffer_ = buffer_ + data.size();
    }
    if (FLAG_wasm_trace_serialization) {
      OFStream os(stdout);
      os << "read vector of " << data.size() << " elements" << std::endl;
    }
  }

  Vector<const byte> current_buffer() const { return buffer_; }

 private:
  Vector<const byte> buffer_;
};

}  // namespace

size_t WasmSerializedFormatVersion::GetVersionSize() { return kVersionSize; }

bool WasmSerializedFormatVersion::WriteVersion(Isolate* isolate,
                                               Vector<byte> buffer) {
  if (buffer.size() < GetVersionSize()) return false;
  Writer writer(buffer);
  writer.Write(SerializedData::ComputeMagicNumber(
      ExternalReferenceTable::instance(isolate)));
  writer.Write(Version::Hash());
  writer.Write(static_cast<uint32_t>(CpuFeatures::SupportedFeatures()));
  writer.Write(FlagList::Hash());
  return true;
}

bool WasmSerializedFormatVersion::IsSupportedVersion(
    Isolate* isolate, const Vector<const byte> buffer) {
  if (buffer.size() < kVersionSize) return false;
  byte version[kVersionSize];
  CHECK(WriteVersion(isolate, {version, kVersionSize}));
  if (memcmp(buffer.start(), version, kVersionSize) == 0) return true;
  return false;
}

NativeModuleSerializer::NativeModuleSerializer(Isolate* isolate,
                                               const NativeModule* module)
    : isolate_(isolate), native_module_(module) {
  DCHECK_NOT_NULL(isolate_);
  DCHECK_NOT_NULL(native_module_);
  DCHECK_NULL(native_module_->lazy_builtin_);
  // TODO(mtrofin): persist the export wrappers. Ideally, we'd only persist
  // the unique ones, i.e. the cache.
  ExternalReferenceTable* table = ExternalReferenceTable::instance(isolate_);
  for (uint32_t i = 0; i < table->size(); ++i) {
    Address addr = table->address(i);
    reference_table_lookup_.insert(std::make_pair(addr, i));
  }
  // defer populating the stub_lookup_ to when we buffer the stubs
  for (auto pair : native_module_->trampolines_) {
    v8::internal::Code* code = Code::GetCodeFromTargetAddress(pair.first);
    int builtin_index = code->builtin_index();
    if (builtin_index >= 0) {
      uint32_t tag = static_cast<uint32_t>(builtin_index);
      builtin_lookup_.insert(std::make_pair(pair.second, tag));
    }
  }
  BufferHeader();
  state_ = Metadata;
}

size_t NativeModuleSerializer::MeasureHeader() const {
  return sizeof(uint32_t) +  // total wasm fct count
         sizeof(
             uint32_t) +  // imported fcts - i.e. index of first wasm function
         sizeof(uint32_t) +  // table count
         native_module_->specialization_data_.function_tables.size() *
             2  // 2 same-sized tables, containing pointers
             * sizeof(GlobalHandleAddress);
}

void NativeModuleSerializer::BufferHeader() {
  size_t metadata_size = MeasureHeader();
  scratch_.resize(metadata_size);
  remaining_ = {scratch_.data(), metadata_size};
  Writer writer(remaining_);
  writer.Write(native_module_->FunctionCount());
  writer.Write(native_module_->num_imported_functions());
  writer.Write(static_cast<uint32_t>(
      native_module_->specialization_data_.function_tables.size()));
  for (size_t i = 0,
              e = native_module_->specialization_data_.function_tables.size();
       i < e; ++i) {
    writer.Write(native_module_->specialization_data_.function_tables[i]);
    writer.Write(native_module_->specialization_data_.signature_tables[i]);
  }
}

size_t NativeModuleSerializer::GetCodeHeaderSize() {
  return sizeof(size_t) +    // size of this section
         sizeof(size_t) +    // offset of constant pool
         sizeof(size_t) +    // offset of safepoint table
         sizeof(uint32_t) +  // stack slots
         sizeof(size_t) +    // code size
         sizeof(size_t) +    // reloc size
         sizeof(uint32_t) +  // handler size
         sizeof(uint32_t) +  // source positions size
         sizeof(size_t) +    // protected instructions size
         sizeof(bool);       // is_liftoff
}

size_t NativeModuleSerializer::MeasureCode(const WasmCode* code) const {
  FixedArray* handler_table = GetHandlerTable(code);
  ByteArray* source_positions = GetSourcePositions(code);
  return GetCodeHeaderSize() + code->instructions().size() +  // code
         code->reloc_info().size() +                          // reloc info
         (handler_table == nullptr
              ? 0
              : static_cast<uint32_t>(
                    handler_table->length())) +  // handler table
         (source_positions == nullptr
              ? 0
              : static_cast<uint32_t>(
                    source_positions->length())) +  // source positions
         code->protected_instructions().size() *
             sizeof(trap_handler::ProtectedInstructionData);
}

size_t NativeModuleSerializer::Measure() const {
  size_t ret = MeasureHeader() + MeasureCopiedStubs();
  for (uint32_t i = native_module_->num_imported_functions(),
                e = native_module_->FunctionCount();
       i < e; ++i) {
    ret += MeasureCode(native_module_->GetCode(i));
  }
  return ret;
}

size_t NativeModuleSerializer::DrainBuffer(Vector<byte> dest) {
  size_t to_write = std::min(dest.size(), remaining_.size());
  memcpy(dest.start(), remaining_.start(), to_write);
  DCHECK_GE(remaining_.size(), to_write);
  remaining_ = remaining_ + to_write;
  return to_write;
}

size_t NativeModuleSerializer::MeasureCopiedStubs() const {
  size_t ret = sizeof(uint32_t) +  // number of stubs
               native_module_->stubs_.size() * sizeof(uint32_t);  // stub keys
  for (auto pair : native_module_->trampolines_) {
    v8::internal::Code* code = Code::GetCodeFromTargetAddress(pair.first);
    int builtin_index = code->builtin_index();
    if (builtin_index < 0) ret += sizeof(uint32_t);
  }
  return ret;
}

void NativeModuleSerializer::BufferCopiedStubs() {
  // We buffer all the stubs together, because they are very likely
  // few and small. Each stub is buffered like a WasmCode would,
  // and in addition prefaced by its stub key. The whole section is prefaced
  // by the number of stubs.
  size_t buff_size = MeasureCopiedStubs();
  scratch_.resize(buff_size);
  remaining_ = {scratch_.data(), buff_size};
  Writer writer(remaining_);
  writer.Write(
      static_cast<uint32_t>((buff_size - sizeof(uint32_t)) / sizeof(uint32_t)));
  uint32_t stub_id = 0;

  for (auto pair : native_module_->stubs_) {
    uint32_t key = pair.first;
    writer.Write(key);
    stub_lookup_.insert(
        std::make_pair(pair.second->instructions().start(), stub_id));
    ++stub_id;
  }

  for (auto pair : native_module_->trampolines_) {
    v8::internal::Code* code = Code::GetCodeFromTargetAddress(pair.first);
    int builtin_index = code->builtin_index();
    if (builtin_index < 0) {
      stub_lookup_.insert(std::make_pair(pair.second, stub_id));
      writer.Write(code->stub_key());
      ++stub_id;
    }
  }
}

FixedArray* NativeModuleSerializer::GetHandlerTable(
    const WasmCode* code) const {
  if (code->kind() != WasmCode::Function) return nullptr;
  uint32_t index = code->index();
  // We write the address, the size, and then copy the code as-is, followed
  // by reloc info, followed by handler table and source positions.
  Object* handler_table_entry =
      native_module_->compiled_module()->handler_table()->get(
          static_cast<int>(index));
  if (handler_table_entry->IsFixedArray()) {
    return FixedArray::cast(handler_table_entry);
  }
  return nullptr;
}

ByteArray* NativeModuleSerializer::GetSourcePositions(
    const WasmCode* code) const {
  if (code->kind() != WasmCode::Function) return nullptr;
  uint32_t index = code->index();
  Object* source_positions_entry =
      native_module_->compiled_module()->source_positions()->get(
          static_cast<int>(index));
  if (source_positions_entry->IsByteArray()) {
    return ByteArray::cast(source_positions_entry);
  }
  return nullptr;
}

void NativeModuleSerializer::BufferCurrentWasmCode() {
  const WasmCode* code = native_module_->GetCode(index_);
  size_t size = MeasureCode(code);
  scratch_.resize(size);
  remaining_ = {scratch_.data(), size};
  BufferCodeInAllocatedScratch(code);
}

void NativeModuleSerializer::BufferCodeInAllocatedScratch(
    const WasmCode* code) {
  // We write the address, the size, and then copy the code as-is, followed
  // by reloc info, followed by handler table and source positions.
  FixedArray* handler_table_entry = GetHandlerTable(code);
  uint32_t handler_table_size = 0;
  Address handler_table = nullptr;
  if (handler_table_entry != nullptr) {
    handler_table_size = static_cast<uint32_t>(handler_table_entry->length());
    handler_table = reinterpret_cast<Address>(
        handler_table_entry->GetFirstElementAddress());
  }
  ByteArray* source_positions_entry = GetSourcePositions(code);
  Address source_positions = nullptr;
  uint32_t source_positions_size = 0;
  if (source_positions_entry != nullptr) {
    source_positions = source_positions_entry->GetDataStartAddress();
    source_positions_size =
        static_cast<uint32_t>(source_positions_entry->length());
  }
  Writer writer(remaining_);
  // write the header
  writer.Write(MeasureCode(code));
  writer.Write(code->constant_pool_offset());
  writer.Write(code->safepoint_table_offset());
  writer.Write(code->stack_slots());
  writer.Write(code->instructions().size());
  writer.Write(code->reloc_info().size());
  writer.Write(handler_table_size);
  writer.Write(source_positions_size);
  writer.Write(code->protected_instructions().size());
  writer.Write(code->is_liftoff());
  // next is the code, which we have to reloc.
  Address serialized_code_start = writer.current_buffer().start();
  // write the code and everything else
  writer.WriteVector(code->instructions());
  writer.WriteVector(code->reloc_info());
  writer.WriteVector({handler_table, handler_table_size});
  writer.WriteVector({source_positions, source_positions_size});
  writer.WriteVector(
      {reinterpret_cast<const byte*>(code->protected_instructions().data()),
       sizeof(trap_handler::ProtectedInstructionData) *
           code->protected_instructions().size()});
  // now relocate the code
  int mask = RelocInfo::ModeMask(RelocInfo::CODE_TARGET) |
             RelocInfo::ModeMask(RelocInfo::WASM_CALL) |
             RelocInfo::ModeMask(RelocInfo::RUNTIME_ENTRY);
  RelocIterator orig_iter(code->instructions(), code->reloc_info(),
                          code->constant_pool(), mask);
  for (RelocIterator
           iter({serialized_code_start, code->instructions().size()},
                code->reloc_info(),
                serialized_code_start + code->constant_pool_offset(), mask);
       !iter.done(); iter.next(), orig_iter.next()) {
    RelocInfo::Mode mode = orig_iter.rinfo()->rmode();
    switch (mode) {
      case RelocInfo::CODE_TARGET: {
        Address orig_target = orig_iter.rinfo()->target_address();
        uint32_t tag = EncodeBuiltinOrStub(orig_target);
        SetRawTargetData(iter.rinfo(), tag);
      } break;
      case RelocInfo::WASM_CALL: {
        Address orig_target = orig_iter.rinfo()->wasm_call_address();
        uint32_t tag = wasm_targets_lookup_[orig_target];
        SetRawTargetData(iter.rinfo(), tag);
      } break;
      case RelocInfo::RUNTIME_ENTRY: {
        Address orig_target = orig_iter.rinfo()->target_address();
        uint32_t tag = reference_table_lookup_[orig_target];
        SetRawTargetData(iter.rinfo(), tag);
      } break;
      default:
        UNREACHABLE();
    }
  }
}

uint32_t NativeModuleSerializer::EncodeBuiltinOrStub(Address address) {
  auto builtin_iter = builtin_lookup_.find(address);
  uint32_t tag = 0;
  if (builtin_iter != builtin_lookup_.end()) {
    uint32_t id = builtin_iter->second;
    DCHECK_LT(id, std::numeric_limits<uint16_t>::max());
    tag = id << 16;
  } else {
    auto stub_iter = stub_lookup_.find(address);
    DCHECK(stub_iter != stub_lookup_.end());
    uint32_t id = stub_iter->second;
    DCHECK_LT(id, std::numeric_limits<uint16_t>::max());
    tag = id & 0x0000ffff;
  }
  return tag;
}

size_t NativeModuleSerializer::Write(Vector<byte> dest) {
  Vector<byte> original = dest;
  while (dest.size() > 0) {
    switch (state_) {
      case Metadata: {
        dest = dest + DrainBuffer(dest);
        if (remaining_.size() == 0) {
          BufferCopiedStubs();
          state_ = Stubs;
        }
        break;
      }
      case Stubs: {
        dest = dest + DrainBuffer(dest);
        if (remaining_.size() == 0) {
          index_ = native_module_->num_imported_functions();
          BufferCurrentWasmCode();
          state_ = CodeSection;
        }
        break;
      }
      case CodeSection: {
        dest = dest + DrainBuffer(dest);
        if (remaining_.size() == 0) {
          if (++index_ < native_module_->FunctionCount()) {
            BufferCurrentWasmCode();
          } else {
            state_ = Done;
          }
        }
        break;
      }
      default:
        UNREACHABLE();
    }
  }
  DCHECK_GE(original.size(), dest.size());
  return original.size() - dest.size();
}

// static
std::pair<std::unique_ptr<byte[]>, size_t>
NativeModuleSerializer::SerializeWholeModule(
    Isolate* isolate, Handle<WasmCompiledModule> compiled_module) {
  NativeModule* native_module = compiled_module->GetNativeModule();
  NativeModuleSerializer serializer(isolate, native_module);
  size_t version_size = WasmSerializedFormatVersion::GetVersionSize();
  size_t buff_size = serializer.Measure() + version_size;
  std::unique_ptr<byte[]> ret(new byte[buff_size]);
  if (!WasmSerializedFormatVersion::WriteVersion(isolate,
                                                 {ret.get(), buff_size})) {
    return {};
  }

  size_t written =
      serializer.Write({ret.get() + version_size, buff_size - version_size});
  if (written != buff_size - version_size) return {};

  return {std::move(ret), buff_size};
}

NativeModuleDeserializer::NativeModuleDeserializer(Isolate* isolate,
                                                   NativeModule* native_module)
    : isolate_(isolate), native_module_(native_module) {}

void NativeModuleDeserializer::Expect(size_t size) {
  scratch_.resize(size);
  current_expectation_ = size;
  unread_ = {scratch_.data(), size};
}

bool NativeModuleDeserializer::Read(Vector<const byte> data) {
  unread_ = data;
  if (!ReadHeader()) return false;
  if (!ReadStubs()) return false;
  index_ = native_module_->num_imported_functions();
  for (; index_ < native_module_->FunctionCount(); ++index_) {
    if (!ReadCode()) return false;
  }
  native_module_->LinkAll();
  return data.size() - unread_.size();
}

bool NativeModuleDeserializer::ReadHeader() {
  size_t start_size = unread_.size();
  Reader reader(unread_);
  size_t functions = reader.Read<uint32_t>();
  size_t imports = reader.Read<uint32_t>();
  bool ok = functions == native_module_->FunctionCount() &&
            imports == native_module_->num_imported_functions();
  if (!ok) return false;
  size_t table_count = reader.Read<uint32_t>();

  std::vector<GlobalHandleAddress> sigs(table_count);
  std::vector<GlobalHandleAddress> funcs(table_count);
  for (size_t i = 0; i < table_count; ++i) {
    funcs[i] = reader.Read<GlobalHandleAddress>();
    sigs[i] = reader.Read<GlobalHandleAddress>();
  }
  native_module_->signature_tables() = sigs;
  native_module_->function_tables() = funcs;
  // resize, so that from here on the native module can be
  // asked about num_function_tables().
  native_module_->empty_function_tables().resize(table_count);
  native_module_->empty_signature_tables().resize(table_count);

  unread_ = unread_ + (start_size - reader.current_buffer().size());
  return true;
}

bool NativeModuleDeserializer::ReadStubs() {
  size_t start_size = unread_.size();
  Reader reader(unread_);
  size_t nr_stubs = reader.Read<uint32_t>();
  stubs_.reserve(nr_stubs);
  for (size_t i = 0; i < nr_stubs; ++i) {
    uint32_t key = reader.Read<uint32_t>();
    v8::internal::Code* stub =
        *(v8::internal::CodeStub::GetCode(isolate_, key).ToHandleChecked());
    stubs_.push_back(native_module_->GetLocalAddressFor(handle(stub)));
  }
  unread_ = unread_ + (start_size - reader.current_buffer().size());
  return true;
}

bool NativeModuleDeserializer::ReadCode() {
  size_t start_size = unread_.size();
  Reader reader(unread_);
  size_t code_section_size = reader.Read<size_t>();
  USE(code_section_size);
  size_t constant_pool_offset = reader.Read<size_t>();
  size_t safepoint_table_offset = reader.Read<size_t>();
  uint32_t stack_slot_count = reader.Read<uint32_t>();
  size_t code_size = reader.Read<size_t>();
  size_t reloc_size = reader.Read<size_t>();
  uint32_t handler_size = reader.Read<uint32_t>();
  uint32_t source_position_size = reader.Read<uint32_t>();
  size_t protected_instructions_size = reader.Read<size_t>();
  bool is_liftoff = reader.Read<bool>();
  std::shared_ptr<ProtectedInstructions> protected_instructions(
      new ProtectedInstructions(protected_instructions_size));
  DCHECK_EQ(protected_instructions_size, protected_instructions->size());

  Vector<const byte> code_buffer = reader.GetSubvector(code_size);
  std::unique_ptr<byte[]> reloc_info;
  if (reloc_size > 0) {
    reloc_info.reset(new byte[reloc_size]);
    reader.ReadIntoVector({reloc_info.get(), reloc_size});
  }
  WasmCode* ret = native_module_->AddOwnedCode(
      code_buffer, std::move(reloc_info), reloc_size, Just(index_),
      WasmCode::Function, constant_pool_offset, stack_slot_count,
      safepoint_table_offset, protected_instructions, is_liftoff);
  if (ret == nullptr) return false;
  native_module_->SetCodeTable(index_, ret);

  // now relocate the code
  int mask = RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT) |
             RelocInfo::ModeMask(RelocInfo::CODE_TARGET) |
             RelocInfo::ModeMask(RelocInfo::RUNTIME_ENTRY);
  for (RelocIterator iter(ret->instructions(), ret->reloc_info(),
                          ret->constant_pool(), mask);
       !iter.done(); iter.next()) {
    RelocInfo::Mode mode = iter.rinfo()->rmode();
    switch (mode) {
      case RelocInfo::EMBEDDED_OBJECT: {
        // We only expect {undefined}. We check for that when we add code.
        iter.rinfo()->set_target_object(isolate_->heap()->undefined_value(),
                                        SKIP_WRITE_BARRIER);
      }
      case RelocInfo::CODE_TARGET: {
        uint32_t tag = *(reinterpret_cast<uint32_t*>(
            iter.rinfo()->target_address_address()));
        Address target = GetTrampolineOrStubFromTag(tag);
        iter.rinfo()->set_target_address(nullptr, target, SKIP_WRITE_BARRIER,
                                         SKIP_ICACHE_FLUSH);
      } break;
      case RelocInfo::RUNTIME_ENTRY: {
        uint32_t orig_target = static_cast<uint32_t>(
            reinterpret_cast<intptr_t>(iter.rinfo()->target_address()));
        Address address =
            ExternalReferenceTable::instance(isolate_)->address(orig_target);
        iter.rinfo()->set_target_runtime_entry(
            nullptr, address, SKIP_WRITE_BARRIER, SKIP_ICACHE_FLUSH);
      } break;
      default:
        break;
    }
  }
  if (handler_size > 0) {
    Handle<FixedArray> handler_table = isolate_->factory()->NewFixedArray(
        static_cast<int>(handler_size), TENURED);
    reader.ReadIntoVector(
        {reinterpret_cast<Address>(handler_table->GetFirstElementAddress()),
         handler_size});
    native_module_->compiled_module()->handler_table()->set(
        static_cast<int>(index_), *handler_table);
  }
  if (source_position_size > 0) {
    Handle<ByteArray> source_positions = isolate_->factory()->NewByteArray(
        static_cast<int>(source_position_size), TENURED);
    reader.ReadIntoVector(
        {source_positions->GetDataStartAddress(), source_position_size});
    native_module_->compiled_module()->source_positions()->set(
        static_cast<int>(index_), *source_positions);
  }
  if (protected_instructions_size > 0) {
    reader.ReadIntoVector(
        {reinterpret_cast<byte*>(protected_instructions->data()),
         sizeof(trap_handler::ProtectedInstructionData) *
             protected_instructions->size()});
  }
  unread_ = unread_ + (start_size - reader.current_buffer().size());
  return true;
}

Address NativeModuleDeserializer::GetTrampolineOrStubFromTag(uint32_t tag) {
  if ((tag & 0x0000ffff) == 0) {
    int builtin_id = static_cast<int>(tag >> 16);
    v8::internal::Code* builtin = isolate_->builtins()->builtin(builtin_id);
    return native_module_->GetLocalAddressFor(handle(builtin));
  } else {
    DCHECK_EQ(tag & 0xffff0000, 0);
    return stubs_[tag];
  }
}

MaybeHandle<WasmCompiledModule> NativeModuleDeserializer::DeserializeFullBuffer(
    Isolate* isolate, Vector<const byte> data, Vector<const byte> wire_bytes) {
  if (!IsWasmCodegenAllowed(isolate, isolate->native_context())) {
    return {};
  }
  if (!WasmSerializedFormatVersion::IsSupportedVersion(isolate, data)) {
    return {};
  }
  data = data + WasmSerializedFormatVersion::GetVersionSize();
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
  // The {module_wrapper} will take ownership of the {WasmModule} object,
  // and it will be destroyed when the GC reclaims the wrapper object.
  Handle<WasmModuleWrapper> module_wrapper =
      WasmModuleWrapper::From(isolate, decode_result.val.release());
  Handle<Script> script = CreateWasmScript(isolate, wire_bytes);
  Handle<WasmSharedModuleData> shared = WasmSharedModuleData::New(
      isolate, module_wrapper, Handle<SeqOneByteString>::cast(module_bytes),
      script, Handle<ByteArray>::null());
  int export_wrappers_size =
      static_cast<int>(shared->module()->num_exported_functions);
  Handle<FixedArray> export_wrappers = isolate->factory()->NewFixedArray(
      static_cast<int>(export_wrappers_size), TENURED);

  Handle<WasmCompiledModule> compiled_module = WasmCompiledModule::New(
      isolate, shared->module(), isolate->factory()->NewFixedArray(0, TENURED),
      export_wrappers, {}, {});
  compiled_module->OnWasmModuleDecodingComplete(shared);
  NativeModuleDeserializer deserializer(isolate,
                                        compiled_module->GetNativeModule());
  if (!deserializer.Read(data)) return {};

  CompileJsToWasmWrappers(isolate, compiled_module, isolate->counters());
  WasmCompiledModule::ReinitializeAfterDeserialization(isolate,
                                                       compiled_module);
  return compiled_module;
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
