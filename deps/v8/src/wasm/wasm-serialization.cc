// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-serialization.h"

#include "src/base/platform/wrappers.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/external-reference-table.h"
#include "src/objects/objects-inl.h"
#include "src/objects/objects.h"
#include "src/runtime/runtime.h"
#include "src/snapshot/code-serializer.h"
#include "src/utils/ostreams.h"
#include "src/utils/utils.h"
#include "src/utils/version.h"
#include "src/wasm/code-space-access.h"
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
// in Wasm, e.g. StreamProcessor and ZoneBuffer, with these.
class Writer {
 public:
  explicit Writer(Vector<byte> buffer)
      : start_(buffer.begin()), end_(buffer.end()), pos_(buffer.begin()) {}

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
      base::Memcpy(current_location(), v.begin(), v.size());
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
      : start_(buffer.begin()), end_(buffer.end()), pos_(buffer.begin()) {}

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

  template <typename T>
  Vector<const T> ReadVector(size_t size) {
    DCHECK_GE(current_size(), size);
    Vector<const byte> bytes{pos_, size * sizeof(T)};
    pos_ += size * sizeof(T);
    if (FLAG_trace_wasm_serialization) {
      StdoutStream{} << "read vector of " << size << " elements of size "
                     << sizeof(T) << " (total size " << size * sizeof(T) << ")"
                     << std::endl;
    }
    return Vector<const T>::cast(bytes);
  }

  void Skip(size_t size) { pos_ += size; }

 private:
  const byte* const start_;
  const byte* const end_;
  const byte* pos_;
};

void WriteHeader(Writer* writer) {
  writer->Write(SerializedData::kMagicNumber);
  writer->Write(Version::Hash());
  writer->Write(static_cast<uint32_t>(CpuFeatures::SupportedFeatures()));
  writer->Write(FlagList::Hash());
  DCHECK_EQ(WasmSerializer::kHeaderSize, writer->bytes_written());
}

// On Intel, call sites are encoded as a displacement. For linking and for
// serialization/deserialization, we want to store/retrieve a tag (the function
// index). On Intel, that means accessing the raw displacement.
// On ARM64, call sites are encoded as either a literal load or a direct branch.
// Other platforms simply require accessing the target address.
void SetWasmCalleeTag(RelocInfo* rinfo, uint32_t tag) {
#if V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_IA32
  DCHECK(rinfo->HasTargetAddressAddress());
  DCHECK(!RelocInfo::IsCompressedEmbeddedObject(rinfo->rmode()));
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
  DCHECK(!RelocInfo::IsCompressedEmbeddedObject(rinfo->rmode()));
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

constexpr size_t kCodeHeaderSize = sizeof(bool) +  // whether code is present
                                   sizeof(int) +   // offset of constant pool
                                   sizeof(int) +   // offset of safepoint table
                                   sizeof(int) +   // offset of handler table
                                   sizeof(int) +   // offset of code comments
                                   sizeof(int) +   // unpadded binary size
                                   sizeof(int) +   // stack slots
                                   sizeof(int) +   // tagged parameter slots
                                   sizeof(int) +   // code size
                                   sizeof(int) +   // reloc size
                                   sizeof(int) +   // source positions size
                                   sizeof(int) +  // protected instructions size
                                   sizeof(WasmCode::Kind) +  // code kind
                                   sizeof(ExecutionTier);    // tier

// A List of all isolate-independent external references. This is used to create
// a tag from the Address of an external reference and vice versa.
class ExternalReferenceList {
 public:
  ExternalReferenceList(const ExternalReferenceList&) = delete;
  ExternalReferenceList& operator=(const ExternalReferenceList&) = delete;

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
};

static_assert(std::is_trivially_destructible<ExternalReferenceList>::value,
              "static destructors not allowed");

}  // namespace

