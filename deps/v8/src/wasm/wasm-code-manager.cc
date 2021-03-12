// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-code-manager.h"

#include <iomanip>

#include "src/base/build_config.h"
#include "src/base/iterator.h"
#include "src/base/macros.h"
#include "src/base/platform/platform.h"
#include "src/base/small-vector.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/macro-assembler-inl.h"
#include "src/codegen/macro-assembler.h"
#include "src/common/globals.h"
#include "src/diagnostics/disassembler.h"
#include "src/logging/counters.h"
#include "src/logging/log.h"
#include "src/objects/objects-inl.h"
#include "src/snapshot/embedded/embedded-data.h"
#include "src/utils/ostreams.h"
#include "src/utils/vector.h"
#include "src/wasm/code-space-access.h"
#include "src/wasm/compilation-environment.h"
#include "src/wasm/function-compiler.h"
#include "src/wasm/jump-table-assembler.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/wasm-debug.h"
#include "src/wasm/wasm-import-wrapper-cache.h"
#include "src/wasm/wasm-module-sourcemap.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-objects.h"

#if defined(V8_OS_WIN64)
#include "src/base/platform/wrappers.h"
#include "src/diagnostics/unwinding-info-win64.h"
#endif  // V8_OS_WIN64

#define TRACE_HEAP(...)                                   \
  do {                                                    \
    if (FLAG_trace_wasm_native_heap) PrintF(__VA_ARGS__); \
  } while (false)

