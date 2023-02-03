// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-serialization.h"

#include "src/codegen/assembler-arch.h"
#include "src/codegen/assembler-inl.h"
#include "src/debug/debug.h"
#include "src/runtime/runtime.h"
#include "src/snapshot/snapshot-data.h"
#include "src/utils/ostreams.h"
#include "src/utils/version.h"
#include "src/wasm/code-space-access.h"
#include "src/wasm/function-compiler.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-result.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace {
constexpr uint8_t kLazyFunction = 2;
constexpr uint8_t kEagerFunction = 3;
constexpr uint8_t kTurboFanFunction = 4;

// TODO(bbudge) Try to unify the various implementations of readers and writers
// in Wasm, e.g. StreamProcessor and ZoneBuffer, with these.
class Writer {
 public:
  explicit Writer(base::Vector<byte> buffer)
      : start_(buffer.begin()), end_(buffer.end()), pos_(buffer.begin()) {}

  size_t bytes_written() const { return pos_ - start_; }
  byte* current_location() const { return pos_; }
  size_t current_size() const { return end_ - pos_; }
  base::Vector<byte> current_buffer() const {
    return {current_location(), current_size()};
  }

  template <typename T>
  void Write(const T& value) {
    DCHECK_GE(current_size(), sizeof(T));
    WriteUnalignedValue(reinterpret_cast<Address>(current_location()), value);
    pos_ += sizeof(T);
    if (v8_flags.trace_wasm_serialization) {
      StdoutStream{} << "wrote: " << static_cast<size_t>(value)
                     << " sized: " << sizeof(T) << std::endl;
    }
  }