class V8_EXPORT_PRIVATE NativeModuleSerializer {
 public:
  NativeModuleSerializer(const NativeModule*, Vector<WasmCode* const>);
  NativeModuleSerializer(const NativeModuleSerializer&) = delete;
  NativeModuleSerializer& operator=(const NativeModuleSerializer&) = delete;

  size_t Measure() const;
  bool Write(Writer* writer);

 private:
  size_t MeasureCode(const WasmCode*) const;
  void WriteHeader(Writer*);
  bool WriteCode(const WasmCode*, Writer*);

  const NativeModule* const native_module_;
  Vector<WasmCode* const> code_table_;
  bool write_called_;
};

NativeModuleSerializer::NativeModuleSerializer(
    const NativeModule* module, Vector<WasmCode* const> code_table)
    : native_module_(module), code_table_(code_table), write_called_(false) {
  DCHECK_NOT_NULL(native_module_);
  // TODO(mtrofin): persist the export wrappers. Ideally, we'd only persist
  // the unique ones, i.e. the cache.
}

size_t NativeModuleSerializer::MeasureCode(const WasmCode* code) const {
  if (code == nullptr) return sizeof(bool);
  DCHECK_EQ(WasmCode::kFunction, code->kind());
  if (FLAG_wasm_lazy_compilation && code->tier() != ExecutionTier::kTurbofan) {
    return sizeof(bool);
  }
  return kCodeHeaderSize + code->instructions().size() +
         code->reloc_info().size() + code->source_positions().size() +
         code->protected_instructions_data().size();
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

bool NativeModuleSerializer::WriteCode(const WasmCode* code, Writer* writer) {
  DCHECK_IMPLIES(!FLAG_wasm_lazy_compilation, code != nullptr);
  if (code == nullptr) {
    writer->Write(false);
    return true;
  }
  DCHECK_EQ(WasmCode::kFunction, code->kind());
  // Only serialize TurboFan code, as Liftoff code can contain breakpoints or
  // non-relocatable constants.
  if (code->tier() != ExecutionTier::kTurbofan) {
    if (FLAG_wasm_lazy_compilation) {
      writer->Write(false);
      return true;
    }
    return false;
  }
  writer->Write(true);
  // Write the size of the entire code section, followed by the code header.
  writer->Write(code->constant_pool_offset());
  writer->Write(code->safepoint_table_offset());
  writer->Write(code->handler_table_offset());
  writer->Write(code->code_comments_offset());
  writer->Write(code->unpadded_binary_size());
  writer->Write(code->stack_slots());
  writer->Write(code->tagged_parameter_slots());
  writer->Write(code->instructions().length());
  writer->Write(code->reloc_info().length());
  writer->Write(code->source_positions().length());
  writer->Write(code->protected_instructions_data().length());
  writer->Write(code->kind());
  writer->Write(code->tier());

  // Get a pointer to the destination buffer, to hold relocated code.
  byte* serialized_code_start = writer->current_buffer().begin();
  byte* code_start = serialized_code_start;
  size_t code_size = code->instructions().size();
  writer->Skip(code_size);
  // Write the reloc info, source positions, and protected code.
  writer->WriteVector(code->reloc_info());
  writer->WriteVector(code->source_positions());
  writer->WriteVector(code->protected_instructions_data());
#if V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_ARM || \
    V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_PPC64 || V8_TARGET_ARCH_S390X
  // On platforms that don't support misaligned word stores, copy to an aligned
  // buffer if necessary so we can relocate the serialized code.
  std::unique_ptr<byte[]> aligned_buffer;
  if (!IsAligned(reinterpret_cast<Address>(serialized_code_start),
                 kSystemPointerSize)) {
    // 'byte' does not guarantee an alignment but seems to work well enough in
    // practice.
    aligned_buffer.reset(new byte[code_size]);
    code_start = aligned_buffer.get();
  }
#endif
  base::Memcpy(code_start, code->instructions().begin(), code_size);
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
        Address target = orig_iter.rinfo()->wasm_stub_call_address();
        uint32_t tag = native_module_->GetRuntimeStubId(target);
        DCHECK_GT(WasmCode::kRuntimeStubCount, tag);
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
    base::Memcpy(serialized_code_start, code_start, code_size);
  }
  return true;
}