namespace v8 {
namespace internal {
namespace wasm {

using trap_handler::ProtectedInstructionData;

#if defined(V8_OS_MACOSX) && defined(V8_HOST_ARCH_ARM64)
thread_local int CodeSpaceWriteScope::code_space_write_nesting_level_ = 0;
#endif

base::AddressRegion DisjointAllocationPool::Merge(
    base::AddressRegion new_region) {
  // Find the possible insertion position by identifying the first region whose
  // start address is not less than that of {new_region}. Since there cannot be
  // any overlap between regions, this also means that the start of {above} is
  // bigger or equal than the *end* of {new_region}.
  auto above = regions_.lower_bound(new_region);
  DCHECK(above == regions_.end() || above->begin() >= new_region.end());

  // Check whether to merge with {above}.
  if (above != regions_.end() && new_region.end() == above->begin()) {
    base::AddressRegion merged_region{new_region.begin(),
                                      new_region.size() + above->size()};
    DCHECK_EQ(merged_region.end(), above->end());
    // Check whether to also merge with the region below.
    if (above != regions_.begin()) {
      auto below = above;
      --below;
      if (below->end() == new_region.begin()) {
        merged_region = {below->begin(), below->size() + merged_region.size()};
        regions_.erase(below);
      }
    }
    auto insert_pos = regions_.erase(above);
    regions_.insert(insert_pos, merged_region);
    return merged_region;
  }

  // No element below, and not adjavent to {above}: insert and done.
  if (above == regions_.begin()) {
    regions_.insert(above, new_region);
    return new_region;
  }

  auto below = above;
  --below;
  // Consistency check:
  DCHECK(above == regions_.end() || below->end() < above->begin());

  // Adjacent to {below}: merge and done.
  if (below->end() == new_region.begin()) {
    base::AddressRegion merged_region{below->begin(),
                                      below->size() + new_region.size()};
    DCHECK_EQ(merged_region.end(), new_region.end());
    regions_.erase(below);
    regions_.insert(above, merged_region);
    return merged_region;
  }

  // Not adjacent to any existing region: insert between {below} and {above}.
  DCHECK_LT(below->end(), new_region.begin());
  regions_.insert(above, new_region);
  return new_region;
}

base::AddressRegion DisjointAllocationPool::Allocate(size_t size) {
  return AllocateInRegion(size,
                          {kNullAddress, std::numeric_limits<size_t>::max()});
}

base::AddressRegion DisjointAllocationPool::AllocateInRegion(
    size_t size, base::AddressRegion region) {
  // Get an iterator to the first contained region whose start address is not
  // smaller than the start address of {region}. Start the search from the
  // region one before that (the last one whose start address is smaller).
  auto it = regions_.lower_bound(region);
  if (it != regions_.begin()) --it;

  for (auto end = regions_.end(); it != end; ++it) {
    base::AddressRegion overlap = it->GetOverlap(region);
    if (size > overlap.size()) continue;
    base::AddressRegion ret{overlap.begin(), size};
    base::AddressRegion old = *it;
    auto insert_pos = regions_.erase(it);
    if (size == old.size()) {
      // We use the full region --> nothing to add back.
    } else if (ret.begin() == old.begin()) {
      // We return a region at the start --> shrink old region from front.
      regions_.insert(insert_pos, {old.begin() + size, old.size() - size});
    } else if (ret.end() == old.end()) {
      // We return a region at the end --> shrink remaining region.
      regions_.insert(insert_pos, {old.begin(), old.size() - size});
    } else {
      // We return something in the middle --> split the remaining region
      // (insert the region with smaller address first).
      regions_.insert(insert_pos, {old.begin(), ret.begin() - old.begin()});
      regions_.insert(insert_pos, {ret.end(), old.end() - ret.end()});
    }
    return ret;
  }
  return {};
}

Address WasmCode::constant_pool() const {
  if (FLAG_enable_embedded_constant_pool) {
    if (constant_pool_offset_ < code_comments_offset_) {
      return instruction_start() + constant_pool_offset_;
    }
  }
  return kNullAddress;
}

Address WasmCode::handler_table() const {
  return instruction_start() + handler_table_offset_;
}

int WasmCode::handler_table_size() const {
  DCHECK_GE(constant_pool_offset_, handler_table_offset_);
  return static_cast<int>(constant_pool_offset_ - handler_table_offset_);
}

Address WasmCode::code_comments() const {
  return instruction_start() + code_comments_offset_;
}

int WasmCode::code_comments_size() const {
  DCHECK_GE(unpadded_binary_size_, code_comments_offset_);
  return static_cast<int>(unpadded_binary_size_ - code_comments_offset_);
}

std::unique_ptr<const byte[]> WasmCode::ConcatenateBytes(
    std::initializer_list<Vector<const byte>> vectors) {
  size_t total_size = 0;
  for (auto& vec : vectors) total_size += vec.size();
  // Use default-initialization (== no initialization).
  std::unique_ptr<byte[]> result{new byte[total_size]};
  byte* ptr = result.get();
  for (auto& vec : vectors) {
    if (vec.empty()) continue;  // Avoid nullptr in {memcpy}.
    base::Memcpy(ptr, vec.begin(), vec.size());
    ptr += vec.size();
  }
  return result;
}

void WasmCode::RegisterTrapHandlerData() {
  DCHECK(!has_trap_handler_index());
  if (kind() != WasmCode::kFunction) return;
  if (protected_instructions_size_ == 0) return;

  Address base = instruction_start();

  size_t size = instructions().size();
  auto protected_instruction_data = this->protected_instructions();
  const int index =
      RegisterHandlerData(base, size, protected_instruction_data.size(),
                          protected_instruction_data.begin());

  // TODO(eholk): if index is negative, fail.
  CHECK_LE(0, index);
  set_trap_handler_index(index);
  DCHECK(has_trap_handler_index());
}

bool WasmCode::ShouldBeLogged(Isolate* isolate) {
  // The return value is cached in {WasmEngine::IsolateData::log_codes}. Ensure
  // to call {WasmEngine::EnableCodeLogging} if this return value would change
  // for any isolate. Otherwise we might lose code events.
  return isolate->logger()->is_listening_to_code_events() ||
         isolate->code_event_dispatcher()->IsListeningToCodeEvents() ||
         isolate->is_profiling();
}

void WasmCode::LogCode(Isolate* isolate, const char* source_url,
                       int script_id) const {
  DCHECK(ShouldBeLogged(isolate));
  if (IsAnonymous()) return;

  ModuleWireBytes wire_bytes(native_module_->wire_bytes());
  const WasmModule* module = native_module_->module();
  WireBytesRef name_ref = module->lazily_generated_names.LookupFunctionName(
      wire_bytes, index(), VectorOf(module->export_table));
  WasmName name = wire_bytes.GetNameOrNull(name_ref);

  const WasmDebugSymbols& debug_symbols = module->debug_symbols;
  auto load_wasm_source_map = isolate->wasm_load_source_map_callback();
  auto source_map = native_module_->GetWasmSourceMap();
  if (!source_map && debug_symbols.type == WasmDebugSymbols::Type::SourceMap &&
      !debug_symbols.external_url.is_empty() && load_wasm_source_map) {
    WasmName external_url =
        wire_bytes.GetNameOrNull(debug_symbols.external_url);
    std::string external_url_string(external_url.data(), external_url.size());
    HandleScope scope(isolate);
    v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
    Local<v8::String> source_map_str =
        load_wasm_source_map(v8_isolate, external_url_string.c_str());
    native_module_->SetWasmSourceMap(
        std::make_unique<WasmModuleSourceMap>(v8_isolate, source_map_str));
  }

  std::string name_buffer;
  if (kind() == kWasmToJsWrapper) {
    name_buffer = "wasm-to-js:";
    size_t prefix_len = name_buffer.size();
    constexpr size_t kMaxSigLength = 128;
    name_buffer.resize(prefix_len + kMaxSigLength);
    const FunctionSig* sig = module->functions[index_].sig;
    size_t sig_length =
        PrintSignature(VectorOf(&name_buffer[prefix_len], kMaxSigLength), sig);
    name_buffer.resize(prefix_len + sig_length);
    // If the import has a name, also append that (separated by "-").
    if (!name.empty()) {
      name_buffer += '-';
      name_buffer.append(name.begin(), name.size());
    }
    name = VectorOf(name_buffer);
  } else if (name.empty()) {
    name_buffer.resize(32);
    name_buffer.resize(
        SNPrintF(VectorOf(&name_buffer.front(), name_buffer.size()),
                 "wasm-function[%d]", index()));
    name = VectorOf(name_buffer);
  }
  int code_offset = module->functions[index_].code.offset();
  PROFILE(isolate, CodeCreateEvent(CodeEventListener::FUNCTION_TAG, this, name,
                                   source_url, code_offset, script_id));

  if (!source_positions().empty()) {
    LOG_CODE_EVENT(isolate, CodeLinePosInfoRecordEvent(instruction_start(),
                                                       source_positions()));
  }
}

void WasmCode::Validate() const {
#ifdef DEBUG
  // Scope for foreign WasmCode pointers.
  WasmCodeRefScope code_ref_scope;
  // We expect certain relocation info modes to never appear in {WasmCode}
  // objects or to be restricted to a small set of valid values. Hence the
  // iteration below does not use a mask, but visits all relocation data.
  for (RelocIterator it(instructions(), reloc_info(), constant_pool());
       !it.done(); it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    switch (mode) {
      case RelocInfo::WASM_CALL: {
        Address target = it.rinfo()->wasm_call_address();
        WasmCode* code = native_module_->Lookup(target);
        CHECK_NOT_NULL(code);
        CHECK_EQ(WasmCode::kJumpTable, code->kind());
        CHECK(code->contains(target));
        break;
      }
      case RelocInfo::WASM_STUB_CALL: {
        Address target = it.rinfo()->wasm_stub_call_address();
        WasmCode* code = native_module_->Lookup(target);
        CHECK_NOT_NULL(code);
        CHECK_EQ(WasmCode::kJumpTable, code->kind());
        CHECK(code->contains(target));
        break;
      }
      case RelocInfo::INTERNAL_REFERENCE:
      case RelocInfo::INTERNAL_REFERENCE_ENCODED: {
        Address target = it.rinfo()->target_internal_reference();
        CHECK(contains(target));
        break;
      }
      case RelocInfo::EXTERNAL_REFERENCE:
      case RelocInfo::CONST_POOL:
      case RelocInfo::VENEER_POOL:
        // These are OK to appear.
        break;
      default:
        FATAL("Unexpected mode: %d", mode);
    }
  }
#endif
}

void WasmCode::MaybePrint(const char* name) const {
  // Determines whether flags want this code to be printed.
  bool function_index_matches =
      (!IsAnonymous() &&
       FLAG_print_wasm_code_function_index == static_cast<int>(index()));
  if (FLAG_print_code ||
      (kind() == kFunction ? (FLAG_print_wasm_code || function_index_matches)
                           : FLAG_print_wasm_stub_code)) {
    Print(name);
  }
}

void WasmCode::Print(const char* name) const {
  StdoutStream os;
  os << "--- WebAssembly code ---\n";
  Disassemble(name, os);
  if (native_module_->HasDebugInfo()) {
    if (auto* debug_side_table =
            native_module_->GetDebugInfo()->GetDebugSideTableIfExists(this)) {
      debug_side_table->Print(os);
    }
  }
  os << "--- End code ---\n";
}

void WasmCode::Disassemble(const char* name, std::ostream& os,
                           Address current_pc) const {
  if (name) os << "name: " << name << "\n";
  if (!IsAnonymous()) os << "index: " << index() << "\n";
  os << "kind: " << GetWasmCodeKindAsString(kind()) << "\n";
  os << "compiler: " << (is_liftoff() ? "Liftoff" : "TurboFan") << "\n";
  size_t padding = instructions().size() - unpadded_binary_size_;
  os << "Body (size = " << instructions().size() << " = "
     << unpadded_binary_size_ << " + " << padding << " padding)\n";

#ifdef ENABLE_DISASSEMBLER
  int instruction_size = unpadded_binary_size_;
  if (constant_pool_offset_ < instruction_size) {
    instruction_size = constant_pool_offset_;
  }
  if (safepoint_table_offset_ && safepoint_table_offset_ < instruction_size) {
    instruction_size = safepoint_table_offset_;
  }
  if (handler_table_offset_ < instruction_size) {
    instruction_size = handler_table_offset_;
  }
  DCHECK_LT(0, instruction_size);
  os << "Instructions (size = " << instruction_size << ")\n";
  Disassembler::Decode(nullptr, &os, instructions().begin(),
                       instructions().begin() + instruction_size,
                       CodeReference(this), current_pc);
  os << "\n";

  if (handler_table_size() > 0) {
    HandlerTable table(this);
    os << "Exception Handler Table (size = " << table.NumberOfReturnEntries()
       << "):\n";
    table.HandlerTableReturnPrint(os);
    os << "\n";
  }

  if (protected_instructions_size_ > 0) {
    os << "Protected instructions:\n pc offset  land pad\n";
    for (auto& data : protected_instructions()) {
      os << std::setw(10) << std::hex << data.instr_offset << std::setw(10)
         << std::hex << data.landing_offset << "\n";
    }
    os << "\n";
  }

  if (!source_positions().empty()) {
    os << "Source positions:\n pc offset  position\n";
    for (SourcePositionTableIterator it(source_positions()); !it.done();
         it.Advance()) {
      os << std::setw(10) << std::hex << it.code_offset() << std::dec
         << std::setw(10) << it.source_position().ScriptOffset()
         << (it.is_statement() ? "  statement" : "") << "\n";
    }
    os << "\n";
  }

  if (safepoint_table_offset_ > 0) {
    SafepointTable table(this);
    os << "Safepoints (size = " << table.size() << ")\n";
    for (uint32_t i = 0; i < table.length(); i++) {
      uintptr_t pc_offset = table.GetPcOffset(i);
      os << reinterpret_cast<const void*>(instruction_start() + pc_offset);
      os << std::setw(6) << std::hex << pc_offset << "  " << std::dec;
      table.PrintEntry(i, os);
      os << " (sp -> fp)";
      SafepointEntry entry = table.GetEntry(i);
      if (entry.trampoline_pc() != SafepointEntry::kNoTrampolinePC) {
        os << " trampoline: " << std::hex << entry.trampoline_pc() << std::dec;
      }
      if (entry.has_deoptimization_index()) {
        os << " deopt: " << std::setw(6) << entry.deoptimization_index();
      }
      os << "\n";
    }
    os << "\n";
  }

  os << "RelocInfo (size = " << reloc_info().size() << ")\n";
  for (RelocIterator it(instructions(), reloc_info(), constant_pool());
       !it.done(); it.next()) {
    it.rinfo()->Print(nullptr, os);
  }
  os << "\n";
#endif  // ENABLE_DISASSEMBLER
}

const char* GetWasmCodeKindAsString(WasmCode::Kind kind) {
  switch (kind) {
    case WasmCode::kFunction:
      return "wasm function";
    case WasmCode::kWasmToCapiWrapper:
      return "wasm-to-capi";
    case WasmCode::kWasmToJsWrapper:
      return "wasm-to-js";
    case WasmCode::kJumpTable:
      return "jump table";
  }
  return "unknown kind";
}

WasmCode::~WasmCode() {
  if (has_trap_handler_index()) {
    trap_handler::ReleaseHandlerData(trap_handler_index());
  }
}

V8_WARN_UNUSED_RESULT bool WasmCode::DecRefOnPotentiallyDeadCode() {
  if (native_module_->engine()->AddPotentiallyDeadCode(this)) {
    // The code just became potentially dead. The ref count we wanted to
    // decrement is now transferred to the set of potentially dead code, and
    // will be decremented when the next GC is run.
    return false;
  }
  // If we reach here, the code was already potentially dead. Decrement the ref
  // count, and return true if it drops to zero.
  return DecRefOnDeadCode();
}

// static
void WasmCode::DecrementRefCount(Vector<WasmCode* const> code_vec) {
  // Decrement the ref counter of all given code objects. Keep the ones whose
  // ref count drops to zero.
  WasmEngine::DeadCodeMap dead_code;
  WasmEngine* engine = nullptr;
  for (WasmCode* code : code_vec) {
    if (!code->DecRef()) continue;  // Remaining references.
    dead_code[code->native_module()].push_back(code);
    if (!engine) engine = code->native_module()->engine();
    DCHECK_EQ(engine, code->native_module()->engine());
  }

  DCHECK_EQ(dead_code.empty(), engine == nullptr);
  if (engine) engine->FreeDeadCode(dead_code);
}

int WasmCode::GetSourcePositionBefore(int offset) {
  int position = kNoSourcePosition;
  for (SourcePositionTableIterator iterator(source_positions());
       !iterator.done() && iterator.code_offset() < offset;
       iterator.Advance()) {
    position = iterator.source_position().ScriptOffset();
  }
  return position;
}

WasmCodeAllocator::OptionalLock::~OptionalLock() {
  if (allocator_) allocator_->mutex_.Unlock();
}

void WasmCodeAllocator::OptionalLock::Lock(WasmCodeAllocator* allocator) {
  DCHECK(!is_locked());
  allocator_ = allocator;
  allocator->mutex_.Lock();
}

// static
constexpr size_t WasmCodeAllocator::kMaxCodeSpaceSize;

WasmCodeAllocator::WasmCodeAllocator(WasmCodeManager* code_manager,
                                     VirtualMemory code_space,
                                     std::shared_ptr<Counters> async_counters)
    : code_manager_(code_manager),
      free_code_space_(code_space.region()),
      async_counters_(std::move(async_counters)) {
  owned_code_space_.reserve(4);
  owned_code_space_.emplace_back(std::move(code_space));
  async_counters_->wasm_module_num_code_spaces()->AddSample(1);
}

WasmCodeAllocator::~WasmCodeAllocator() {
  code_manager_->FreeNativeModule(VectorOf(owned_code_space_),
                                  committed_code_space());
}

void WasmCodeAllocator::Init(NativeModule* native_module) {
  DCHECK_EQ(1, owned_code_space_.size());
  native_module->AddCodeSpace(owned_code_space_[0].region(), {});
}

namespace {
// On Windows, we cannot commit a region that straddles different reservations
// of virtual memory. Because we bump-allocate, and because, if we need more
// memory, we append that memory at the end of the owned_code_space_ list, we
// traverse that list in reverse order to find the reservation(s) that guide how
// to chunk the region to commit.
#if V8_OS_WIN
constexpr bool kNeedsToSplitRangeByReservations = true;
#else
constexpr bool kNeedsToSplitRangeByReservations = false;
#endif

base::SmallVector<base::AddressRegion, 1> SplitRangeByReservationsIfNeeded(
    base::AddressRegion range,
    const std::vector<VirtualMemory>& owned_code_space) {
  if (!kNeedsToSplitRangeByReservations) return {range};

  base::SmallVector<base::AddressRegion, 1> split_ranges;
  size_t missing_begin = range.begin();
  size_t missing_end = range.end();
  for (auto& vmem : base::Reversed(owned_code_space)) {
    Address overlap_begin = std::max(missing_begin, vmem.address());
    Address overlap_end = std::min(missing_end, vmem.end());
    if (overlap_begin >= overlap_end) continue;
    split_ranges.emplace_back(overlap_begin, overlap_end - overlap_begin);
    // Opportunistically reduce the missing range. This might terminate the loop
    // early.
    if (missing_begin == overlap_begin) missing_begin = overlap_end;
    if (missing_end == overlap_end) missing_end = overlap_begin;
    if (missing_begin >= missing_end) break;
  }
#ifdef ENABLE_SLOW_DCHECKS
  // The returned vector should cover the full range.
  size_t total_split_size = 0;
  for (auto split : split_ranges) total_split_size += split.size();
  DCHECK_EQ(range.size(), total_split_size);
#endif
  return split_ranges;
}

int NumWasmFunctionsInFarJumpTable(uint32_t num_declared_functions) {
  return NativeModule::kNeedsFarJumpsBetweenCodeSpaces
             ? static_cast<int>(num_declared_functions)
             : 0;
}

// Returns an overapproximation of the code size overhead per new code space
// created by the jump tables.
size_t OverheadPerCodeSpace(uint32_t num_declared_functions) {
  // Overhead for the jump table.
  size_t overhead = RoundUp<kCodeAlignment>(
      JumpTableAssembler::SizeForNumberOfSlots(num_declared_functions));

#if defined(V8_OS_WIN64)
  // On Win64, we need to reserve some pages at the beginning of an executable
  // space. See {AddCodeSpace}.
  overhead += Heap::GetCodeRangeReservedAreaSize();
#endif  // V8_OS_WIN64

  // Overhead for the far jump table.
  overhead +=
      RoundUp<kCodeAlignment>(JumpTableAssembler::SizeForNumberOfFarJumpSlots(
          WasmCode::kRuntimeStubCount,
          NumWasmFunctionsInFarJumpTable(num_declared_functions)));

  return overhead;
}

size_t ReservationSize(size_t code_size_estimate, int num_declared_functions,
                       size_t total_reserved) {
  size_t overhead = OverheadPerCodeSpace(num_declared_functions);

  // Reserve a power of two at least as big as any of
  //   a) needed size + overhead (this is the minimum needed)
  //   b) 2 * overhead (to not waste too much space by overhead)
  //   c) 1/4 of current total reservation size (to grow exponentially)
  size_t reserve_size = base::bits::RoundUpToPowerOfTwo(
      std::max(std::max(RoundUp<kCodeAlignment>(code_size_estimate) + overhead,
                        2 * overhead),
               total_reserved / 4));

  // Limit by the maximum supported code space size.
  return std::min(WasmCodeAllocator::kMaxCodeSpaceSize, reserve_size);
}

}  // namespace

Vector<byte> WasmCodeAllocator::AllocateForCode(NativeModule* native_module,
                                                size_t size) {
  return AllocateForCodeInRegion(native_module, size, kUnrestrictedRegion,
                                 WasmCodeAllocator::OptionalLock{});
}

Vector<byte> WasmCodeAllocator::AllocateForCodeInRegion(
    NativeModule* native_module, size_t size, base::AddressRegion region,
    const WasmCodeAllocator::OptionalLock& optional_lock) {
  OptionalLock new_lock;
  if (!optional_lock.is_locked()) new_lock.Lock(this);
  const auto& locked_lock =
      optional_lock.is_locked() ? optional_lock : new_lock;
  DCHECK(locked_lock.is_locked());
  DCHECK_EQ(code_manager_, native_module->engine()->code_manager());
  DCHECK_LT(0, size);
  v8::PageAllocator* page_allocator = GetPlatformPageAllocator();
  size = RoundUp<kCodeAlignment>(size);
  base::AddressRegion code_space =
      free_code_space_.AllocateInRegion(size, region);
  if (V8_UNLIKELY(code_space.is_empty())) {
    // Only allocations without a specific region are allowed to fail. Otherwise
    // the region must have been allocated big enough to hold all initial
    // allocations (jump tables etc).
    CHECK_EQ(kUnrestrictedRegion, region);

    Address hint = owned_code_space_.empty() ? kNullAddress
                                             : owned_code_space_.back().end();

    size_t total_reserved = 0;
    for (auto& vmem : owned_code_space_) total_reserved += vmem.size();
    size_t reserve_size = ReservationSize(
        size, native_module->module()->num_declared_functions, total_reserved);
    VirtualMemory new_mem =
        code_manager_->TryAllocate(reserve_size, reinterpret_cast<void*>(hint));
    if (!new_mem.IsReserved()) {
      V8::FatalProcessOutOfMemory(nullptr, "wasm code reservation");
      UNREACHABLE();
    }

    base::AddressRegion new_region = new_mem.region();
    code_manager_->AssignRange(new_region, native_module);
    free_code_space_.Merge(new_region);
    owned_code_space_.emplace_back(std::move(new_mem));
    native_module->AddCodeSpace(new_region, locked_lock);

    code_space = free_code_space_.Allocate(size);
    DCHECK(!code_space.is_empty());
    async_counters_->wasm_module_num_code_spaces()->AddSample(
        static_cast<int>(owned_code_space_.size()));
  }
  const Address commit_page_size = page_allocator->CommitPageSize();
  Address commit_start = RoundUp(code_space.begin(), commit_page_size);
  Address commit_end = RoundUp(code_space.end(), commit_page_size);
  // {commit_start} will be either code_space.start or the start of the next
  // page. {commit_end} will be the start of the page after the one in which
  // the allocation ends.
  // We start from an aligned start, and we know we allocated vmem in
  // page multiples.
  // We just need to commit what's not committed. The page in which we
  // start is already committed (or we start at the beginning of a page).
  // The end needs to be committed all through the end of the page.
  if (commit_start < commit_end) {
    for (base::AddressRegion split_range : SplitRangeByReservationsIfNeeded(
             {commit_start, commit_end - commit_start}, owned_code_space_)) {
      code_manager_->Commit(split_range);
    }
    committed_code_space_.fetch_add(commit_end - commit_start);
    // Committed code cannot grow bigger than maximum code space size.
    DCHECK_LE(committed_code_space_.load(), FLAG_wasm_max_code_space * MB);
  }
  DCHECK(IsAligned(code_space.begin(), kCodeAlignment));
  allocated_code_space_.Merge(code_space);
  generated_code_size_.fetch_add(code_space.size(), std::memory_order_relaxed);

  TRACE_HEAP("Code alloc for %p: 0x%" PRIxPTR ",+%zu\n", this,
             code_space.begin(), size);
  return {reinterpret_cast<byte*>(code_space.begin()), code_space.size()};
}

bool WasmCodeAllocator::SetExecutable(bool executable) {
  base::MutexGuard lock(&mutex_);
  if (is_executable_ == executable) return true;
  TRACE_HEAP("Setting module %p as executable: %d.\n", this, executable);

  v8::PageAllocator* page_allocator = GetPlatformPageAllocator();

  if (FLAG_wasm_write_protect_code_memory) {
    PageAllocator::Permission permission =
        executable ? PageAllocator::kReadExecute : PageAllocator::kReadWrite;
#if V8_OS_WIN
    // On windows, we need to switch permissions per separate virtual memory
    // reservation.
    // For now, in that case, we commit at reserved memory granularity.
    // Technically, that may be a waste, because we may reserve more than we
    // use. On 32-bit though, the scarce resource is the address space -
    // committed or not.
    for (auto& vmem : owned_code_space_) {
      if (!SetPermissions(page_allocator, vmem.address(), vmem.size(),
                          permission)) {
        return false;
      }
      TRACE_HEAP("Set %p:%p to executable:%d\n", vmem.address(), vmem.end(),
                 executable);
    }
#else   // V8_OS_WIN
    size_t commit_page_size = page_allocator->CommitPageSize();
    for (auto& region : allocated_code_space_.regions()) {
      // allocated_code_space_ is fine-grained, so we need to
      // page-align it.
      size_t region_size = RoundUp(region.size(), commit_page_size);
      if (!SetPermissions(page_allocator, region.begin(), region_size,
                          permission)) {
        return false;
      }
      TRACE_HEAP("Set 0x%" PRIxPTR ":0x%" PRIxPTR " to executable:%d\n",
                 region.begin(), region.end(), executable);
    }
#endif  // V8_OS_WIN
  }
  is_executable_ = executable;
  return true;
}

void WasmCodeAllocator::FreeCode(Vector<WasmCode* const> codes) {
  // Zap code area and collect freed code regions.
  DisjointAllocationPool freed_regions;
  size_t code_size = 0;
  CODE_SPACE_WRITE_SCOPE
  for (WasmCode* code : codes) {
    ZapCode(code->instruction_start(), code->instructions().size());
    FlushInstructionCache(code->instruction_start(),
                          code->instructions().size());
    code_size += code->instructions().size();
    freed_regions.Merge(base::AddressRegion{code->instruction_start(),
                                            code->instructions().size()});
  }
  freed_code_size_.fetch_add(code_size);

  // Merge {freed_regions} into {freed_code_space_} and put all ranges of full
  // pages to decommit into {regions_to_decommit} (decommitting is expensive,
  // so try to merge regions before decommitting).
  DisjointAllocationPool regions_to_decommit;
  PageAllocator* allocator = GetPlatformPageAllocator();
  size_t commit_page_size = allocator->CommitPageSize();
  {
    base::MutexGuard guard(&mutex_);
    for (auto region : freed_regions.regions()) {
      auto merged_region = freed_code_space_.Merge(region);
      Address discard_start =
          std::max(RoundUp(merged_region.begin(), commit_page_size),
                   RoundDown(region.begin(), commit_page_size));
      Address discard_end =
          std::min(RoundDown(merged_region.end(), commit_page_size),
                   RoundUp(region.end(), commit_page_size));
      if (discard_start >= discard_end) continue;
      regions_to_decommit.Merge({discard_start, discard_end - discard_start});
    }
  }

  for (auto region : regions_to_decommit.regions()) {
    size_t old_committed = committed_code_space_.fetch_sub(region.size());
    DCHECK_GE(old_committed, region.size());
    USE(old_committed);
    for (base::AddressRegion split_range :
         SplitRangeByReservationsIfNeeded(region, owned_code_space_)) {
      code_manager_->Decommit(split_range);
    }
  }
}

size_t WasmCodeAllocator::GetNumCodeSpaces() const {
  base::MutexGuard lock(&mutex_);
  return owned_code_space_.size();
}

// static
constexpr base::AddressRegion WasmCodeAllocator::kUnrestrictedRegion;

NativeModule::NativeModule(WasmEngine* engine, const WasmFeatures& enabled,
                           VirtualMemory code_space,
                           std::shared_ptr<const WasmModule> module,
                           std::shared_ptr<Counters> async_counters,
                           std::shared_ptr<NativeModule>* shared_this)
    : engine_(engine),
      engine_scope_(engine->GetBarrierForBackgroundCompile()->TryLock()),
      code_allocator_(engine->code_manager(), std::move(code_space),
                      async_counters),
      enabled_features_(enabled),
      module_(std::move(module)),
      import_wrapper_cache_(std::unique_ptr<WasmImportWrapperCache>(
          new WasmImportWrapperCache())),
      use_trap_handler_(trap_handler::IsTrapHandlerEnabled() ? kUseTrapHandler
                                                             : kNoTrapHandler) {
  DCHECK(engine_scope_);
  // We receive a pointer to an empty {std::shared_ptr}, and install ourselve
  // there.
  DCHECK_NOT_NULL(shared_this);
  DCHECK_NULL(*shared_this);
  shared_this->reset(this);
  compilation_state_ =
      CompilationState::New(*shared_this, std::move(async_counters));
  compilation_state_->InitCompileJob(engine);
  DCHECK_NOT_NULL(module_);
  if (module_->num_declared_functions > 0) {
    code_table_ =
        std::make_unique<WasmCode*[]>(module_->num_declared_functions);
    num_liftoff_function_calls_ =
        std::make_unique<uint32_t[]>(module_->num_declared_functions);

    // Start counter at 4 to avoid runtime calls for smaller numbers.
    constexpr int kCounterStart = 4;
    std::fill_n(num_liftoff_function_calls_.get(),
                module_->num_declared_functions, kCounterStart);
  }
  code_allocator_.Init(this);
}

void NativeModule::ReserveCodeTableForTesting(uint32_t max_functions) {
  WasmCodeRefScope code_ref_scope;
  DCHECK_LE(module_->num_declared_functions, max_functions);
  auto new_table = std::make_unique<WasmCode*[]>(max_functions);
  if (module_->num_declared_functions > 0) {
    base::Memcpy(new_table.get(), code_table_.get(),
                 module_->num_declared_functions * sizeof(WasmCode*));
  }
  code_table_ = std::move(new_table);

  base::AddressRegion single_code_space_region;
  {
    base::MutexGuard guard(&allocation_mutex_);
    CHECK_EQ(1, code_space_data_.size());
    single_code_space_region = code_space_data_[0].region;
  }
  // Re-allocate jump table.
  main_jump_table_ = CreateEmptyJumpTableInRegion(
      JumpTableAssembler::SizeForNumberOfSlots(max_functions),
      single_code_space_region, WasmCodeAllocator::OptionalLock{});
  base::MutexGuard guard(&allocation_mutex_);
  code_space_data_[0].jump_table = main_jump_table_;
}

void NativeModule::LogWasmCodes(Isolate* isolate, Script script) {
  DisallowGarbageCollection no_gc;
  if (!WasmCode::ShouldBeLogged(isolate)) return;

  TRACE_EVENT1("v8.wasm", "wasm.LogWasmCodes", "functions",
               module_->num_declared_functions);

  Object source_url_obj = script.source_url();
  DCHECK(source_url_obj.IsString() || source_url_obj.IsUndefined());
  std::unique_ptr<char[]> source_url =
      source_url_obj.IsString() ? String::cast(source_url_obj).ToCString()
                                : nullptr;

  // Log all owned code, not just the current entries in the code table. This
  // will also include import wrappers.
  base::MutexGuard lock(&allocation_mutex_);
  for (auto& owned_entry : owned_code_) {
    owned_entry.second->LogCode(isolate, source_url.get(), script.id());
  }
  for (auto& owned_entry : new_owned_code_) {
    owned_entry->LogCode(isolate, source_url.get(), script.id());
  }
}

CompilationEnv NativeModule::CreateCompilationEnv() const {
  return {module(), use_trap_handler_, kRuntimeExceptionSupport,
          enabled_features_, kNoLowerSimd};
}

WasmCode* NativeModule::AddCodeForTesting(Handle<Code> code) {
  CODE_SPACE_WRITE_SCOPE
  const size_t relocation_size = code->relocation_size();
  OwnedVector<byte> reloc_info;
  if (relocation_size > 0) {
    reloc_info = OwnedVector<byte>::Of(
        Vector<byte>{code->relocation_start(), relocation_size});
  }
  Handle<ByteArray> source_pos_table(code->SourcePositionTable(),
                                     code->GetIsolate());
  OwnedVector<byte> source_pos =
      OwnedVector<byte>::NewForOverwrite(source_pos_table->length());
  if (source_pos_table->length() > 0) {
    source_pos_table->copy_out(0, source_pos.start(),
                               source_pos_table->length());
  }
  CHECK(!code->is_off_heap_trampoline());
  STATIC_ASSERT(Code::kOnHeapBodyIsContiguous);
  Vector<const byte> instructions(
      reinterpret_cast<byte*>(code->raw_body_start()),
      static_cast<size_t>(code->raw_body_size()));
  const int stack_slots = code->has_safepoint_info() ? code->stack_slots() : 0;

  // Metadata offsets in Code objects are relative to the start of the metadata
  // section, whereas WasmCode expects offsets relative to InstructionStart.
  const int base_offset = code->raw_instruction_size();
  // TODO(jgruber,v8:8758): Remove this translation. It exists only because
  // Code objects contains real offsets but WasmCode expects an offset of 0 to
  // mean 'empty'.
  const int safepoint_table_offset =
      code->has_safepoint_table() ? base_offset + code->safepoint_table_offset()
                                  : 0;
  const int handler_table_offset = base_offset + code->handler_table_offset();
  const int constant_pool_offset = base_offset + code->constant_pool_offset();
  const int code_comments_offset = base_offset + code->code_comments_offset();

  Vector<uint8_t> dst_code_bytes =
      code_allocator_.AllocateForCode(this, instructions.size());
  base::Memcpy(dst_code_bytes.begin(), instructions.begin(),
               instructions.size());

  // Apply the relocation delta by iterating over the RelocInfo.
  intptr_t delta = reinterpret_cast<Address>(dst_code_bytes.begin()) -
                   code->raw_instruction_start();
  int mode_mask =
      RelocInfo::kApplyMask | RelocInfo::ModeMask(RelocInfo::WASM_STUB_CALL);
  auto jump_tables_ref =
      FindJumpTablesForRegion(base::AddressRegionOf(dst_code_bytes));
  Address dst_code_addr = reinterpret_cast<Address>(dst_code_bytes.begin());
  Address constant_pool_start = dst_code_addr + constant_pool_offset;
  RelocIterator orig_it(*code, mode_mask);
  for (RelocIterator it(dst_code_bytes, reloc_info.as_vector(),
                        constant_pool_start, mode_mask);
       !it.done(); it.next(), orig_it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    if (RelocInfo::IsWasmStubCall(mode)) {
      uint32_t stub_call_tag = orig_it.rinfo()->wasm_call_tag();
      DCHECK_LT(stub_call_tag, WasmCode::kRuntimeStubCount);
      Address entry = GetNearRuntimeStubEntry(
          static_cast<WasmCode::RuntimeStubId>(stub_call_tag), jump_tables_ref);
      it.rinfo()->set_wasm_stub_call_address(entry, SKIP_ICACHE_FLUSH);
    } else {
      it.rinfo()->apply(delta);
    }
  }

  // Flush the i-cache after relocation.
  FlushInstructionCache(dst_code_bytes.begin(), dst_code_bytes.size());

  std::unique_ptr<WasmCode> new_code{
      new WasmCode{this,                    // native_module
                   kAnonymousFuncIndex,     // index
                   dst_code_bytes,          // instructions
                   stack_slots,             // stack_slots
                   0,                       // tagged_parameter_slots
                   safepoint_table_offset,  // safepoint_table_offset
                   handler_table_offset,    // handler_table_offset
                   constant_pool_offset,    // constant_pool_offset
                   code_comments_offset,    // code_comments_offset
                   instructions.length(),   // unpadded_binary_size
                   {},                      // protected_instructions
                   reloc_info.as_vector(),  // reloc_info
                   source_pos.as_vector(),  // source positions
                   WasmCode::kFunction,     // kind
                   ExecutionTier::kNone,    // tier
                   kNoDebugging}};          // for_debugging
  new_code->MaybePrint();
  new_code->Validate();

  return PublishCode(std::move(new_code));
}

void NativeModule::UseLazyStub(uint32_t func_index) {
  DCHECK_LE(module_->num_imported_functions, func_index);
  DCHECK_LT(func_index,
            module_->num_imported_functions + module_->num_declared_functions);

  if (!lazy_compile_table_) {
    uint32_t num_slots = module_->num_declared_functions;
    WasmCodeRefScope code_ref_scope;
    CODE_SPACE_WRITE_SCOPE
    base::AddressRegion single_code_space_region;
    {
      base::MutexGuard guard(&allocation_mutex_);
      DCHECK_EQ(1, code_space_data_.size());
      single_code_space_region = code_space_data_[0].region;
    }
    lazy_compile_table_ = CreateEmptyJumpTableInRegion(
        JumpTableAssembler::SizeForNumberOfLazyFunctions(num_slots),
        single_code_space_region, WasmCodeAllocator::OptionalLock{});
    JumpTableAssembler::GenerateLazyCompileTable(
        lazy_compile_table_->instruction_start(), num_slots,
        module_->num_imported_functions,
        GetNearRuntimeStubEntry(WasmCode::kWasmCompileLazy,
                                FindJumpTablesForRegion(base::AddressRegionOf(
                                    lazy_compile_table_->instructions()))));
  }

  // Add jump table entry for jump to the lazy compile stub.
  uint32_t slot_index = declared_function_index(module(), func_index);
  DCHECK_NULL(code_table_[slot_index]);
  Address lazy_compile_target =
      lazy_compile_table_->instruction_start() +
      JumpTableAssembler::LazyCompileSlotIndexToOffset(slot_index);
  base::MutexGuard guard(&allocation_mutex_);
  PatchJumpTablesLocked(slot_index, lazy_compile_target);
}

std::unique_ptr<WasmCode> NativeModule::AddCode(
    int index, const CodeDesc& desc, int stack_slots,
    int tagged_parameter_slots, Vector<const byte> protected_instructions_data,
    Vector<const byte> source_position_table, WasmCode::Kind kind,
    ExecutionTier tier, ForDebugging for_debugging) {
  Vector<byte> code_space =
      code_allocator_.AllocateForCode(this, desc.instr_size);
  auto jump_table_ref =
      FindJumpTablesForRegion(base::AddressRegionOf(code_space));
  return AddCodeWithCodeSpace(index, desc, stack_slots, tagged_parameter_slots,
                              protected_instructions_data,
                              source_position_table, kind, tier, for_debugging,
                              code_space, jump_table_ref);
}

std::unique_ptr<WasmCode> NativeModule::AddCodeWithCodeSpace(
    int index, const CodeDesc& desc, int stack_slots,
    int tagged_parameter_slots, Vector<const byte> protected_instructions_data,
    Vector<const byte> source_position_table, WasmCode::Kind kind,
    ExecutionTier tier, ForDebugging for_debugging,
    Vector<uint8_t> dst_code_bytes, const JumpTablesRef& jump_tables) {
  Vector<byte> reloc_info{desc.buffer + desc.buffer_size - desc.reloc_size,
                          static_cast<size_t>(desc.reloc_size)};
  UpdateCodeSize(desc.instr_size, tier, for_debugging);

  // TODO(jgruber,v8:8758): Remove this translation. It exists only because
  // CodeDesc contains real offsets but WasmCode expects an offset of 0 to mean
  // 'empty'.
  const int safepoint_table_offset =
      desc.safepoint_table_size == 0 ? 0 : desc.safepoint_table_offset;
  const int handler_table_offset = desc.handler_table_offset;
  const int constant_pool_offset = desc.constant_pool_offset;
  const int code_comments_offset = desc.code_comments_offset;
  const int instr_size = desc.instr_size;

  CODE_SPACE_WRITE_SCOPE
  base::Memcpy(dst_code_bytes.begin(), desc.buffer,
               static_cast<size_t>(desc.instr_size));

  // Apply the relocation delta by iterating over the RelocInfo.
  intptr_t delta = dst_code_bytes.begin() - desc.buffer;
  int mode_mask = RelocInfo::kApplyMask |
                  RelocInfo::ModeMask(RelocInfo::WASM_CALL) |
                  RelocInfo::ModeMask(RelocInfo::WASM_STUB_CALL);
  Address code_start = reinterpret_cast<Address>(dst_code_bytes.begin());
  Address constant_pool_start = code_start + constant_pool_offset;
  for (RelocIterator it(dst_code_bytes, reloc_info, constant_pool_start,
                        mode_mask);
       !it.done(); it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    if (RelocInfo::IsWasmCall(mode)) {
      uint32_t call_tag = it.rinfo()->wasm_call_tag();
      Address target = GetNearCallTargetForFunction(call_tag, jump_tables);
      it.rinfo()->set_wasm_call_address(target, SKIP_ICACHE_FLUSH);
    } else if (RelocInfo::IsWasmStubCall(mode)) {
      uint32_t stub_call_tag = it.rinfo()->wasm_call_tag();
      DCHECK_LT(stub_call_tag, WasmCode::kRuntimeStubCount);
      Address entry = GetNearRuntimeStubEntry(
          static_cast<WasmCode::RuntimeStubId>(stub_call_tag), jump_tables);
      it.rinfo()->set_wasm_stub_call_address(entry, SKIP_ICACHE_FLUSH);
    } else {
      it.rinfo()->apply(delta);
    }
  }

  // Flush the i-cache after relocation.
  FlushInstructionCache(dst_code_bytes.begin(), dst_code_bytes.size());

  // Liftoff code will not be relocated or serialized, thus do not store any
  // relocation information.
  if (tier == ExecutionTier::kLiftoff) reloc_info = {};

  std::unique_ptr<WasmCode> code{new WasmCode{
      this, index, dst_code_bytes, stack_slots, tagged_parameter_slots,
      safepoint_table_offset, handler_table_offset, constant_pool_offset,
      code_comments_offset, instr_size, protected_instructions_data, reloc_info,
      source_position_table, kind, tier, for_debugging}};
  code->MaybePrint();
  code->Validate();

  return code;
}

WasmCode* NativeModule::PublishCode(std::unique_ptr<WasmCode> code) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
               "wasm.PublishCode");
  base::MutexGuard lock(&allocation_mutex_);
  return PublishCodeLocked(std::move(code));
}

std::vector<WasmCode*> NativeModule::PublishCode(
    Vector<std::unique_ptr<WasmCode>> codes) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
               "wasm.PublishCode", "number", codes.size());
  std::vector<WasmCode*> published_code;
  published_code.reserve(codes.size());
  base::MutexGuard lock(&allocation_mutex_);
  // The published code is put into the top-most surrounding {WasmCodeRefScope}.
  for (auto& code : codes) {
    published_code.push_back(PublishCodeLocked(std::move(code)));
  }
  return published_code;
}