  void WriteVector(const base::Vector<const byte> v) {
    DCHECK_GE(current_size(), v.size());
    if (v.size() > 0) {
      memcpy(current_location(), v.begin(), v.size());
      pos_ += v.size();
    }
    if (v8_flags.trace_wasm_serialization) {
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
  explicit Reader(base::Vector<const byte> buffer)
      : start_(buffer.begin()), end_(buffer.end()), pos_(buffer.begin()) {}

  size_t bytes_read() const { return pos_ - start_; }
  const byte* current_location() const { return pos_; }
  size_t current_size() const { return end_ - pos_; }
  base::Vector<const byte> current_buffer() const {
    return {current_location(), current_size()};
  }

  template <typename T>
  T Read() {
    DCHECK_GE(current_size(), sizeof(T));
    T value =
        ReadUnalignedValue<T>(reinterpret_cast<Address>(current_location()));
    pos_ += sizeof(T);
    if (v8_flags.trace_wasm_serialization) {
      StdoutStream{} << "read: " << static_cast<size_t>(value)
                     << " sized: " << sizeof(T) << std::endl;
    }
    return value;
  }

  template <typename T>
  base::Vector<const T> ReadVector(size_t size) {
    DCHECK_GE(current_size(), size);
    base::Vector<const byte> bytes{pos_, size * sizeof(T)};
    pos_ += size * sizeof(T);
    if (v8_flags.trace_wasm_serialization) {
      StdoutStream{} << "read vector of " << size << " elements of size "
                     << sizeof(T) << " (total size " << size * sizeof(T) << ")"
                     << std::endl;
    }
    return base::Vector<const T>::cast(bytes);
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

constexpr size_t kHeaderSize = sizeof(size_t);  // total code size

constexpr size_t kCodeHeaderSize = sizeof(uint8_t) +  // code kind
                                   sizeof(int) +      // offset of constant pool
                                   sizeof(int) +  // offset of safepoint table
                                   sizeof(int) +  // offset of handler table
                                   sizeof(int) +  // offset of code comments
                                   sizeof(int) +  // unpadded binary size
                                   sizeof(int) +  // stack slots
                                   sizeof(int) +  // tagged parameter slots
                                   sizeof(int) +  // code size
                                   sizeof(int) +  // reloc size
                                   sizeof(int) +  // source positions size
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
  NativeModuleSerializer(const NativeModule*, base::Vector<WasmCode* const>);
  NativeModuleSerializer(const NativeModuleSerializer&) = delete;
  NativeModuleSerializer& operator=(const NativeModuleSerializer&) = delete;

  size_t Measure() const;
  bool Write(Writer* writer);

 private:
  size_t MeasureCode(const WasmCode*) const;
  void WriteHeader(Writer*, size_t total_code_size);
  void WriteCode(const WasmCode*, Writer*);

  const NativeModule* const native_module_;
  const base::Vector<WasmCode* const> code_table_;
  bool write_called_ = false;
  size_t total_written_code_ = 0;
  int num_turbofan_functions_ = 0;
};

NativeModuleSerializer::NativeModuleSerializer(
    const NativeModule* module, base::Vector<WasmCode* const> code_table)
    : native_module_(module), code_table_(code_table) {
  DCHECK_NOT_NULL(native_module_);
  // TODO(mtrofin): persist the export wrappers. Ideally, we'd only persist
  // the unique ones, i.e. the cache.
}

size_t NativeModuleSerializer::MeasureCode(const WasmCode* code) const {
  if (code == nullptr) return sizeof(uint8_t);
  DCHECK_EQ(WasmCode::kWasmFunction, code->kind());
  if (code->tier() != ExecutionTier::kTurbofan) {
    return sizeof(uint8_t);
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

void NativeModuleSerializer::WriteHeader(Writer* writer,
                                         size_t total_code_size) {
  // TODO(eholk): We need to properly preserve the flag whether the trap
  // handler was used or not when serializing.

  writer->Write(total_code_size);
}

void NativeModuleSerializer::WriteCode(const WasmCode* code, Writer* writer) {
  if (code == nullptr) {
    writer->Write(kLazyFunction);
    return;
  }

  DCHECK_EQ(WasmCode::kWasmFunction, code->kind());
  // Only serialize TurboFan code, as Liftoff code can contain breakpoints or
  // non-relocatable constants.
  if (code->tier() != ExecutionTier::kTurbofan) {
    // We check if the function has been executed already. If so, we serialize
    // it as {kEagerFunction} so that upon deserialization the function will
    // get eagerly compiled with Liftoff (if enabled). If the function has not
    // been executed yet, we serialize it as {kLazyFunction}, and the function
    // will not get compiled upon deserialization.
    NativeModule* native_module = code->native_module();
    uint32_t budget =
        native_module->tiering_budget_array()[declared_function_index(
            native_module->module(), code->index())];
    writer->Write(budget == static_cast<uint32_t>(v8_flags.wasm_tiering_budget)
                      ? kLazyFunction
                      : kEagerFunction);
    return;
  }

  ++num_turbofan_functions_;
  writer->Write(kTurboFanFunction);
  // Write the size of the entire code section, followed by the code header.
  writer->Write(code->constant_pool_offset());
  writer->Write(code->safepoint_table_offset());
  writer->Write(code->handler_table_offset());
  writer->Write(code->code_comments_offset());
  writer->Write(code->unpadded_binary_size());
  writer->Write(code->stack_slots());
  writer->Write(code->raw_tagged_parameter_slots_for_serialization());
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
#if V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_PPC ||      \
    V8_TARGET_ARCH_PPC64 || V8_TARGET_ARCH_S390X || V8_TARGET_ARCH_RISCV32 || \
    V8_TARGET_ARCH_RISCV64
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
  memcpy(code_start, code->instructions().begin(), code_size);
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
    memcpy(serialized_code_start, code_start, code_size);
  }
  total_written_code_ += code_size;
}

bool NativeModuleSerializer::Write(Writer* writer) {
  DCHECK(!write_called_);
  write_called_ = true;

  size_t total_code_size = 0;
  for (WasmCode* code : code_table_) {
    if (code && code->tier() == ExecutionTier::kTurbofan) {
      DCHECK(IsAligned(code->instructions().size(), kCodeAlignment));
      total_code_size += code->instructions().size();
    }
  }
  WriteHeader(writer, total_code_size);

  for (WasmCode* code : code_table_) {
    WriteCode(code, writer);
  }
  // If not a single function was written, serialization was not successful.
  if (num_turbofan_functions_ == 0) return false;

  // Make sure that the serialized total code size was correct.
  CHECK_EQ(total_written_code_, total_code_size);

  return true;
}

WasmSerializer::WasmSerializer(NativeModule* native_module)
    : native_module_(native_module),
      code_table_(native_module->SnapshotCodeTable()) {}

size_t WasmSerializer::GetSerializedNativeModuleSize() const {
  NativeModuleSerializer serializer(native_module_,
                                    base::VectorOf(code_table_));
  return kHeaderSize + serializer.Measure();
}

bool WasmSerializer::SerializeNativeModule(base::Vector<byte> buffer) const {
  NativeModuleSerializer serializer(native_module_,
                                    base::VectorOf(code_table_));
  size_t measured_size = kHeaderSize + serializer.Measure();
  if (buffer.size() < measured_size) return false;

  Writer writer(buffer);
  WriteHeader(&writer);

  if (!serializer.Write(&writer)) return false;
  DCHECK_EQ(measured_size, writer.bytes_written());
  return true;
}

struct DeserializationUnit {
  base::Vector<const byte> src_code_buffer;
  std::unique_ptr<WasmCode> code;
  NativeModule::JumpTablesRef jump_tables;
};

class DeserializationQueue {
 public:
  void Add(std::vector<DeserializationUnit> batch) {
    DCHECK(!batch.empty());
    base::MutexGuard guard(&mutex_);
    queue_.emplace(std::move(batch));
  }

  std::vector<DeserializationUnit> Pop() {
    base::MutexGuard guard(&mutex_);
    if (queue_.empty()) return {};
    auto batch = std::move(queue_.front());
    queue_.pop();
    return batch;
  }

  std::vector<DeserializationUnit> PopAll() {
    base::MutexGuard guard(&mutex_);
    if (queue_.empty()) return {};
    auto units = std::move(queue_.front());
    queue_.pop();
    while (!queue_.empty()) {
      units.insert(units.end(), std::make_move_iterator(queue_.front().begin()),
                   std::make_move_iterator(queue_.front().end()));
      queue_.pop();
    }
    return units;
  }

  size_t NumBatches() const {
    base::MutexGuard guard(&mutex_);
    return queue_.size();
  }

 private:
  mutable base::Mutex mutex_;
  std::queue<std::vector<DeserializationUnit>> queue_;
};

class V8_EXPORT_PRIVATE NativeModuleDeserializer {
 public:
  explicit NativeModuleDeserializer(NativeModule*);
  NativeModuleDeserializer(const NativeModuleDeserializer&) = delete;
  NativeModuleDeserializer& operator=(const NativeModuleDeserializer&) = delete;

  bool Read(Reader* reader);

  base::Vector<const int> lazy_functions() {
    return base::VectorOf(lazy_functions_);
  }

  base::Vector<const int> eager_functions() {
    return base::VectorOf(eager_functions_);
  }

 private:
  friend class DeserializeCodeTask;

  void ReadHeader(Reader* reader);
  DeserializationUnit ReadCode(int fn_index, Reader* reader);
  void CopyAndRelocate(const DeserializationUnit& unit);
  void Publish(std::vector<DeserializationUnit> batch);

  NativeModule* const native_module_;
#ifdef DEBUG
  bool read_called_ = false;
#endif

  // Updated in {ReadCode}.
  size_t remaining_code_size_ = 0;
  base::Vector<byte> current_code_space_;
  NativeModule::JumpTablesRef current_jump_tables_;
  std::vector<int> lazy_functions_;
  std::vector<int> eager_functions_;
};

class DeserializeCodeTask : public JobTask {
 public:
  DeserializeCodeTask(NativeModuleDeserializer* deserializer,
                      DeserializationQueue* reloc_queue)
      : deserializer_(deserializer), reloc_queue_(reloc_queue) {}

  void Run(JobDelegate* delegate) override {
    CodeSpaceWriteScope code_space_write_scope(deserializer_->native_module_);
    bool finished = false;
    while (!finished) {
      // Repeatedly publish everything that was copied already.
      finished = TryPublishing(delegate);

      auto batch = reloc_queue_->Pop();
      if (batch.empty()) break;
      for (const auto& unit : batch) {
        deserializer_->CopyAndRelocate(unit);
      }
      publish_queue_.Add(std::move(batch));
      ResetPKUPermissionsForThreadSpawning pku_reset_scope;
      delegate->NotifyConcurrencyIncrease();
    }
  }

  size_t GetMaxConcurrency(size_t /* worker_count */) const override {
    // Number of copy&reloc batches, plus 1 if there is also something to
    // publish.
    bool publish = publishing_.load(std::memory_order_relaxed) == false &&
                   publish_queue_.NumBatches() > 0;
    return reloc_queue_->NumBatches() + (publish ? 1 : 0);
  }

 private:
  bool TryPublishing(JobDelegate* delegate) {
    // Publishing is sequential, so only start publishing if no one else is.
    if (publishing_.exchange(true, std::memory_order_relaxed)) return false;

    WasmCodeRefScope code_scope;
    while (true) {
      bool yield = false;
      while (!yield) {
        auto to_publish = publish_queue_.PopAll();
        if (to_publish.empty()) break;
        deserializer_->Publish(std::move(to_publish));
        yield = delegate->ShouldYield();
      }
      publishing_.store(false, std::memory_order_relaxed);
      if (yield) return true;
      // After finishing publishing, check again if new work arrived in the mean
      // time. If so, continue publishing.
      if (publish_queue_.NumBatches() == 0) break;
      if (publishing_.exchange(true, std::memory_order_relaxed)) break;
      // We successfully reset {publishing_} from {false} to {true}.
    }
    return false;
  }

  NativeModuleDeserializer* const deserializer_;
  DeserializationQueue* const reloc_queue_;
  DeserializationQueue publish_queue_;
  std::atomic<bool> publishing_{false};
};

NativeModuleDeserializer::NativeModuleDeserializer(NativeModule* native_module)
    : native_module_(native_module) {}

bool NativeModuleDeserializer::Read(Reader* reader) {
  DCHECK(!read_called_);
#ifdef DEBUG
  read_called_ = true;
#endif

  ReadHeader(reader);
  uint32_t total_fns = native_module_->num_functions();
  uint32_t first_wasm_fn = native_module_->num_imported_functions();

  WasmCodeRefScope wasm_code_ref_scope;

  DeserializationQueue reloc_queue;

  // Create a new job without any workers; those are spawned on
  // {NotifyConcurrencyIncrease}.
  std::unique_ptr<JobHandle> job_handle = V8::GetCurrentPlatform()->CreateJob(
      TaskPriority::kUserVisible,
      std::make_unique<DeserializeCodeTask>(this, &reloc_queue));

  // Choose a batch size such that we do not create too small batches (>=100k
  // code bytes), but also not too many (<=100 batches).
  constexpr size_t kMinBatchSizeInBytes = 100000;
  size_t batch_limit =
      std::max(kMinBatchSizeInBytes, remaining_code_size_ / 100);

  std::vector<DeserializationUnit> batch;
  size_t batch_size = 0;
  CodeSpaceWriteScope code_space_write_scope(native_module_);
  for (uint32_t i = first_wasm_fn; i < total_fns; ++i) {
    DeserializationUnit unit = ReadCode(i, reader);
    if (!unit.code) continue;
    batch_size += unit.code->instructions().size();
    batch.emplace_back(std::move(unit));
    if (batch_size >= batch_limit) {
      reloc_queue.Add(std::move(batch));
      DCHECK(batch.empty());
      batch_size = 0;
      ResetPKUPermissionsForThreadSpawning pku_reset_scope;
      job_handle->NotifyConcurrencyIncrease();
    }
  }

  // We should have read the expected amount of code now, and should have fully
  // utilized the allocated code space.
  DCHECK_EQ(0, remaining_code_size_);
  DCHECK_EQ(0, current_code_space_.size());

  if (!batch.empty()) {
    reloc_queue.Add(std::move(batch));
    ResetPKUPermissionsForThreadSpawning pku_reset_scope;
    job_handle->NotifyConcurrencyIncrease();
  }

  // Wait for all tasks to finish, while participating in their work.
  job_handle->Join();

  return reader->current_size() == 0;
}

void NativeModuleDeserializer::ReadHeader(Reader* reader) {
  remaining_code_size_ = reader->Read<size_t>();
}

DeserializationUnit NativeModuleDeserializer::ReadCode(int fn_index,
                                                       Reader* reader) {
  uint8_t code_kind = reader->Read<uint8_t>();
  if (code_kind == kLazyFunction) {
    lazy_functions_.push_back(fn_index);
    return {};
  }
  if (code_kind == kEagerFunction) {
    eager_functions_.push_back(fn_index);
    return {};
  }

  int constant_pool_offset = reader->Read<int>();
  int safepoint_table_offset = reader->Read<int>();
  int handler_table_offset = reader->Read<int>();
  int code_comment_offset = reader->Read<int>();
  int unpadded_binary_size = reader->Read<int>();
  int stack_slot_count = reader->Read<int>();
  uint32_t tagged_parameter_slots = reader->Read<uint32_t>();
  int code_size = reader->Read<int>();
  int reloc_size = reader->Read<int>();
  int source_position_size = reader->Read<int>();
  int protected_instructions_size = reader->Read<int>();
  WasmCode::Kind kind = reader->Read<WasmCode::Kind>();
  ExecutionTier tier = reader->Read<ExecutionTier>();

  DCHECK(IsAligned(code_size, kCodeAlignment));
  DCHECK_GE(remaining_code_size_, code_size);
  if (current_code_space_.size() < static_cast<size_t>(code_size)) {
    // Allocate the next code space. Don't allocate more than 90% of
    // {kMaxCodeSpaceSize}, to leave some space for jump tables.
    constexpr size_t kMaxReservation =
        RoundUp<kCodeAlignment>(WasmCodeAllocator::kMaxCodeSpaceSize * 9 / 10);
    size_t code_space_size = std::min(kMaxReservation, remaining_code_size_);
    std::tie(current_code_space_, current_jump_tables_) =
        native_module_->AllocateForDeserializedCode(code_space_size);
    DCHECK_EQ(current_code_space_.size(), code_space_size);
    DCHECK(current_jump_tables_.is_valid());
  }

  DeserializationUnit unit;
  unit.src_code_buffer = reader->ReadVector<byte>(code_size);
  auto reloc_info = reader->ReadVector<byte>(reloc_size);
  auto source_pos = reader->ReadVector<byte>(source_position_size);
  auto protected_instructions =
      reader->ReadVector<byte>(protected_instructions_size);

  base::Vector<uint8_t> instructions =
      current_code_space_.SubVector(0, code_size);
  current_code_space_ += code_size;
  remaining_code_size_ -= code_size;

  unit.code = native_module_->AddDeserializedCode(
      fn_index, instructions, stack_slot_count, tagged_parameter_slots,
      safepoint_table_offset, handler_table_offset, constant_pool_offset,
      code_comment_offset, unpadded_binary_size, protected_instructions,
      reloc_info, source_pos, kind, tier);
  unit.jump_tables = current_jump_tables_;
  return unit;
}

void NativeModuleDeserializer::CopyAndRelocate(
    const DeserializationUnit& unit) {
  memcpy(unit.code->instructions().begin(), unit.src_code_buffer.begin(),
         unit.src_code_buffer.size());

  // Relocate the code.
  int mask = RelocInfo::ModeMask(RelocInfo::WASM_CALL) |
             RelocInfo::ModeMask(RelocInfo::WASM_STUB_CALL) |
             RelocInfo::ModeMask(RelocInfo::EXTERNAL_REFERENCE) |
             RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE) |
             RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE_ENCODED);
  for (RelocIterator iter(unit.code->instructions(), unit.code->reloc_info(),
                          unit.code->constant_pool(), mask);
       !iter.done(); iter.next()) {
    RelocInfo::Mode mode = iter.rinfo()->rmode();
    switch (mode) {
      case RelocInfo::WASM_CALL: {
        uint32_t tag = GetWasmCalleeTag(iter.rinfo());
        Address target =
            native_module_->GetNearCallTargetForFunction(tag, unit.jump_tables);
        iter.rinfo()->set_wasm_call_address(target, SKIP_ICACHE_FLUSH);
        break;
      }
      case RelocInfo::WASM_STUB_CALL: {
        uint32_t tag = GetWasmCalleeTag(iter.rinfo());
        DCHECK_LT(tag, WasmCode::kRuntimeStubCount);
        Address target = native_module_->GetNearRuntimeStubEntry(
            static_cast<WasmCode::RuntimeStubId>(tag), unit.jump_tables);
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

void NativeModuleDeserializer::Publish(std::vector<DeserializationUnit> batch) {
  DCHECK(!batch.empty());
  std::vector<std::unique_ptr<WasmCode>> codes;
  codes.reserve(batch.size());
  for (auto& unit : batch) {
    codes.emplace_back(std::move(unit).code);
  }
  auto published_codes = native_module_->PublishCode(base::VectorOf(codes));
  for (auto* wasm_code : published_codes) {
    wasm_code->MaybePrint();
    wasm_code->Validate();
  }
}

bool IsSupportedVersion(base::Vector<const byte> header) {
  if (header.size() < WasmSerializer::kHeaderSize) return false;
  byte current_version[WasmSerializer::kHeaderSize];
  Writer writer({current_version, WasmSerializer::kHeaderSize});
  WriteHeader(&writer);
  return memcmp(header.begin(), current_version, WasmSerializer::kHeaderSize) ==
         0;
}

MaybeHandle<WasmModuleObject> DeserializeNativeModule(
    Isolate* isolate, base::Vector<const byte> data,
    base::Vector<const byte> wire_bytes_vec,
    base::Vector<const char> source_url) {
  if (!IsWasmCodegenAllowed(isolate, isolate->native_context())) return {};
  if (!IsSupportedVersion(data)) return {};

  // Make the copy of the wire bytes early, so we use the same memory for
  // decoding, lookup in the native module cache, and insertion into the cache.
  auto owned_wire_bytes = base::OwnedVector<uint8_t>::Of(wire_bytes_vec);

  // TODO(titzer): module features should be part of the serialization format.
  WasmEngine* wasm_engine = GetWasmEngine();
  WasmFeatures enabled_features = WasmFeatures::FromIsolate(isolate);
  ModuleResult decode_result = DecodeWasmModule(
      enabled_features, owned_wire_bytes.start(), owned_wire_bytes.end(), false,
      i::wasm::kWasmOrigin, isolate->counters(), isolate->metrics_recorder(),
      isolate->GetOrRegisterRecorderContextId(isolate->native_context()),
      DecodingMethod::kDeserialize, wasm_engine->allocator());
  if (decode_result.failed()) return {};
  std::shared_ptr<WasmModule> module = std::move(decode_result).value();
  CHECK_NOT_NULL(module);

  auto shared_native_module = wasm_engine->MaybeGetNativeModule(
      module->origin, owned_wire_bytes.as_vector(), isolate);
  if (shared_native_module == nullptr) {
    const bool dynamic_tiering = v8_flags.wasm_dynamic_tiering;
    const bool include_liftoff = !dynamic_tiering;
    size_t code_size_estimate =
        wasm::WasmCodeManager::EstimateNativeModuleCodeSize(
            module.get(), include_liftoff, DynamicTiering{dynamic_tiering});
    shared_native_module = wasm_engine->NewNativeModule(
        isolate, enabled_features, std::move(module), code_size_estimate);
    // We have to assign a compilation ID here, as it is required for a
    // potential re-compilation, e.g. triggered by
    // {TierDownAllModulesPerIsolate}. The value is -2 so that it is different
    // than the compilation ID of actual compilations, and also different than
    // the sentinel value of the CompilationState.
    shared_native_module->compilation_state()->set_compilation_id(-2);
    shared_native_module->SetWireBytes(std::move(owned_wire_bytes));

    NativeModuleDeserializer deserializer(shared_native_module.get());
    Reader reader(data + WasmSerializer::kHeaderSize);
    bool error = !deserializer.Read(&reader);
    if (error) {
      wasm_engine->UpdateNativeModuleCache(error, &shared_native_module,
                                           isolate);
      return {};
    }
    shared_native_module->compilation_state()->InitializeAfterDeserialization(
        deserializer.lazy_functions(), deserializer.eager_functions());
    wasm_engine->UpdateNativeModuleCache(error, &shared_native_module, isolate);
  }

  Handle<Script> script =
      wasm_engine->GetOrCreateScript(isolate, shared_native_module, source_url);
  Handle<WasmModuleObject> module_object =
      WasmModuleObject::New(isolate, shared_native_module, script);

  // Finish the Wasm script now and make it public to the debugger.
  isolate->debug()->OnAfterCompile(script);

  // Log the code within the generated module for profiling.
  shared_native_module->LogWasmCodes(isolate, *script);

  return module_object;
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