bool NativeModuleSerializer::Write(Writer* writer) {
  DCHECK(!write_called_);
  write_called_ = true;

  WriteHeader(writer);

  for (WasmCode* code : code_table_) {
    if (!WriteCode(code, writer)) return false;
  }
  return true;
}

WasmSerializer::WasmSerializer(NativeModule* native_module)
    : native_module_(native_module),
      code_table_(native_module->SnapshotCodeTable()) {}

size_t WasmSerializer::GetSerializedNativeModuleSize() const {
  NativeModuleSerializer serializer(native_module_, VectorOf(code_table_));
  return kHeaderSize + serializer.Measure();
}

bool WasmSerializer::SerializeNativeModule(Vector<byte> buffer) const {
  NativeModuleSerializer serializer(native_module_, VectorOf(code_table_));
  size_t measured_size = kHeaderSize + serializer.Measure();
  if (buffer.size() < measured_size) return false;

  Writer writer(buffer);
  WriteHeader(&writer);

  if (!serializer.Write(&writer)) return false;
  DCHECK_EQ(measured_size, writer.bytes_written());
  return true;
}

struct DeserializationUnit {
  Vector<const byte> src_code_buffer;
  std::unique_ptr<WasmCode> code;
};

class DeserializationQueue {
 public:
  void Add(std::unique_ptr<std::vector<DeserializationUnit>> batch) {
    base::MutexGuard guard(&mutex_);
    queue_.push(std::move(batch));
    cv_.NotifyOne();
  }

  std::unique_ptr<std::vector<DeserializationUnit>> Pop() {
    base::MutexGuard guard(&mutex_);
    while (queue_.empty()) {
      cv_.Wait(&mutex_);
    }
    auto batch = std::move(queue_.front());
    if (batch) queue_.pop();
    return batch;
  }

  std::unique_ptr<std::vector<DeserializationUnit>> UnlockedPop() {
    DCHECK(!queue_.empty());
    auto batch = std::move(queue_.front());
    queue_.pop();
    return batch;
  }

 private:
  base::Mutex mutex_;
  base::ConditionVariable cv_;
  std::queue<std::unique_ptr<std::vector<DeserializationUnit>>> queue_;
};

class V8_EXPORT_PRIVATE NativeModuleDeserializer {
 public:
  explicit NativeModuleDeserializer(NativeModule*);
  NativeModuleDeserializer(const NativeModuleDeserializer&) = delete;
  NativeModuleDeserializer& operator=(const NativeModuleDeserializer&) = delete;

  bool Read(Reader* reader);

 private:
  friend class CopyAndRelocTask;
  friend class PublishTask;

  bool ReadHeader(Reader* reader);
  DeserializationUnit ReadCodeAndAlloc(int fn_index, Reader* reader);
  void CopyAndRelocate(const DeserializationUnit& unit);
  void Publish(std::unique_ptr<std::vector<DeserializationUnit>> batch);

  NativeModule* const native_module_;
  bool read_called_;
#ifdef DEBUG
  std::atomic<int> total_published_{0};
#endif
};

class CopyAndRelocTask : public CancelableTask {
 public:
  CopyAndRelocTask(NativeModuleDeserializer* deserializer,
                   DeserializationQueue& from_queue,
                   DeserializationQueue& to_queue,
                   CancelableTaskManager* task_manager)
      : CancelableTask(task_manager),
        deserializer_(deserializer),
        from_queue_(from_queue),
        to_queue_(to_queue) {}

  void RunInternal() override {
    CODE_SPACE_WRITE_SCOPE
    for (;;) {
      auto batch = from_queue_.Pop();
      if (!batch) break;
      for (auto& unit : *batch) {
        deserializer_->CopyAndRelocate(unit);
      }
      to_queue_.Add(std::move(batch));
    }
    to_queue_.Add(nullptr);
  }