WasmCode::Kind GetCodeKind(const WasmCompilationResult& result) {
  switch (result.kind) {
    case WasmCompilationResult::kWasmToJsWrapper:
      return WasmCode::Kind::kWasmToJsWrapper;
    case WasmCompilationResult::kFunction:
      return WasmCode::Kind::kFunction;
    default:
      UNREACHABLE();
  }
}

WasmCode* NativeModule::PublishCodeLocked(std::unique_ptr<WasmCode> code) {
  // The caller must hold the {allocation_mutex_}, thus we fail to lock it here.
  DCHECK(!allocation_mutex_.TryLock());

  // Add the code to the surrounding code ref scope, so the returned pointer is
  // guaranteed to be valid.
  WasmCodeRefScope::AddRef(code.get());

  if (!code->IsAnonymous() &&
      code->index() >= module_->num_imported_functions) {
    DCHECK_LT(code->index(), num_functions());

    code->RegisterTrapHandlerData();

    // Assume an order of execution tiers that represents the quality of their
    // generated code.
    static_assert(ExecutionTier::kNone < ExecutionTier::kLiftoff &&
                      ExecutionTier::kLiftoff < ExecutionTier::kTurbofan,
                  "Assume an order on execution tiers");

    uint32_t slot_idx = declared_function_index(module(), code->index());
    WasmCode* prior_code = code_table_[slot_idx];
    // If we are tiered down, install all debugging code (except for stepping
    // code, which is only used for a single frame and never installed in the
    // code table of jump table). Otherwise, install code if it was compiled
    // with a higher tier.
    static_assert(
        kForDebugging > kNoDebugging && kWithBreakpoints > kForDebugging,
        "for_debugging is ordered");
    const bool update_code_table =
        // Never install stepping code.
        code->for_debugging() != kForStepping &&
        (!prior_code ||
         (tiering_state_ == kTieredDown
              // Tiered down: Install breakpoints over normal debug code.
              ? prior_code->for_debugging() <= code->for_debugging()
              // Tiered up: Install if the tier is higher than before.
              : prior_code->tier() < code->tier()));
    if (update_code_table) {
      code_table_[slot_idx] = code.get();
      if (prior_code) {
        WasmCodeRefScope::AddRef(prior_code);
        // The code is added to the current {WasmCodeRefScope}, hence the ref
        // count cannot drop to zero here.
        prior_code->DecRefOnLiveCode();
      }

      PatchJumpTablesLocked(slot_idx, code->instruction_start());
    } else {
      // The code tables does not hold a reference to the code, hence decrement
      // the initial ref count of 1. The code was added to the
      // {WasmCodeRefScope} though, so it cannot die here.
      code->DecRefOnLiveCode();
    }
    if (!code->for_debugging() && tiering_state_ == kTieredDown &&
        code->tier() == ExecutionTier::kTurbofan) {
      liftoff_bailout_count_.fetch_add(1);
    }
  }
  WasmCode* result = code.get();
  new_owned_code_.emplace_back(std::move(code));
  return result;
}

Vector<uint8_t> NativeModule::AllocateForDeserializedCode(
    size_t total_code_size) {
  return code_allocator_.AllocateForCode(this, total_code_size);
}

std::unique_ptr<WasmCode> NativeModule::AddDeserializedCode(
    int index, Vector<byte> instructions, int stack_slots,
    int tagged_parameter_slots, int safepoint_table_offset,
    int handler_table_offset, int constant_pool_offset,
    int code_comments_offset, int unpadded_binary_size,
    Vector<const byte> protected_instructions_data,
    Vector<const byte> reloc_info, Vector<const byte> source_position_table,
    WasmCode::Kind kind, ExecutionTier tier) {
  UpdateCodeSize(instructions.size(), tier, kNoDebugging);

  return std::unique_ptr<WasmCode>{new WasmCode{
      this, index, instructions, stack_slots, tagged_parameter_slots,
      safepoint_table_offset, handler_table_offset, constant_pool_offset,
      code_comments_offset, unpadded_binary_size, protected_instructions_data,
      reloc_info, source_position_table, kind, tier, kNoDebugging}};
}

std::vector<WasmCode*> NativeModule::SnapshotCodeTable() const {
  base::MutexGuard lock(&allocation_mutex_);
  WasmCode** start = code_table_.get();
  WasmCode** end = start + module_->num_declared_functions;
  for (WasmCode* code : VectorOf(start, end - start)) {
    if (code) WasmCodeRefScope::AddRef(code);
  }
  return std::vector<WasmCode*>{start, end};
}