 private:
  NativeModuleDeserializer* deserializer_;
  DeserializationQueue& from_queue_;
  DeserializationQueue& to_queue_;
};

class PublishTask : public CancelableTask {
 public:
  PublishTask(NativeModuleDeserializer* deserializer,
              DeserializationQueue& from_queue,
              CancelableTaskManager* task_manager)
      : CancelableTask(task_manager),
        deserializer_(deserializer),
        from_queue_(from_queue) {}

  void RunInternal() override {
    WasmCodeRefScope code_scope;
    for (;;) {
      auto batch = from_queue_.Pop();
      if (!batch) break;
      deserializer_->Publish(std::move(batch));
    }
  }

 private:
  NativeModuleDeserializer* deserializer_;
  DeserializationQueue& from_queue_;
};

NativeModuleDeserializer::NativeModuleDeserializer(NativeModule* native_module)
    : native_module_(native_module), read_called_(false) {}

bool NativeModuleDeserializer::Read(Reader* reader) {
  DCHECK(!read_called_);
  read_called_ = true;

  if (!ReadHeader(reader)) return false;
  uint32_t total_fns = native_module_->num_functions();
  uint32_t first_wasm_fn = native_module_->num_imported_functions();
  WasmCodeRefScope wasm_code_ref_scope;

  DeserializationQueue reloc_queue;
  DeserializationQueue publish_queue;

  CancelableTaskManager cancelable_task_manager;

  auto copy_task = std::make_unique<CopyAndRelocTask>(
      this, reloc_queue, publish_queue, &cancelable_task_manager);
  V8::GetCurrentPlatform()->CallOnWorkerThread(std::move(copy_task));

  auto publish_task = std::make_unique<PublishTask>(this, publish_queue,
                                                    &cancelable_task_manager);
  V8::GetCurrentPlatform()->CallOnWorkerThread(std::move(publish_task));

  auto batch = std::make_unique<std::vector<DeserializationUnit>>();
  int num_batches = 0;
  const byte* batch_start = reader->current_location();
  for (uint32_t i = first_wasm_fn; i < total_fns; ++i) {
    DeserializationUnit unit = ReadCodeAndAlloc(i, reader);
    if (unit.code) {
      batch->push_back(std::move(unit));
    }
    uint64_t batch_size_in_bytes = reader->current_location() - batch_start;
    constexpr int kMinBatchSizeInBytes = 100000;
    if (batch_size_in_bytes >= kMinBatchSizeInBytes) {
      reloc_queue.Add(std::move(batch));
      num_batches++;
      batch = std::make_unique<std::vector<DeserializationUnit>>();
      batch_start = reader->current_location();
    }
  }

  if (!batch->empty()) {
    reloc_queue.Add(std::move(batch));
    num_batches++;
  }
  reloc_queue.Add(nullptr);

  // Participate to deserialization in the main thread to ensure progress even
  // if background tasks are not scheduled.
  int published = 0;
  {
    CODE_SPACE_WRITE_SCOPE
    for (;;) {
      auto batch = reloc_queue.Pop();
      if (!batch) break;
      for (auto& unit : *batch) {
        CopyAndRelocate(unit);
      }
      Publish(std::move(batch));
      ++published;
    }
  }

  if (published == num_batches) {
    // {CopyAndRelocTask} did not take any work from the reloc queue, probably
    // because it was not scheduled yet. Ensure that the end marker gets added
    // to the queue in this case.
    publish_queue.Add(nullptr);
  }
  cancelable_task_manager.CancelAndWait();

  // Process the publish queue now in case {PublishTask} was canceled.
  for (;;) {
    auto batch = publish_queue.UnlockedPop();
    if (!batch) break;
    Publish(std::move(batch));
  }
  DCHECK_EQ(total_published_.load(), num_batches);
  return reader->current_size() == 0;
}