WasmCode* NativeModule::GetCode(uint32_t index) const {
  base::MutexGuard guard(&allocation_mutex_);
  WasmCode* code = code_table_[declared_function_index(module(), index)];
  if (code) WasmCodeRefScope::AddRef(code);
  return code;
}

bool NativeModule::HasCode(uint32_t index) const {
  base::MutexGuard guard(&allocation_mutex_);
  return code_table_[declared_function_index(module(), index)] != nullptr;
}

bool NativeModule::HasCodeWithTier(uint32_t index, ExecutionTier tier) const {
  base::MutexGuard guard(&allocation_mutex_);
  return code_table_[declared_function_index(module(), index)] != nullptr &&
         code_table_[declared_function_index(module(), index)]->tier() == tier;
}

void NativeModule::SetWasmSourceMap(
    std::unique_ptr<WasmModuleSourceMap> source_map) {
  source_map_ = std::move(source_map);
}

WasmModuleSourceMap* NativeModule::GetWasmSourceMap() const {
  return source_map_.get();
}

WasmCode* NativeModule::CreateEmptyJumpTableInRegion(
    int jump_table_size, base::AddressRegion region,
    const WasmCodeAllocator::OptionalLock& allocator_lock) {
  // Only call this if we really need a jump table.
  DCHECK_LT(0, jump_table_size);
  Vector<uint8_t> code_space = code_allocator_.AllocateForCodeInRegion(
      this, jump_table_size, region, allocator_lock);
  DCHECK(!code_space.empty());
  UpdateCodeSize(jump_table_size, ExecutionTier::kNone, kNoDebugging);
  CODE_SPACE_WRITE_SCOPE
  ZapCode(reinterpret_cast<Address>(code_space.begin()), code_space.size());
  std::unique_ptr<WasmCode> code{
      new WasmCode{this,                  // native_module
                   kAnonymousFuncIndex,   // index
                   code_space,            // instructions
                   0,                     // stack_slots
                   0,                     // tagged_parameter_slots
                   0,                     // safepoint_table_offset
                   jump_table_size,       // handler_table_offset
                   jump_table_size,       // constant_pool_offset
                   jump_table_size,       // code_comments_offset
                   jump_table_size,       // unpadded_binary_size
                   {},                    // protected_instructions
                   {},                    // reloc_info
                   {},                    // source_pos
                   WasmCode::kJumpTable,  // kind
                   ExecutionTier::kNone,  // tier
                   kNoDebugging}};        // for_debugging
  return PublishCode(std::move(code));
}

void NativeModule::UpdateCodeSize(size_t size, ExecutionTier tier,
                                  ForDebugging for_debugging) {
  if (for_debugging != kNoDebugging) return;
  // Count jump tables (ExecutionTier::kNone) for both Liftoff and TurboFan as
  // this is shared code.
  if (tier != ExecutionTier::kTurbofan) liftoff_code_size_.fetch_add(size);
  if (tier != ExecutionTier::kLiftoff) turbofan_code_size_.fetch_add(size);
}

void NativeModule::PatchJumpTablesLocked(uint32_t slot_index, Address target) {
  // The caller must hold the {allocation_mutex_}, thus we fail to lock it here.
  DCHECK(!allocation_mutex_.TryLock());

  CODE_SPACE_WRITE_SCOPE
  for (auto& code_space_data : code_space_data_) {
    DCHECK_IMPLIES(code_space_data.jump_table, code_space_data.far_jump_table);
    if (!code_space_data.jump_table) continue;
    PatchJumpTableLocked(code_space_data, slot_index, target);
  }
}

void NativeModule::PatchJumpTableLocked(const CodeSpaceData& code_space_data,
                                        uint32_t slot_index, Address target) {
  // The caller must hold the {allocation_mutex_}, thus we fail to lock it here.
  DCHECK(!allocation_mutex_.TryLock());

  DCHECK_NOT_NULL(code_space_data.jump_table);
  DCHECK_NOT_NULL(code_space_data.far_jump_table);

  DCHECK_LT(slot_index, module_->num_declared_functions);
  Address jump_table_slot =
      code_space_data.jump_table->instruction_start() +
      JumpTableAssembler::JumpSlotIndexToOffset(slot_index);
  uint32_t far_jump_table_offset = JumpTableAssembler::FarJumpSlotIndexToOffset(
      WasmCode::kRuntimeStubCount + slot_index);
  // Only pass the far jump table start if the far jump table actually has a
  // slot for this function index (i.e. does not only contain runtime stubs).
  bool has_far_jump_slot =
      far_jump_table_offset <
      code_space_data.far_jump_table->instructions().size();
  Address far_jump_table_start =
      code_space_data.far_jump_table->instruction_start();
  Address far_jump_table_slot =
      has_far_jump_slot ? far_jump_table_start + far_jump_table_offset
                        : kNullAddress;
  JumpTableAssembler::PatchJumpTableSlot(jump_table_slot, far_jump_table_slot,
                                         target);
}

void NativeModule::AddCodeSpace(
    base::AddressRegion region,
    const WasmCodeAllocator::OptionalLock& allocator_lock) {
  // Each code space must be at least twice as large as the overhead per code
  // space. Otherwise, we are wasting too much memory.
  DCHECK_GE(region.size(),
            2 * OverheadPerCodeSpace(module()->num_declared_functions));

#if defined(V8_OS_WIN64)
  // On some platforms, specifically Win64, we need to reserve some pages at
  // the beginning of an executable space.
  // See src/heap/spaces.cc, MemoryAllocator::InitializeCodePageAllocator() and
  // https://cs.chromium.org/chromium/src/components/crash/content/app/crashpad_win.cc?rcl=fd680447881449fba2edcf0589320e7253719212&l=204
  // for details.
  if (engine_->code_manager()
          ->CanRegisterUnwindInfoForNonABICompliantCodeRange()) {
    size_t size = Heap::GetCodeRangeReservedAreaSize();
    DCHECK_LT(0, size);
    Vector<byte> padding = code_allocator_.AllocateForCodeInRegion(
        this, size, region, allocator_lock);
    CHECK_EQ(reinterpret_cast<Address>(padding.begin()), region.begin());
    win64_unwindinfo::RegisterNonABICompliantCodeRange(
        reinterpret_cast<void*>(region.begin()), region.size());
  }
#endif  // V8_OS_WIN64

  WasmCodeRefScope code_ref_scope;
  CODE_SPACE_WRITE_SCOPE
  WasmCode* jump_table = nullptr;
  WasmCode* far_jump_table = nullptr;
  const uint32_t num_wasm_functions = module_->num_declared_functions;
  const bool is_first_code_space = code_space_data_.empty();
  // We always need a far jump table, because it contains the runtime stubs.
  const bool needs_far_jump_table = !FindJumpTablesForRegion(region).is_valid();
  const bool needs_jump_table = num_wasm_functions > 0 && needs_far_jump_table;

  if (needs_jump_table) {
    jump_table = CreateEmptyJumpTableInRegion(
        JumpTableAssembler::SizeForNumberOfSlots(num_wasm_functions), region,
        allocator_lock);
    CHECK(region.contains(jump_table->instruction_start()));
  }

  if (needs_far_jump_table) {
    int num_function_slots = NumWasmFunctionsInFarJumpTable(num_wasm_functions);
    far_jump_table = CreateEmptyJumpTableInRegion(
        JumpTableAssembler::SizeForNumberOfFarJumpSlots(
            WasmCode::kRuntimeStubCount,
            NumWasmFunctionsInFarJumpTable(num_function_slots)),
        region, allocator_lock);
    CHECK(region.contains(far_jump_table->instruction_start()));
    EmbeddedData embedded_data = EmbeddedData::FromBlob();
#define RUNTIME_STUB(Name) Builtins::k##Name,
#define RUNTIME_STUB_TRAP(Name) RUNTIME_STUB(ThrowWasm##Name)
    Builtins::Name stub_names[WasmCode::kRuntimeStubCount] = {
        WASM_RUNTIME_STUB_LIST(RUNTIME_STUB, RUNTIME_STUB_TRAP)};
#undef RUNTIME_STUB
#undef RUNTIME_STUB_TRAP
    STATIC_ASSERT(Builtins::kAllBuiltinsAreIsolateIndependent);
    Address builtin_addresses[WasmCode::kRuntimeStubCount];
    for (int i = 0; i < WasmCode::kRuntimeStubCount; ++i) {
      Builtins::Name builtin = stub_names[i];
      builtin_addresses[i] = embedded_data.InstructionStartOfBuiltin(builtin);
    }
    JumpTableAssembler::GenerateFarJumpTable(
        far_jump_table->instruction_start(), builtin_addresses,
        WasmCode::kRuntimeStubCount, num_function_slots);
  }

  if (is_first_code_space) {
    // This can be updated and accessed without locks, since the addition of the
    // first code space happens during initialization of the {NativeModule},
    // where no concurrent accesses are possible.
    main_jump_table_ = jump_table;
    main_far_jump_table_ = far_jump_table;
  }

  base::MutexGuard guard(&allocation_mutex_);
  code_space_data_.push_back(CodeSpaceData{region, jump_table, far_jump_table});

  if (jump_table && !is_first_code_space) {
    // Patch the new jump table(s) with existing functions. If this is the first
    // code space, there cannot be any functions that have been compiled yet.
    const CodeSpaceData& new_code_space_data = code_space_data_.back();
    for (uint32_t slot_index = 0; slot_index < num_wasm_functions;
         ++slot_index) {
      if (code_table_[slot_index]) {
        PatchJumpTableLocked(new_code_space_data, slot_index,
                             code_table_[slot_index]->instruction_start());
      } else if (lazy_compile_table_) {
        Address lazy_compile_target =
            lazy_compile_table_->instruction_start() +
            JumpTableAssembler::LazyCompileSlotIndexToOffset(slot_index);
        PatchJumpTableLocked(new_code_space_data, slot_index,
                             lazy_compile_target);
      }
    }
  }
}

namespace {
class NativeModuleWireBytesStorage final : public WireBytesStorage {
 public:
  explicit NativeModuleWireBytesStorage(
      std::shared_ptr<OwnedVector<const uint8_t>> wire_bytes)
      : wire_bytes_(std::move(wire_bytes)) {}

  Vector<const uint8_t> GetCode(WireBytesRef ref) const final {
    return std::atomic_load(&wire_bytes_)
        ->as_vector()
        .SubVector(ref.offset(), ref.end_offset());
  }

 private:
  const std::shared_ptr<OwnedVector<const uint8_t>> wire_bytes_;
};
}  // namespace

void NativeModule::SetWireBytes(OwnedVector<const uint8_t> wire_bytes) {
  auto shared_wire_bytes =
      std::make_shared<OwnedVector<const uint8_t>>(std::move(wire_bytes));
  std::atomic_store(&wire_bytes_, shared_wire_bytes);
  if (!shared_wire_bytes->empty()) {
    compilation_state_->SetWireBytesStorage(
        std::make_shared<NativeModuleWireBytesStorage>(
            std::move(shared_wire_bytes)));
  }
}

void NativeModule::TransferNewOwnedCodeLocked() const {
  // The caller holds the allocation mutex.
  DCHECK(!allocation_mutex_.TryLock());
  DCHECK(!new_owned_code_.empty());
  // Sort the {new_owned_code_} vector reversed, such that the position of the
  // previously inserted element can be used as a hint for the next element. If
  // elements in {new_owned_code_} are adjacent, this will guarantee
  // constant-time insertion into the map.
  std::sort(new_owned_code_.begin(), new_owned_code_.end(),
            [](const std::unique_ptr<WasmCode>& a,
               const std::unique_ptr<WasmCode>& b) {
              return a->instruction_start() > b->instruction_start();
            });
  auto insertion_hint = owned_code_.end();
  for (auto& code : new_owned_code_) {
    DCHECK_EQ(0, owned_code_.count(code->instruction_start()));
    // Check plausibility of the insertion hint.
    DCHECK(insertion_hint == owned_code_.end() ||
           insertion_hint->first > code->instruction_start());
    insertion_hint = owned_code_.emplace_hint(
        insertion_hint, code->instruction_start(), std::move(code));
  }
  new_owned_code_.clear();
}

WasmCode* NativeModule::Lookup(Address pc) const {
  base::MutexGuard lock(&allocation_mutex_);
  if (!new_owned_code_.empty()) TransferNewOwnedCodeLocked();
  auto iter = owned_code_.upper_bound(pc);
  if (iter == owned_code_.begin()) return nullptr;
  --iter;
  WasmCode* candidate = iter->second.get();
  DCHECK_EQ(candidate->instruction_start(), iter->first);
  if (!candidate->contains(pc)) return nullptr;
  WasmCodeRefScope::AddRef(candidate);
  return candidate;
}

uint32_t NativeModule::GetJumpTableOffset(uint32_t func_index) const {
  uint32_t slot_idx = declared_function_index(module(), func_index);
  return JumpTableAssembler::JumpSlotIndexToOffset(slot_idx);
}

Address NativeModule::GetCallTargetForFunction(uint32_t func_index) const {
  // Return the jump table slot for that function index.
  DCHECK_NOT_NULL(main_jump_table_);
  uint32_t slot_offset = GetJumpTableOffset(func_index);
  DCHECK_LT(slot_offset, main_jump_table_->instructions().size());
  return main_jump_table_->instruction_start() + slot_offset;
}

NativeModule::JumpTablesRef NativeModule::FindJumpTablesForRegion(
    base::AddressRegion code_region) const {
  auto jump_table_usable = [code_region](const WasmCode* jump_table) {
    Address table_start = jump_table->instruction_start();
    Address table_end = table_start + jump_table->instructions().size();
    // Compute the maximum distance from anywhere in the code region to anywhere
    // in the jump table, avoiding any underflow.
    size_t max_distance = std::max(
        code_region.end() > table_start ? code_region.end() - table_start : 0,
        table_end > code_region.begin() ? table_end - code_region.begin() : 0);
    // We can allow a max_distance that is equal to kMaxCodeSpaceSize, because
    // every call or jump will target an address *within* the region, but never
    // exactly the end of the region. So all occuring offsets are actually
    // smaller than max_distance.
    return max_distance <= WasmCodeAllocator::kMaxCodeSpaceSize;
  };

  // Fast path: Try to use {main_jump_table_} and {main_far_jump_table_}.
  // Access to these fields is possible without locking, since these fields are
  // initialized on construction of the {NativeModule}.
  if (main_far_jump_table_ && jump_table_usable(main_far_jump_table_) &&
      (main_jump_table_ == nullptr || jump_table_usable(main_jump_table_))) {
    return {
        main_jump_table_ ? main_jump_table_->instruction_start() : kNullAddress,
        main_far_jump_table_->instruction_start()};
  }

  // Otherwise, take the mutex and look for another suitable jump table.
  base::MutexGuard guard(&allocation_mutex_);
  for (auto& code_space_data : code_space_data_) {
    DCHECK_IMPLIES(code_space_data.jump_table, code_space_data.far_jump_table);
    if (!code_space_data.far_jump_table) continue;
    // Only return these jump tables if they are reachable from the whole
    // {code_region}.
    if (kNeedsFarJumpsBetweenCodeSpaces &&
        (!jump_table_usable(code_space_data.far_jump_table) ||
         (code_space_data.jump_table &&
          !jump_table_usable(code_space_data.jump_table)))) {
      continue;
    }
    return {code_space_data.jump_table
                ? code_space_data.jump_table->instruction_start()
                : kNullAddress,
            code_space_data.far_jump_table->instruction_start()};
  }
  return {};
}

Address NativeModule::GetNearCallTargetForFunction(
    uint32_t func_index, const JumpTablesRef& jump_tables) const {
  DCHECK(jump_tables.is_valid());
  uint32_t slot_offset = GetJumpTableOffset(func_index);
  return jump_tables.jump_table_start + slot_offset;
}

Address NativeModule::GetNearRuntimeStubEntry(
    WasmCode::RuntimeStubId index, const JumpTablesRef& jump_tables) const {
  DCHECK(jump_tables.is_valid());
  auto offset = JumpTableAssembler::FarJumpSlotIndexToOffset(index);
  return jump_tables.far_jump_table_start + offset;
}

uint32_t NativeModule::GetFunctionIndexFromJumpTableSlot(
    Address slot_address) const {
  WasmCodeRefScope code_refs;
  WasmCode* code = Lookup(slot_address);
  DCHECK_NOT_NULL(code);
  DCHECK_EQ(WasmCode::kJumpTable, code->kind());
  uint32_t slot_offset =
      static_cast<uint32_t>(slot_address - code->instruction_start());
  uint32_t slot_idx = JumpTableAssembler::SlotOffsetToIndex(slot_offset);
  DCHECK_LT(slot_idx, module_->num_declared_functions);
  DCHECK_EQ(slot_address,
            code->instruction_start() +
                JumpTableAssembler::JumpSlotIndexToOffset(slot_idx));
  return module_->num_imported_functions + slot_idx;
}

WasmCode::RuntimeStubId NativeModule::GetRuntimeStubId(Address target) const {
  base::MutexGuard guard(&allocation_mutex_);

  for (auto& code_space_data : code_space_data_) {
    if (code_space_data.far_jump_table != nullptr &&
        code_space_data.far_jump_table->contains(target)) {
      uint32_t offset = static_cast<uint32_t>(
          target - code_space_data.far_jump_table->instruction_start());
      uint32_t index = JumpTableAssembler::FarJumpSlotOffsetToIndex(offset);
      if (index >= WasmCode::kRuntimeStubCount) continue;
      if (JumpTableAssembler::FarJumpSlotIndexToOffset(index) != offset) {
        continue;
      }
      return static_cast<WasmCode::RuntimeStubId>(index);
    }
  }

  // Invalid address.
  return WasmCode::kRuntimeStubCount;
}

NativeModule::~NativeModule() {
  TRACE_HEAP("Deleting native module: %p\n", this);
  // Cancel all background compilation before resetting any field of the
  // NativeModule or freeing anything.
  compilation_state_->CancelCompilation();
  engine_->FreeNativeModule(this);
  // Free the import wrapper cache before releasing the {WasmCode} objects in
  // {owned_code_}. The destructor of {WasmImportWrapperCache} still needs to
  // decrease reference counts on the {WasmCode} objects.
  import_wrapper_cache_.reset();
}

WasmCodeManager::WasmCodeManager(size_t max_committed)
    : max_committed_code_space_(max_committed),
      critical_committed_code_space_(max_committed / 2) {
  DCHECK_LE(max_committed, FLAG_wasm_max_code_space * MB);
}