bool NativeModuleDeserializer::ReadHeader(Reader* reader) {
  size_t functions = reader->Read<uint32_t>();
  size_t imports = reader->Read<uint32_t>();
  return functions == native_module_->num_functions() &&
         imports == native_module_->num_imported_functions();
}

DeserializationUnit NativeModuleDeserializer::ReadCodeAndAlloc(int fn_index,
                                                               Reader* reader) {
  bool has_code = reader->Read<bool>();
  if (!has_code) {
    DCHECK(FLAG_wasm_lazy_compilation ||
           native_module_->enabled_features().has_compilation_hints());
    native_module_->UseLazyStub(fn_index);
    return {{}, nullptr};
  }
  int constant_pool_offset = reader->Read<int>();
  int safepoint_table_offset = reader->Read<int>();
  int handler_table_offset = reader->Read<int>();
  int code_comment_offset = reader->Read<int>();
  int unpadded_binary_size = reader->Read<int>();
  int stack_slot_count = reader->Read<int>();
  int tagged_parameter_slots = reader->Read<int>();
  int code_size = reader->Read<int>();
  int reloc_size = reader->Read<int>();
  int source_position_size = reader->Read<int>();
  int protected_instructions_size = reader->Read<int>();
  WasmCode::Kind kind = reader->Read<WasmCode::Kind>();
  ExecutionTier tier = reader->Read<ExecutionTier>();

  DeserializationUnit unit;
  unit.src_code_buffer = reader->ReadVector<byte>(code_size);
  auto reloc_info = reader->ReadVector<byte>(reloc_size);
  auto source_pos = reader->ReadVector<byte>(source_position_size);
  auto protected_instructions =
      reader->ReadVector<byte>(protected_instructions_size);
  unit.code = native_module_->AllocateDeserializedCode(
      fn_index, unit.src_code_buffer, stack_slot_count, tagged_parameter_slots,
      safepoint_table_offset, handler_table_offset, constant_pool_offset,
      code_comment_offset, unpadded_binary_size, protected_instructions,
      reloc_info, source_pos, kind, tier);
  return unit;
}

void NativeModuleDeserializer::CopyAndRelocate(
    const DeserializationUnit& unit) {
  base::Memcpy(unit.code->instructions().begin(), unit.src_code_buffer.begin(),
               unit.src_code_buffer.size());

  // Relocate the code.
  int mask = RelocInfo::ModeMask(RelocInfo::WASM_CALL) |
             RelocInfo::ModeMask(RelocInfo::WASM_STUB_CALL) |
             RelocInfo::ModeMask(RelocInfo::EXTERNAL_REFERENCE) |
             RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE) |
             RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE_ENCODED);
  auto jump_tables_ref = native_module_->FindJumpTablesForRegion(
      base::AddressRegionOf(unit.code->instructions()));
  for (RelocIterator iter(unit.code->instructions(), unit.code->reloc_info(),
                          unit.code->constant_pool(), mask);
       !iter.done(); iter.next()) {
    RelocInfo::Mode mode = iter.rinfo()->rmode();
    switch (mode) {
      case RelocInfo::WASM_CALL: {
        uint32_t tag = GetWasmCalleeTag(iter.rinfo());
        Address target =
            native_module_->GetNearCallTargetForFunction(tag, jump_tables_ref);
        iter.rinfo()->set_wasm_call_address(target, SKIP_ICACHE_FLUSH);
        break;
      }
      case RelocInfo::WASM_STUB_CALL: {
        uint32_t tag = GetWasmCalleeTag(iter.rinfo());
        DCHECK_LT(tag, WasmCode::kRuntimeStubCount);
        Address target = native_module_->GetNearRuntimeStubEntry(
            static_cast<WasmCode::RuntimeStubId>(tag), jump_tables_ref);
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
        Address target = unit.code->instruction_start() + offset;
        Assembler::deserialization_set_target_internal_reference_at(
            iter.rinfo()->pc(), target, mode);
        break;
      }
      default:
        UNREACHABLE();
    }
  }

  // Finally, flush the icache for that code.
  FlushInstructionCache(unit.code->instructions().begin(),
                        unit.code->instructions().size());
}