#if defined(V8_OS_WIN64)
bool WasmCodeManager::CanRegisterUnwindInfoForNonABICompliantCodeRange() const {
  return win64_unwindinfo::CanRegisterUnwindInfoForNonABICompliantCodeRange() &&
         FLAG_win64_unwinding_info;
}
#endif  // V8_OS_WIN64

void WasmCodeManager::Commit(base::AddressRegion region) {
  // TODO(v8:8462): Remove eager commit once perf supports remapping.
  if (V8_UNLIKELY(FLAG_perf_prof)) return;
  DCHECK(IsAligned(region.begin(), CommitPageSize()));
  DCHECK(IsAligned(region.size(), CommitPageSize()));
  // Reserve the size. Use CAS loop to avoid overflow on
  // {total_committed_code_space_}.
  size_t old_value = total_committed_code_space_.load();
  while (true) {
    DCHECK_GE(max_committed_code_space_, old_value);
    if (region.size() > max_committed_code_space_ - old_value) {
      V8::FatalProcessOutOfMemory(
          nullptr,
          "WasmCodeManager::Commit: Exceeding maximum wasm code space");
      UNREACHABLE();
    }
    if (total_committed_code_space_.compare_exchange_weak(
            old_value, old_value + region.size())) {
      break;
    }
  }
  PageAllocator::Permission permission = FLAG_wasm_write_protect_code_memory
                                             ? PageAllocator::kReadWrite
                                             : PageAllocator::kReadWriteExecute;

  TRACE_HEAP("Setting rw permissions for 0x%" PRIxPTR ":0x%" PRIxPTR "\n",
             region.begin(), region.end());

  if (!SetPermissions(GetPlatformPageAllocator(), region.begin(), region.size(),
                      permission)) {
    // Highly unlikely.
    V8::FatalProcessOutOfMemory(
        nullptr,
        "WasmCodeManager::Commit: Cannot make pre-reserved region writable");
    UNREACHABLE();
  }
}

void WasmCodeManager::Decommit(base::AddressRegion region) {
  // TODO(v8:8462): Remove this once perf supports remapping.
  if (V8_UNLIKELY(FLAG_perf_prof)) return;
  PageAllocator* allocator = GetPlatformPageAllocator();
  DCHECK(IsAligned(region.begin(), allocator->CommitPageSize()));
  DCHECK(IsAligned(region.size(), allocator->CommitPageSize()));
  size_t old_committed = total_committed_code_space_.fetch_sub(region.size());
  DCHECK_LE(region.size(), old_committed);
  USE(old_committed);
  TRACE_HEAP("Discarding system pages 0x%" PRIxPTR ":0x%" PRIxPTR "\n",
             region.begin(), region.end());
  CHECK(allocator->SetPermissions(reinterpret_cast<void*>(region.begin()),
                                  region.size(), PageAllocator::kNoAccess));
}

void WasmCodeManager::AssignRange(base::AddressRegion region,
                                  NativeModule* native_module) {
  base::MutexGuard lock(&native_modules_mutex_);
  lookup_map_.insert(std::make_pair(
      region.begin(), std::make_pair(region.end(), native_module)));
}

VirtualMemory WasmCodeManager::TryAllocate(size_t size, void* hint) {
  v8::PageAllocator* page_allocator = GetPlatformPageAllocator();
  DCHECK_GT(size, 0);
  size_t allocate_page_size = page_allocator->AllocatePageSize();
  size = RoundUp(size, allocate_page_size);
  if (!BackingStore::ReserveAddressSpace(size)) return {};
  if (hint == nullptr) hint = page_allocator->GetRandomMmapAddr();

  // When we start exposing Wasm in jitless mode, then the jitless flag
  // will have to determine whether we set kMapAsJittable or not.
  DCHECK(!FLAG_jitless);
  VirtualMemory mem(page_allocator, size, hint, allocate_page_size,
                    VirtualMemory::kMapAsJittable);
  if (!mem.IsReserved()) {
    BackingStore::ReleaseReservation(size);
    return {};
  }
  TRACE_HEAP("VMem alloc: 0x%" PRIxPTR ":0x%" PRIxPTR " (%zu)\n", mem.address(),
             mem.end(), mem.size());

  // TODO(v8:8462): Remove eager commit once perf supports remapping.
  if (FLAG_perf_prof) {
    SetPermissions(GetPlatformPageAllocator(), mem.address(), mem.size(),
                   PageAllocator::kReadWriteExecute);
  }
  return mem;
}

namespace {
// The numbers here are rough estimates, used to calculate the size of the
// initial code reservation and for estimating the amount of external memory
// reported to the GC.
// They do not need to be accurate. Choosing them too small will result in
// separate code spaces being allocated (compile time and runtime overhead),
// choosing them too large results in over-reservation (virtual address space
// only).
// The current numbers have been determined on 2019-11-11 by clemensb@, based
// on one small and one large module compiled from C++ by Emscripten. If in
// doubt, they where chosen slightly larger than required, as over-reservation
// is not a big issue currently.
// Numbers will change when Liftoff or TurboFan evolve, other toolchains are
// used to produce the wasm code, or characteristics of wasm modules on the
// web change. They might require occasional tuning.
// This patch might help to find reasonable numbers for any future adaptation:
// https://crrev.com/c/1910945
#if V8_TARGET_ARCH_X64
constexpr size_t kTurbofanFunctionOverhead = 20;
constexpr size_t kTurbofanCodeSizeMultiplier = 3;
constexpr size_t kLiftoffFunctionOverhead = 60;
constexpr size_t kLiftoffCodeSizeMultiplier = 4;
constexpr size_t kImportSize = 350;
#elif V8_TARGET_ARCH_IA32
constexpr size_t kTurbofanFunctionOverhead = 20;
constexpr size_t kTurbofanCodeSizeMultiplier = 4;
constexpr size_t kLiftoffFunctionOverhead = 60;
constexpr size_t kLiftoffCodeSizeMultiplier = 5;
constexpr size_t kImportSize = 480;
#elif V8_TARGET_ARCH_ARM
constexpr size_t kTurbofanFunctionOverhead = 40;
constexpr size_t kTurbofanCodeSizeMultiplier = 4;
constexpr size_t kLiftoffFunctionOverhead = 108;
constexpr size_t kLiftoffCodeSizeMultiplier = 7;
constexpr size_t kImportSize = 750;
#elif V8_TARGET_ARCH_ARM64
constexpr size_t kTurbofanFunctionOverhead = 60;
constexpr size_t kTurbofanCodeSizeMultiplier = 4;
constexpr size_t kLiftoffFunctionOverhead = 80;
constexpr size_t kLiftoffCodeSizeMultiplier = 7;
constexpr size_t kImportSize = 750;
#else
// Other platforms should add their own estimates if needed. Numbers below are
// the minimum of other architectures.
constexpr size_t kTurbofanFunctionOverhead = 20;
constexpr size_t kTurbofanCodeSizeMultiplier = 3;
constexpr size_t kLiftoffFunctionOverhead = 60;
constexpr size_t kLiftoffCodeSizeMultiplier = 4;
constexpr size_t kImportSize = 350;
#endif
}  // namespace

// static
size_t WasmCodeManager::EstimateLiftoffCodeSize(int body_size) {
  return kLiftoffFunctionOverhead + kCodeAlignment / 2 +
         body_size * kLiftoffCodeSizeMultiplier;
}

// static
size_t WasmCodeManager::EstimateNativeModuleCodeSize(const WasmModule* module,
                                                     bool include_liftoff) {
  int num_functions = static_cast<int>(module->num_declared_functions);
  int num_imported_functions = static_cast<int>(module->num_imported_functions);
  int code_section_length = 0;
  if (num_functions > 0) {
    DCHECK_EQ(module->functions.size(), num_imported_functions + num_functions);
    auto* first_fn = &module->functions[module->num_imported_functions];
    auto* last_fn = &module->functions.back();
    code_section_length =
        static_cast<int>(last_fn->code.end_offset() - first_fn->code.offset());
  }
  return EstimateNativeModuleCodeSize(num_functions, num_imported_functions,
                                      code_section_length, include_liftoff);
}

// static
size_t WasmCodeManager::EstimateNativeModuleCodeSize(int num_functions,
                                                     int num_imported_functions,
                                                     int code_section_length,
                                                     bool include_liftoff) {
  const size_t overhead_per_function =
      kTurbofanFunctionOverhead + kCodeAlignment / 2 +
      (include_liftoff ? kLiftoffFunctionOverhead + kCodeAlignment / 2 : 0);
  const size_t overhead_per_code_byte =
      kTurbofanCodeSizeMultiplier +
      (include_liftoff ? kLiftoffCodeSizeMultiplier : 0);
  const size_t jump_table_size = RoundUp<kCodeAlignment>(
      JumpTableAssembler::SizeForNumberOfSlots(num_functions));
  const size_t far_jump_table_size =
      RoundUp<kCodeAlignment>(JumpTableAssembler::SizeForNumberOfFarJumpSlots(
          WasmCode::kRuntimeStubCount,
          NumWasmFunctionsInFarJumpTable(num_functions)));
  return jump_table_size                                 // jump table
         + far_jump_table_size                           // far jump table
         + overhead_per_function * num_functions         // per function
         + overhead_per_code_byte * code_section_length  // per code byte
         + kImportSize * num_imported_functions;         // per import
}

// static
size_t WasmCodeManager::EstimateNativeModuleMetaDataSize(
    const WasmModule* module) {
  size_t wasm_module_estimate = EstimateStoredSize(module);

  uint32_t num_wasm_functions = module->num_declared_functions;

  // TODO(wasm): Include wire bytes size.
  size_t native_module_estimate =
      sizeof(NativeModule) +                     /* NativeModule struct */
      (sizeof(WasmCode*) * num_wasm_functions) + /* code table size */
      (sizeof(WasmCode) * num_wasm_functions);   /* code object size */

  return wasm_module_estimate + native_module_estimate;
}

std::shared_ptr<NativeModule> WasmCodeManager::NewNativeModule(
    WasmEngine* engine, Isolate* isolate, const WasmFeatures& enabled,
    size_t code_size_estimate, std::shared_ptr<const WasmModule> module) {
  DCHECK_EQ(this, isolate->wasm_engine()->code_manager());
  if (total_committed_code_space_.load() >
      critical_committed_code_space_.load()) {
    (reinterpret_cast<v8::Isolate*>(isolate))
        ->MemoryPressureNotification(MemoryPressureLevel::kCritical);
    size_t committed = total_committed_code_space_.load();
    DCHECK_GE(max_committed_code_space_, committed);
    critical_committed_code_space_.store(
        committed + (max_committed_code_space_ - committed) / 2);
  }

  // If we cannot add code space later, reserve enough address space up front.
  size_t code_vmem_size =
      ReservationSize(code_size_estimate, module->num_declared_functions, 0);

  // The '--wasm-max-code-space-reservation' testing flag can be used to reduce
  // the maximum size of the initial code space reservation (in MB).
  if (FLAG_wasm_max_initial_code_space_reservation > 0) {
    size_t flag_max_bytes =
        static_cast<size_t>(FLAG_wasm_max_initial_code_space_reservation) * MB;
    if (flag_max_bytes < code_vmem_size) code_vmem_size = flag_max_bytes;
  }

  // Try up to two times; getting rid of dead JSArrayBuffer allocations might
  // require two GCs because the first GC maybe incremental and may have
  // floating garbage.
  static constexpr int kAllocationRetries = 2;
  VirtualMemory code_space;
  for (int retries = 0;; ++retries) {
    code_space = TryAllocate(code_vmem_size);
    if (code_space.IsReserved()) break;
    if (retries == kAllocationRetries) {
      V8::FatalProcessOutOfMemory(isolate, "NewNativeModule");
      UNREACHABLE();
    }
    // Run one GC, then try the allocation again.
    isolate->heap()->MemoryPressureNotification(MemoryPressureLevel::kCritical,
                                                true);
  }

  Address start = code_space.address();
  size_t size = code_space.size();
  Address end = code_space.end();
  std::shared_ptr<NativeModule> ret;
  new NativeModule(engine, enabled, std::move(code_space), std::move(module),
                   isolate->async_counters(), &ret);
  // The constructor initialized the shared_ptr.
  DCHECK_NOT_NULL(ret);
  TRACE_HEAP("New NativeModule %p: Mem: %" PRIuPTR ",+%zu\n", ret.get(), start,
             size);

  base::MutexGuard lock(&native_modules_mutex_);
  lookup_map_.insert(std::make_pair(start, std::make_pair(end, ret.get())));
  return ret;
}