void NativeModuleDeserializer::Publish(
    std::unique_ptr<std::vector<DeserializationUnit>> batch) {
  DCHECK_NOT_NULL(batch);
  std::vector<std::unique_ptr<WasmCode>> codes;
  for (auto& unit : *batch) {
    codes.push_back(std::move(unit).code);
  }
  auto published_codes = native_module_->PublishCode(VectorOf(codes));
  for (auto* wasm_code : published_codes) {
    wasm_code->MaybePrint();
    wasm_code->Validate();
  }
#ifdef DEBUG
  total_published_.fetch_add(1);
#endif
}

bool IsSupportedVersion(Vector<const byte> header) {
  if (header.size() < WasmSerializer::kHeaderSize) return false;
  byte current_version[WasmSerializer::kHeaderSize];
  Writer writer({current_version, WasmSerializer::kHeaderSize});
  WriteHeader(&writer);
  return memcmp(header.begin(), current_version, WasmSerializer::kHeaderSize) ==
         0;
}

MaybeHandle<WasmModuleObject> DeserializeNativeModule(
    Isolate* isolate, Vector<const byte> data,
    Vector<const byte> wire_bytes_vec, Vector<const char> source_url) {
  if (!IsWasmCodegenAllowed(isolate, isolate->native_context())) return {};
  if (!IsSupportedVersion(data)) return {};

  ModuleWireBytes wire_bytes(wire_bytes_vec);
  // TODO(titzer): module features should be part of the serialization format.
  WasmEngine* wasm_engine = isolate->wasm_engine();
  WasmFeatures enabled_features = WasmFeatures::FromIsolate(isolate);
  ModuleResult decode_result = DecodeWasmModule(
      enabled_features, wire_bytes.start(), wire_bytes.end(), false,
      i::wasm::kWasmOrigin, isolate->counters(), isolate->metrics_recorder(),
      isolate->GetOrRegisterRecorderContextId(isolate->native_context()),
      DecodingMethod::kDeserialize, wasm_engine->allocator());
  if (decode_result.failed()) return {};
  std::shared_ptr<WasmModule> module = std::move(decode_result).value();
  CHECK_NOT_NULL(module);

  auto shared_native_module = wasm_engine->MaybeGetNativeModule(
      module->origin, wire_bytes_vec, isolate);
  if (shared_native_module == nullptr) {
    const bool kIncludeLiftoff = false;
    size_t code_size_estimate =
        wasm::WasmCodeManager::EstimateNativeModuleCodeSize(module.get(),
                                                            kIncludeLiftoff);
    shared_native_module = wasm_engine->NewNativeModule(
        isolate, enabled_features, std::move(module), code_size_estimate);
    shared_native_module->SetWireBytes(
        OwnedVector<uint8_t>::Of(wire_bytes_vec));

    NativeModuleDeserializer deserializer(shared_native_module.get());
    Reader reader(data + WasmSerializer::kHeaderSize);
    bool error = !deserializer.Read(&reader);
    shared_native_module->compilation_state()->InitializeAfterDeserialization();
    wasm_engine->UpdateNativeModuleCache(error, &shared_native_module, isolate);
    if (error) return {};
  }

  Handle<FixedArray> export_wrappers;
  CompileJsToWasmWrappers(isolate, shared_native_module->module(),
                          &export_wrappers);

  Handle<Script> script =
      wasm_engine->GetOrCreateScript(isolate, shared_native_module, source_url);
  Handle<WasmModuleObject> module_object = WasmModuleObject::New(
      isolate, shared_native_module, script, export_wrappers);

  // Finish the Wasm script now and make it public to the debugger.
  isolate->debug()->OnAfterCompile(script);

  // Log the code within the generated module for profiling.
  shared_native_module->LogWasmCodes(isolate, *script);

  return module_object;
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