void NativeModule::SampleCodeSize(
    Counters* counters, NativeModule::CodeSamplingTime sampling_time) const {
  size_t code_size = sampling_time == kSampling
                         ? code_allocator_.committed_code_space()
                         : code_allocator_.generated_code_size();
  int code_size_mb = static_cast<int>(code_size / MB);
  Histogram* histogram = nullptr;
  switch (sampling_time) {
    case kAfterBaseline:
      histogram = counters->wasm_module_code_size_mb_after_baseline();
      break;
    case kAfterTopTier:
      histogram = counters->wasm_module_code_size_mb_after_top_tier();
      break;
    case kSampling: {
      histogram = counters->wasm_module_code_size_mb();
      // If this is a wasm module of >= 2MB, also sample the freed code size,
      // absolute and relative. Code GC does not happen on asm.js modules, and
      // small modules will never trigger GC anyway.
      size_t generated_size = code_allocator_.generated_code_size();
      if (generated_size >= 2 * MB && module()->origin == kWasmOrigin) {
        size_t freed_size = code_allocator_.freed_code_size();
        DCHECK_LE(freed_size, generated_size);
        int freed_percent = static_cast<int>(100 * freed_size / generated_size);
        counters->wasm_module_freed_code_size_percent()->AddSample(
            freed_percent);
      }
      break;
    }
  }
  histogram->AddSample(code_size_mb);
}

std::unique_ptr<WasmCode> NativeModule::AddCompiledCode(
    WasmCompilationResult result) {
  std::vector<std::unique_ptr<WasmCode>> code = AddCompiledCode({&result, 1});
  return std::move(code[0]);
}

std::vector<std::unique_ptr<WasmCode>> NativeModule::AddCompiledCode(
    Vector<WasmCompilationResult> results) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
               "wasm.AddCompiledCode", "num", results.size());
  DCHECK(!results.empty());
  // First, allocate code space for all the results.
  size_t total_code_space = 0;
  for (auto& result : results) {
    DCHECK(result.succeeded());
    total_code_space += RoundUp<kCodeAlignment>(result.code_desc.instr_size);
  }
  Vector<byte> code_space =
      code_allocator_.AllocateForCode(this, total_code_space);
  // Lookup the jump tables to use once, then use for all code objects.
  auto jump_tables = FindJumpTablesForRegion(base::AddressRegionOf(code_space));
  // If we happen to have a {total_code_space} which is bigger than
  // {kMaxCodeSpaceSize}, we would not find valid jump tables for the whole
  // region. If this ever happens, we need to handle this case (by splitting the
  // {results} vector in smaller chunks).
  CHECK(jump_tables.is_valid());

  std::vector<std::unique_ptr<WasmCode>> generated_code;
  generated_code.reserve(results.size());

  // Now copy the generated code into the code space and relocate it.
  CODE_SPACE_WRITE_SCOPE
  for (auto& result : results) {
    DCHECK_EQ(result.code_desc.buffer, result.instr_buffer.get());
    size_t code_size = RoundUp<kCodeAlignment>(result.code_desc.instr_size);
    Vector<byte> this_code_space = code_space.SubVector(0, code_size);
    code_space += code_size;
    generated_code.emplace_back(AddCodeWithCodeSpace(
        result.func_index, result.code_desc, result.frame_slot_count,
        result.tagged_parameter_slots,
        result.protected_instructions_data.as_vector(),
        result.source_positions.as_vector(), GetCodeKind(result),
        result.result_tier, result.for_debugging, this_code_space,
        jump_tables));
  }
  DCHECK_EQ(0, code_space.size());

  return generated_code;
}

void NativeModule::SetTieringState(TieringState new_tiering_state) {
  // Do not tier down asm.js (just never change the tiering state).
  if (module()->origin != kWasmOrigin) return;

  base::MutexGuard lock(&allocation_mutex_);
  tiering_state_ = new_tiering_state;
}

bool NativeModule::IsTieredDown() {
  base::MutexGuard lock(&allocation_mutex_);
  return tiering_state_ == kTieredDown;
}

void NativeModule::RecompileForTiering() {
  // Read the tiering state under the lock, then trigger recompilation after
  // releasing the lock. If the tiering state was changed when the triggered
  // compilation units finish, code installation will handle that correctly.
  TieringState current_state;
  {
    base::MutexGuard lock(&allocation_mutex_);
    current_state = tiering_state_;
  }
  RecompileNativeModule(this, current_state);
}

std::vector<int> NativeModule::FindFunctionsToRecompile(
    TieringState new_tiering_state) {
  base::MutexGuard guard(&allocation_mutex_);
  std::vector<int> function_indexes;
  int imported = module()->num_imported_functions;
  int declared = module()->num_declared_functions;
  for (int slot_index = 0; slot_index < declared; ++slot_index) {
    int function_index = imported + slot_index;
    WasmCode* code = code_table_[slot_index];
    bool code_is_good = new_tiering_state == kTieredDown
                            ? code && code->for_debugging()
                            : code && code->tier() == ExecutionTier::kTurbofan;
    if (!code_is_good) function_indexes.push_back(function_index);
  }
  return function_indexes;
}

void NativeModule::FreeCode(Vector<WasmCode* const> codes) {
  // Free the code space.
  code_allocator_.FreeCode(codes);

  DebugInfo* debug_info = nullptr;
  {
    base::MutexGuard guard(&allocation_mutex_);
    if (!new_owned_code_.empty()) TransferNewOwnedCodeLocked();
    debug_info = debug_info_.get();
    // Free the {WasmCode} objects. This will also unregister trap handler data.
    for (WasmCode* code : codes) {
      DCHECK_EQ(1, owned_code_.count(code->instruction_start()));
      owned_code_.erase(code->instruction_start());
    }
  }
  // Remove debug side tables for all removed code objects, after releasing our
  // lock. This is to avoid lock order inversion.
  if (debug_info) debug_info->RemoveDebugSideTables(codes);
}

size_t NativeModule::GetNumberOfCodeSpacesForTesting() const {
  return code_allocator_.GetNumCodeSpaces();
}

bool NativeModule::HasDebugInfo() const {
  base::MutexGuard guard(&allocation_mutex_);
  return debug_info_ != nullptr;
}

DebugInfo* NativeModule::GetDebugInfo() {
  base::MutexGuard guard(&allocation_mutex_);
  if (!debug_info_) debug_info_ = std::make_unique<DebugInfo>(this);
  return debug_info_.get();
}

void WasmCodeManager::FreeNativeModule(Vector<VirtualMemory> owned_code_space,
                                       size_t committed_size) {
  base::MutexGuard lock(&native_modules_mutex_);
  for (auto& code_space : owned_code_space) {
    DCHECK(code_space.IsReserved());
    TRACE_HEAP("VMem Release: 0x%" PRIxPTR ":0x%" PRIxPTR " (%zu)\n",
               code_space.address(), code_space.end(), code_space.size());

#if defined(V8_OS_WIN64)
    if (CanRegisterUnwindInfoForNonABICompliantCodeRange()) {
      win64_unwindinfo::UnregisterNonABICompliantCodeRange(
          reinterpret_cast<void*>(code_space.address()));
    }
#endif  // V8_OS_WIN64

    lookup_map_.erase(code_space.address());
    BackingStore::ReleaseReservation(code_space.size());
    code_space.Free();
    DCHECK(!code_space.IsReserved());
  }

  DCHECK(IsAligned(committed_size, CommitPageSize()));
  // TODO(v8:8462): Remove this once perf supports remapping.
  if (!FLAG_perf_prof) {
    size_t old_committed =
        total_committed_code_space_.fetch_sub(committed_size);
    DCHECK_LE(committed_size, old_committed);
    USE(old_committed);
  }
}

NativeModule* WasmCodeManager::LookupNativeModule(Address pc) const {
  base::MutexGuard lock(&native_modules_mutex_);
  if (lookup_map_.empty()) return nullptr;

  auto iter = lookup_map_.upper_bound(pc);
  if (iter == lookup_map_.begin()) return nullptr;
  --iter;
  Address region_start = iter->first;
  Address region_end = iter->second.first;
  NativeModule* candidate = iter->second.second;

  DCHECK_NOT_NULL(candidate);
  return region_start <= pc && pc < region_end ? candidate : nullptr;
}

WasmCode* WasmCodeManager::LookupCode(Address pc) const {
  NativeModule* candidate = LookupNativeModule(pc);
  return candidate ? candidate->Lookup(pc) : nullptr;
}

// TODO(v8:7424): Code protection scopes are not yet supported with shared code
// enabled and need to be revisited.
NativeModuleModificationScope::NativeModuleModificationScope(
    NativeModule* native_module)
    : native_module_(native_module) {
  if (FLAG_wasm_write_protect_code_memory && native_module_ &&
      (native_module_->modification_scope_depth_++) == 0) {
    bool success = native_module_->SetExecutable(false);
    CHECK(success);
  }
}

NativeModuleModificationScope::~NativeModuleModificationScope() {
  if (FLAG_wasm_write_protect_code_memory && native_module_ &&
      (native_module_->modification_scope_depth_--) == 1) {
    bool success = native_module_->SetExecutable(true);
    CHECK(success);
  }
}

namespace {
thread_local WasmCodeRefScope* current_code_refs_scope = nullptr;
}  // namespace

WasmCodeRefScope::WasmCodeRefScope()
    : previous_scope_(current_code_refs_scope) {
  current_code_refs_scope = this;
}

WasmCodeRefScope::~WasmCodeRefScope() {
  DCHECK_EQ(this, current_code_refs_scope);
  current_code_refs_scope = previous_scope_;
  WasmCode::DecrementRefCount(VectorOf(code_ptrs_));
}

// static
void WasmCodeRefScope::AddRef(WasmCode* code) {
  DCHECK_NOT_NULL(code);
  WasmCodeRefScope* current_scope = current_code_refs_scope;
  DCHECK_NOT_NULL(current_scope);
  current_scope->code_ptrs_.push_back(code);
  code->IncRef();
}

const char* GetRuntimeStubName(WasmCode::RuntimeStubId stub_id) {
#define RUNTIME_STUB_NAME(Name) #Name,
#define RUNTIME_STUB_NAME_TRAP(Name) "ThrowWasm" #Name,
  constexpr const char* runtime_stub_names[] = {WASM_RUNTIME_STUB_LIST(
      RUNTIME_STUB_NAME, RUNTIME_STUB_NAME_TRAP) "<unknown>"};
#undef RUNTIME_STUB_NAME
#undef RUNTIME_STUB_NAME_TRAP
  STATIC_ASSERT(arraysize(runtime_stub_names) ==
                WasmCode::kRuntimeStubCount + 1);

  DCHECK_GT(arraysize(runtime_stub_names), stub_id);
  return runtime_stub_names[stub_id];
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
#undef TRACE_HEAP
