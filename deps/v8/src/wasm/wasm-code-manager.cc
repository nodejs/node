// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-code-manager.h"

#include <iomanip>

#include "src/base/adapters.h"
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
#include "src/wasm/compilation-environment.h"
#include "src/wasm/function-compiler.h"
#include "src/wasm/jump-table-assembler.h"
#include "src/wasm/wasm-import-wrapper-cache.h"
#include "src/wasm/wasm-module-sourcemap.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-objects.h"

#if defined(V8_OS_WIN64)
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

base::AddressRegion DisjointAllocationPool::Merge(base::AddressRegion region) {
  auto dest_it = regions_.begin();
  auto dest_end = regions_.end();

  // Skip over dest regions strictly before {region}.
  while (dest_it != dest_end && dest_it->end() < region.begin()) ++dest_it;

  // After last dest region: insert and done.
  if (dest_it == dest_end) {
    regions_.push_back(region);
    return region;
  }

  // Adjacent (from below) to dest: merge and done.
  if (dest_it->begin() == region.end()) {
    base::AddressRegion merged_region{region.begin(),
                                      region.size() + dest_it->size()};
    DCHECK_EQ(merged_region.end(), dest_it->end());
    *dest_it = merged_region;
    return merged_region;
  }

  // Before dest: insert and done.
  if (dest_it->begin() > region.end()) {
    regions_.insert(dest_it, region);
    return region;
  }

  // Src is adjacent from above. Merge and check whether the merged region is
  // now adjacent to the next region.
  DCHECK_EQ(dest_it->end(), region.begin());
  dest_it->set_size(dest_it->size() + region.size());
  DCHECK_EQ(dest_it->end(), region.end());
  auto next_dest = dest_it;
  ++next_dest;
  if (next_dest != dest_end && dest_it->end() == next_dest->begin()) {
    dest_it->set_size(dest_it->size() + next_dest->size());
    DCHECK_EQ(dest_it->end(), next_dest->end());
    regions_.erase(next_dest);
  }
  return *dest_it;
}

base::AddressRegion DisjointAllocationPool::Allocate(size_t size) {
  return AllocateInRegion(size,
                          {kNullAddress, std::numeric_limits<size_t>::max()});
}

base::AddressRegion DisjointAllocationPool::AllocateInRegion(
    size_t size, base::AddressRegion region) {
  for (auto it = regions_.begin(), end = regions_.end(); it != end; ++it) {
    base::AddressRegion overlap = it->GetOverlap(region);
    if (size > overlap.size()) continue;
    base::AddressRegion ret{overlap.begin(), size};
    if (size == it->size()) {
      // We use the full region --> erase the region from {regions_}.
      regions_.erase(it);
    } else if (ret.begin() == it->begin()) {
      // We return a region at the start --> shrink remaining region from front.
      *it = base::AddressRegion{it->begin() + size, it->size() - size};
    } else if (ret.end() == it->end()) {
      // We return a region at the end --> shrink remaining region.
      *it = base::AddressRegion{it->begin(), it->size() - size};
    } else {
      // We return something in the middle --> split the remaining region.
      regions_.insert(
          it, base::AddressRegion{it->begin(), ret.begin() - it->begin()});
      *it = base::AddressRegion{ret.end(), it->end() - ret.end()};
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

uint32_t WasmCode::handler_table_size() const {
  DCHECK_GE(constant_pool_offset_, handler_table_offset_);
  return static_cast<uint32_t>(constant_pool_offset_ - handler_table_offset_);
}

Address WasmCode::code_comments() const {
  return instruction_start() + code_comments_offset_;
}

uint32_t WasmCode::code_comments_size() const {
  DCHECK_GE(unpadded_binary_size_, code_comments_offset_);
  return static_cast<uint32_t>(unpadded_binary_size_ - code_comments_offset_);
}

void WasmCode::RegisterTrapHandlerData() {
  DCHECK(!has_trap_handler_index());
  if (kind() != WasmCode::kFunction) return;
  if (protected_instructions_.empty()) return;

  Address base = instruction_start();

  size_t size = instructions().size();
  const int index =
      RegisterHandlerData(base, size, protected_instructions().size(),
                          protected_instructions().begin());

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

void WasmCode::LogCode(Isolate* isolate) const {
  DCHECK(ShouldBeLogged(isolate));
  if (IsAnonymous()) return;

  ModuleWireBytes wire_bytes(native_module()->wire_bytes());
  // TODO(herhut): Allow to log code without on-heap round-trip of the name.
  WireBytesRef name_ref =
      native_module()->module()->LookupFunctionName(wire_bytes, index());
  WasmName name_vec = wire_bytes.GetNameOrNull(name_ref);

  const std::string& source_map_url = native_module()->module()->source_map_url;
  auto load_wasm_source_map = isolate->wasm_load_source_map_callback();
  auto source_map = native_module()->GetWasmSourceMap();
  if (!source_map && !source_map_url.empty() && load_wasm_source_map) {
    HandleScope scope(isolate);
    v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
    Local<v8::String> source_map_str =
        load_wasm_source_map(v8_isolate, source_map_url.c_str());
    native_module()->SetWasmSourceMap(
        base::make_unique<WasmModuleSourceMap>(v8_isolate, source_map_str));
  }

  if (!name_vec.empty()) {
    HandleScope scope(isolate);
    MaybeHandle<String> maybe_name = isolate->factory()->NewStringFromUtf8(
        Vector<const char>::cast(name_vec));
    Handle<String> name;
    if (!maybe_name.ToHandle(&name)) {
      name = isolate->factory()->NewStringFromAsciiChecked("<name too long>");
    }
    int name_length;
    auto cname =
        name->ToCString(AllowNullsFlag::DISALLOW_NULLS,
                        RobustnessFlag::ROBUST_STRING_TRAVERSAL, &name_length);
    PROFILE(isolate,
            CodeCreateEvent(CodeEventListener::FUNCTION_TAG, this,
                            {cname.get(), static_cast<size_t>(name_length)}));
  } else {
    EmbeddedVector<char, 32> generated_name;
    int length = SNPrintF(generated_name, "wasm-function[%d]", index());
    generated_name.Truncate(length);
    PROFILE(isolate, CodeCreateEvent(CodeEventListener::FUNCTION_TAG, this,
                                     generated_name));
  }

  if (!source_positions().empty()) {
    LOG_CODE_EVENT(isolate, CodeLinePosInfoRecordEvent(instruction_start(),
                                                       source_positions()));
  }
}

void WasmCode::Validate() const {
#ifdef DEBUG
  // We expect certain relocation info modes to never appear in {WasmCode}
  // objects or to be restricted to a small set of valid values. Hence the
  // iteration below does not use a mask, but visits all relocation data.
  for (RelocIterator it(instructions(), reloc_info(), constant_pool());
       !it.done(); it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    switch (mode) {
      case RelocInfo::WASM_CALL: {
        Address target = it.rinfo()->wasm_call_address();
        DCHECK(native_module_->is_jump_table_slot(target));
        break;
      }
      case RelocInfo::WASM_STUB_CALL: {
        Address target = it.rinfo()->wasm_stub_call_address();
        WasmCode* code = native_module_->Lookup(target);
        CHECK_NOT_NULL(code);
#ifdef V8_EMBEDDED_BUILTINS
        CHECK_EQ(WasmCode::kJumpTable, code->kind());
        CHECK_EQ(native_module()->runtime_stub_table_, code);
        CHECK(code->contains(target));
#else
        CHECK_EQ(WasmCode::kRuntimeStub, code->kind());
        CHECK_EQ(target, code->instruction_start());
#endif
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
  if ((FLAG_print_wasm_code && kind() == kFunction) ||
      (FLAG_print_wasm_stub_code && kind() != kFunction) || FLAG_print_code) {
    Print(name);
  }
}

void WasmCode::Print(const char* name) const {
  StdoutStream os;
  os << "--- WebAssembly code ---\n";
  Disassemble(name, os);
  os << "--- End code ---\n";
}

void WasmCode::Disassemble(const char* name, std::ostream& os,
                           Address current_pc) const {
  if (name) os << "name: " << name << "\n";
  if (!IsAnonymous()) os << "index: " << index() << "\n";
  os << "kind: " << GetWasmCodeKindAsString(kind_) << "\n";
  os << "compiler: " << (is_liftoff() ? "Liftoff" : "TurboFan") << "\n";
  size_t padding = instructions().size() - unpadded_binary_size_;
  os << "Body (size = " << instructions().size() << " = "
     << unpadded_binary_size_ << " + " << padding << " padding)\n";

#ifdef ENABLE_DISASSEMBLER
  size_t instruction_size = unpadded_binary_size_;
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
    HandlerTable table(handler_table(), handler_table_size(),
                       HandlerTable::kReturnAddressBasedEncoding);
    os << "Exception Handler Table (size = " << table.NumberOfReturnEntries()
       << "):\n";
    table.HandlerTableReturnPrint(os);
    os << "\n";
  }

  if (!protected_instructions_.empty()) {
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
    SafepointTable table(instruction_start(), safepoint_table_offset_,
                         stack_slots_);
    os << "Safepoints (size = " << table.size() << ")\n";
    for (uint32_t i = 0; i < table.length(); i++) {
      uintptr_t pc_offset = table.GetPcOffset(i);
      os << reinterpret_cast<const void*>(instruction_start() + pc_offset);
      os << std::setw(6) << std::hex << pc_offset << "  " << std::dec;
      table.PrintEntry(i, os);
      os << " (sp -> fp)";
      SafepointEntry entry = table.GetEntry(i);
      if (entry.trampoline_pc() != -1) {
        os << " trampoline: " << std::hex << entry.trampoline_pc() << std::dec;
      }
      if (entry.has_deoptimization_index()) {
        os << " deopt: " << std::setw(6) << entry.deoptimization_index();
      }
      os << "\n";
    }
    os << "\n";
  }

  os << "RelocInfo (size = " << reloc_info_.size() << ")\n";
  for (RelocIterator it(instructions(), reloc_info(), constant_pool());
       !it.done(); it.next()) {
    it.rinfo()->Print(nullptr, os);
  }
  os << "\n";

  if (code_comments_size() > 0) {
    PrintCodeCommentsSection(os, code_comments(), code_comments_size());
  }
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
    case WasmCode::kRuntimeStub:
      return "runtime-stub";
    case WasmCode::kInterpreterEntry:
      return "interpreter entry";
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

WasmCodeAllocator::WasmCodeAllocator(WasmCodeManager* code_manager,
                                     VirtualMemory code_space,
                                     bool can_request_more,
                                     std::shared_ptr<Counters> async_counters)
    : code_manager_(code_manager),
      free_code_space_(code_space.region()),
      can_request_more_memory_(can_request_more),
      async_counters_(std::move(async_counters)) {
  owned_code_space_.reserve(can_request_more ? 4 : 1);
  owned_code_space_.emplace_back(std::move(code_space));
  async_counters_->wasm_module_num_code_spaces()->AddSample(1);
}

WasmCodeAllocator::~WasmCodeAllocator() {
  code_manager_->FreeNativeModule(VectorOf(owned_code_space_),
                                  committed_code_space());
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
}  // namespace

Vector<byte> WasmCodeAllocator::AllocateForCode(NativeModule* native_module,
                                                size_t size) {
  return AllocateForCodeInRegion(
      native_module, size, {kNullAddress, std::numeric_limits<size_t>::max()});
}

Vector<byte> WasmCodeAllocator::AllocateForCodeInRegion(
    NativeModule* native_module, size_t size, base::AddressRegion region) {
  base::MutexGuard lock(&mutex_);
  DCHECK_EQ(code_manager_, native_module->engine()->code_manager());
  DCHECK_LT(0, size);
  v8::PageAllocator* page_allocator = GetPlatformPageAllocator();
  size = RoundUp<kCodeAlignment>(size);
  base::AddressRegion code_space =
      free_code_space_.AllocateInRegion(size, region);
  if (code_space.is_empty()) {
    const bool in_specific_region =
        region.size() < std::numeric_limits<size_t>::max();
    if (!can_request_more_memory_ || in_specific_region) {
      auto error = in_specific_region ? "wasm code reservation in region"
                                      : "wasm code reservation";
      V8::FatalProcessOutOfMemory(nullptr, error);
      UNREACHABLE();
    }

    Address hint = owned_code_space_.empty() ? kNullAddress
                                             : owned_code_space_.back().end();

    // Reserve at least 20% of the total generated code size so far, and of
    // course at least {size}. Round up to the next power of two.
    size_t total_reserved = 0;
    for (auto& vmem : owned_code_space_) total_reserved += vmem.size();
    size_t reserve_size =
        base::bits::RoundUpToPowerOfTwo(std::max(size, total_reserved / 5));
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
    native_module->AddCodeSpace(new_region);

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
    committed_code_space_.fetch_add(commit_end - commit_start);
    // Committed code cannot grow bigger than maximum code space size.
    DCHECK_LE(committed_code_space_.load(), kMaxWasmCodeMemory);
    for (base::AddressRegion split_range : SplitRangeByReservationsIfNeeded(
             {commit_start, commit_end - commit_start}, owned_code_space_)) {
      if (!code_manager_->Commit(split_range)) {
        V8::FatalProcessOutOfMemory(nullptr, "wasm code commit");
        UNREACHABLE();
      }
    }
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
    // reservation. This is really just a problem when the NativeModule is
    // growable (meaning can_request_more_memory_). That's 32-bit in production,
    // or unittests.
    // For now, in that case, we commit at reserved memory granularity.
    // Technically, that may be a waste, because we may reserve more than we
    // use. On 32-bit though, the scarce resource is the address space -
    // committed or not.
    if (can_request_more_memory_) {
      for (auto& vmem : owned_code_space_) {
        if (!SetPermissions(page_allocator, vmem.address(), vmem.size(),
                            permission)) {
          return false;
        }
        TRACE_HEAP("Set %p:%p to executable:%d\n", vmem.address(), vmem.end(),
                   executable);
      }
      is_executable_ = executable;
      return true;
    }
#endif
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
  }
  is_executable_ = executable;
  return true;
}

void WasmCodeAllocator::FreeCode(Vector<WasmCode* const> codes) {
  // Zap code area and collect freed code regions.
  DisjointAllocationPool freed_regions;
  size_t code_size = 0;
  for (WasmCode* code : codes) {
    ZapCode(code->instruction_start(), code->instructions().size());
    FlushInstructionCache(code->instruction_start(),
                          code->instructions().size());
    code_size += code->instructions().size();
    freed_regions.Merge(base::AddressRegion{code->instruction_start(),
                                            code->instructions().size()});
  }
  freed_code_size_.fetch_add(code_size);

  // Merge {freed_regions} into {freed_code_space_} and discard full pages.
  base::MutexGuard guard(&mutex_);
  PageAllocator* allocator = GetPlatformPageAllocator();
  size_t commit_page_size = allocator->CommitPageSize();
  for (auto region : freed_regions.regions()) {
    auto merged_region = freed_code_space_.Merge(region);
    Address discard_start =
        std::max(RoundUp(merged_region.begin(), commit_page_size),
                 RoundDown(region.begin(), commit_page_size));
    Address discard_end =
        std::min(RoundDown(merged_region.end(), commit_page_size),
                 RoundUp(region.end(), commit_page_size));
    if (discard_start >= discard_end) continue;
    size_t discard_size = discard_end - discard_start;
    size_t old_committed = committed_code_space_.fetch_sub(discard_size);
    DCHECK_GE(old_committed, discard_size);
    USE(old_committed);
    for (base::AddressRegion split_range : SplitRangeByReservationsIfNeeded(
             {discard_start, discard_size}, owned_code_space_)) {
      code_manager_->Decommit(split_range);
    }
  }
}

base::AddressRegion WasmCodeAllocator::GetSingleCodeRegion() const {
  base::MutexGuard lock(&mutex_);
  DCHECK_EQ(1, owned_code_space_.size());
  return owned_code_space_[0].region();
}

NativeModule::NativeModule(WasmEngine* engine, const WasmFeatures& enabled,
                           bool can_request_more, VirtualMemory code_space,
                           std::shared_ptr<const WasmModule> module,
                           std::shared_ptr<Counters> async_counters,
                           std::shared_ptr<NativeModule>* shared_this)
    : code_allocator_(engine->code_manager(), std::move(code_space),
                      can_request_more, async_counters),
      enabled_features_(enabled),
      module_(std::move(module)),
      import_wrapper_cache_(std::unique_ptr<WasmImportWrapperCache>(
          new WasmImportWrapperCache())),
      engine_(engine),
      use_trap_handler_(trap_handler::IsTrapHandlerEnabled() ? kUseTrapHandler
                                                             : kNoTrapHandler) {
  // We receive a pointer to an empty {std::shared_ptr}, and install ourselve
  // there.
  DCHECK_NOT_NULL(shared_this);
  DCHECK_NULL(*shared_this);
  shared_this->reset(this);
  compilation_state_ =
      CompilationState::New(*shared_this, std::move(async_counters));
  DCHECK_NOT_NULL(module_);
  if (module_->num_declared_functions > 0) {
    code_table_.reset(new WasmCode* [module_->num_declared_functions] {});
  }
  AddCodeSpace(code_allocator_.GetSingleCodeRegion());
}

void NativeModule::ReserveCodeTableForTesting(uint32_t max_functions) {
  WasmCodeRefScope code_ref_scope;
  DCHECK_LE(num_functions(), max_functions);
  WasmCode** new_table = new WasmCode* [max_functions] {};
  if (module_->num_declared_functions > 0) {
    memcpy(new_table, code_table_.get(),
           module_->num_declared_functions * sizeof(*new_table));
  }
  code_table_.reset(new_table);

  CHECK_EQ(1, code_space_data_.size());
  // Re-allocate jump table.
  code_space_data_[0].jump_table = CreateEmptyJumpTableInRegion(
      JumpTableAssembler::SizeForNumberOfSlots(max_functions),
      code_space_data_[0].region);
  main_jump_table_ = code_space_data_[0].jump_table;
}

void NativeModule::LogWasmCodes(Isolate* isolate) {
  if (!WasmCode::ShouldBeLogged(isolate)) return;

  // TODO(titzer): we skip the logging of the import wrappers
  // here, but they should be included somehow.
  int start = module()->num_imported_functions;
  int end = start + module()->num_declared_functions;
  WasmCodeRefScope code_ref_scope;
  for (int func_index = start; func_index < end; ++func_index) {
    if (WasmCode* code = GetCode(func_index)) code->LogCode(isolate);
  }
}

CompilationEnv NativeModule::CreateCompilationEnv() const {
  return {module(), use_trap_handler_, kRuntimeExceptionSupport,
          enabled_features_};
}

WasmCode* NativeModule::AddCodeForTesting(Handle<Code> code) {
  return AddAndPublishAnonymousCode(code, WasmCode::kFunction);
}

void NativeModule::UseLazyStub(uint32_t func_index) {
  DCHECK_LE(module_->num_imported_functions, func_index);
  DCHECK_LT(func_index,
            module_->num_imported_functions + module_->num_declared_functions);

  if (!lazy_compile_table_) {
    uint32_t num_slots = module_->num_declared_functions;
    WasmCodeRefScope code_ref_scope;
    DCHECK_EQ(1, code_space_data_.size());
    lazy_compile_table_ = CreateEmptyJumpTableInRegion(
        JumpTableAssembler::SizeForNumberOfLazyFunctions(num_slots),
        code_space_data_[0].region);
    JumpTableAssembler::GenerateLazyCompileTable(
        lazy_compile_table_->instruction_start(), num_slots,
        module_->num_imported_functions,
        runtime_stub_entry(WasmCode::kWasmCompileLazy));
  }

  // Add jump table entry for jump to the lazy compile stub.
  uint32_t slot_index = func_index - module_->num_imported_functions;
  DCHECK_NE(runtime_stub_entry(WasmCode::kWasmCompileLazy), kNullAddress);
  Address lazy_compile_target =
      lazy_compile_table_->instruction_start() +
      JumpTableAssembler::LazyCompileSlotIndexToOffset(slot_index);
  JumpTableAssembler::PatchJumpTableSlot(main_jump_table_->instruction_start(),
                                         slot_index, lazy_compile_target,
                                         WasmCode::kFlushICache);
}

// TODO(mstarzinger): Remove {Isolate} parameter once {V8_EMBEDDED_BUILTINS}
// was removed and embedded builtins are no longer optional.
void NativeModule::SetRuntimeStubs(Isolate* isolate) {
  DCHECK_EQ(kNullAddress, runtime_stub_entries_[0]);  // Only called once.
#ifdef V8_EMBEDDED_BUILTINS
  WasmCodeRefScope code_ref_scope;
  DCHECK_EQ(1, code_space_data_.size());
  WasmCode* jump_table = CreateEmptyJumpTableInRegion(
      JumpTableAssembler::SizeForNumberOfStubSlots(WasmCode::kRuntimeStubCount),
      code_space_data_[0].region);
  Address base = jump_table->instruction_start();
  EmbeddedData embedded_data = EmbeddedData::FromBlob();
#define RUNTIME_STUB(Name) Builtins::k##Name,
#define RUNTIME_STUB_TRAP(Name) RUNTIME_STUB(ThrowWasm##Name)
  Builtins::Name wasm_runtime_stubs[WasmCode::kRuntimeStubCount] = {
      WASM_RUNTIME_STUB_LIST(RUNTIME_STUB, RUNTIME_STUB_TRAP)};
#undef RUNTIME_STUB
#undef RUNTIME_STUB_TRAP
  Address builtin_address[WasmCode::kRuntimeStubCount];
  for (int i = 0; i < WasmCode::kRuntimeStubCount; ++i) {
    Builtins::Name builtin = wasm_runtime_stubs[i];
    CHECK(embedded_data.ContainsBuiltin(builtin));
    builtin_address[i] = embedded_data.InstructionStartOfBuiltin(builtin);
    runtime_stub_entries_[i] =
        base + JumpTableAssembler::StubSlotIndexToOffset(i);
  }
  JumpTableAssembler::GenerateRuntimeStubTable(base, builtin_address,
                                               WasmCode::kRuntimeStubCount);
  DCHECK_NULL(runtime_stub_table_);
  runtime_stub_table_ = jump_table;
#else  // V8_EMBEDDED_BUILTINS
  HandleScope scope(isolate);
  WasmCodeRefScope code_ref_scope;
  USE(runtime_stub_table_);  // Actually unused, but avoids ifdef's in header.
#define COPY_BUILTIN(Name)                                        \
  runtime_stub_entries_[WasmCode::k##Name] =                      \
      AddAndPublishAnonymousCode(                                 \
          isolate->builtins()->builtin_handle(Builtins::k##Name), \
          WasmCode::kRuntimeStub, #Name)                          \
          ->instruction_start();
#define COPY_BUILTIN_TRAP(Name) COPY_BUILTIN(ThrowWasm##Name)
  WASM_RUNTIME_STUB_LIST(COPY_BUILTIN, COPY_BUILTIN_TRAP)
#undef COPY_BUILTIN_TRAP
#undef COPY_BUILTIN
#endif  // V8_EMBEDDED_BUILTINS
  DCHECK_NE(kNullAddress, runtime_stub_entries_[0]);
}

WasmCode* NativeModule::AddAndPublishAnonymousCode(Handle<Code> code,
                                                   WasmCode::Kind kind,
                                                   const char* name) {
  // For off-heap builtins, we create a copy of the off-heap instruction stream
  // instead of the on-heap code object containing the trampoline. Ensure that
  // we do not apply the on-heap reloc info to the off-heap instructions.
  const size_t relocation_size =
      code->is_off_heap_trampoline() ? 0 : code->relocation_size();
  OwnedVector<byte> reloc_info;
  if (relocation_size > 0) {
    reloc_info = OwnedVector<byte>::New(relocation_size);
    memcpy(reloc_info.start(), code->relocation_start(), relocation_size);
  }
  Handle<ByteArray> source_pos_table(code->SourcePositionTable(),
                                     code->GetIsolate());
  OwnedVector<byte> source_pos =
      OwnedVector<byte>::New(source_pos_table->length());
  if (source_pos_table->length() > 0) {
    source_pos_table->copy_out(0, source_pos.start(),
                               source_pos_table->length());
  }
  Vector<const byte> instructions(
      reinterpret_cast<byte*>(code->InstructionStart()),
      static_cast<size_t>(code->InstructionSize()));
  const uint32_t stack_slots = static_cast<uint32_t>(
      code->has_safepoint_info() ? code->stack_slots() : 0);

  // TODO(jgruber,v8:8758): Remove this translation. It exists only because
  // Code objects contains real offsets but WasmCode expects an offset of 0 to
  // mean 'empty'.
  const size_t safepoint_table_offset = static_cast<size_t>(
      code->has_safepoint_table() ? code->safepoint_table_offset() : 0);
  const size_t handler_table_offset =
      static_cast<size_t>(code->handler_table_offset());
  const size_t constant_pool_offset =
      static_cast<size_t>(code->constant_pool_offset());
  const size_t code_comments_offset =
      static_cast<size_t>(code->code_comments_offset());

  Vector<uint8_t> dst_code_bytes =
      code_allocator_.AllocateForCode(this, instructions.size());
  memcpy(dst_code_bytes.begin(), instructions.begin(), instructions.size());

  // Apply the relocation delta by iterating over the RelocInfo.
  intptr_t delta = reinterpret_cast<Address>(dst_code_bytes.begin()) -
                   code->InstructionStart();
  int mode_mask = RelocInfo::kApplyMask |
                  RelocInfo::ModeMask(RelocInfo::WASM_STUB_CALL);
  Address constant_pool_start =
      reinterpret_cast<Address>(dst_code_bytes.begin()) + constant_pool_offset;
  RelocIterator orig_it(*code, mode_mask);
  for (RelocIterator it(dst_code_bytes, reloc_info.as_vector(),
                        constant_pool_start, mode_mask);
       !it.done(); it.next(), orig_it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    if (RelocInfo::IsWasmStubCall(mode)) {
      uint32_t stub_call_tag = orig_it.rinfo()->wasm_call_tag();
      DCHECK_LT(stub_call_tag, WasmCode::kRuntimeStubCount);
      Address entry = runtime_stub_entry(
          static_cast<WasmCode::RuntimeStubId>(stub_call_tag));
      it.rinfo()->set_wasm_stub_call_address(entry, SKIP_ICACHE_FLUSH);
    } else {
      it.rinfo()->apply(delta);
    }
  }

  // Flush the i-cache after relocation.
  FlushInstructionCache(dst_code_bytes.begin(), dst_code_bytes.size());

  DCHECK_NE(kind, WasmCode::Kind::kInterpreterEntry);
  std::unique_ptr<WasmCode> new_code{new WasmCode{
      this,                                     // native_module
      kAnonymousFuncIndex,                      // index
      dst_code_bytes,                           // instructions
      stack_slots,                              // stack_slots
      0,                                        // tagged_parameter_slots
      safepoint_table_offset,                   // safepoint_table_offset
      handler_table_offset,                     // handler_table_offset
      constant_pool_offset,                     // constant_pool_offset
      code_comments_offset,                     // code_comments_offset
      instructions.size(),                      // unpadded_binary_size
      OwnedVector<ProtectedInstructionData>{},  // protected_instructions
      std::move(reloc_info),                    // reloc_info
      std::move(source_pos),                    // source positions
      kind,                                     // kind
      ExecutionTier::kNone}};                   // tier
  new_code->MaybePrint(name);
  new_code->Validate();

  return PublishCode(std::move(new_code));
}

std::unique_ptr<WasmCode> NativeModule::AddCode(
    uint32_t index, const CodeDesc& desc, uint32_t stack_slots,
    uint32_t tagged_parameter_slots,
    OwnedVector<trap_handler::ProtectedInstructionData> protected_instructions,
    OwnedVector<const byte> source_position_table, WasmCode::Kind kind,
    ExecutionTier tier) {
  return AddCodeWithCodeSpace(
      index, desc, stack_slots, tagged_parameter_slots,
      std::move(protected_instructions), std::move(source_position_table), kind,
      tier, code_allocator_.AllocateForCode(this, desc.instr_size));
}

std::unique_ptr<WasmCode> NativeModule::AddCodeWithCodeSpace(
    uint32_t index, const CodeDesc& desc, uint32_t stack_slots,
    uint32_t tagged_parameter_slots,
    OwnedVector<ProtectedInstructionData> protected_instructions,
    OwnedVector<const byte> source_position_table, WasmCode::Kind kind,
    ExecutionTier tier, Vector<uint8_t> dst_code_bytes) {
  OwnedVector<byte> reloc_info;
  if (desc.reloc_size > 0) {
    reloc_info = OwnedVector<byte>::New(desc.reloc_size);
    memcpy(reloc_info.start(), desc.buffer + desc.buffer_size - desc.reloc_size,
           desc.reloc_size);
  }

  // TODO(jgruber,v8:8758): Remove this translation. It exists only because
  // CodeDesc contains real offsets but WasmCode expects an offset of 0 to mean
  // 'empty'.
  const size_t safepoint_table_offset = static_cast<size_t>(
      desc.safepoint_table_size == 0 ? 0 : desc.safepoint_table_offset);
  const size_t handler_table_offset =
      static_cast<size_t>(desc.handler_table_offset);
  const size_t constant_pool_offset =
      static_cast<size_t>(desc.constant_pool_offset);
  const size_t code_comments_offset =
      static_cast<size_t>(desc.code_comments_offset);
  const size_t instr_size = static_cast<size_t>(desc.instr_size);

  memcpy(dst_code_bytes.begin(), desc.buffer,
         static_cast<size_t>(desc.instr_size));

  // Apply the relocation delta by iterating over the RelocInfo.
  intptr_t delta = dst_code_bytes.begin() - desc.buffer;
  int mode_mask = RelocInfo::kApplyMask |
                  RelocInfo::ModeMask(RelocInfo::WASM_CALL) |
                  RelocInfo::ModeMask(RelocInfo::WASM_STUB_CALL);
  Address constant_pool_start =
      reinterpret_cast<Address>(dst_code_bytes.begin()) + constant_pool_offset;
  for (RelocIterator it(dst_code_bytes, reloc_info.as_vector(),
                        constant_pool_start, mode_mask);
       !it.done(); it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    if (RelocInfo::IsWasmCall(mode)) {
      uint32_t call_tag = it.rinfo()->wasm_call_tag();
      Address target = GetCallTargetForFunction(call_tag);
      it.rinfo()->set_wasm_call_address(target, SKIP_ICACHE_FLUSH);
    } else if (RelocInfo::IsWasmStubCall(mode)) {
      uint32_t stub_call_tag = it.rinfo()->wasm_call_tag();
      DCHECK_LT(stub_call_tag, WasmCode::kRuntimeStubCount);
      Address entry = runtime_stub_entry(
          static_cast<WasmCode::RuntimeStubId>(stub_call_tag));
      it.rinfo()->set_wasm_stub_call_address(entry, SKIP_ICACHE_FLUSH);
    } else {
      it.rinfo()->apply(delta);
    }
  }

  // Flush the i-cache after relocation.
  FlushInstructionCache(dst_code_bytes.begin(), dst_code_bytes.size());

  std::unique_ptr<WasmCode> code{new WasmCode{
      this, index, dst_code_bytes, stack_slots, tagged_parameter_slots,
      safepoint_table_offset, handler_table_offset, constant_pool_offset,
      code_comments_offset, instr_size, std::move(protected_instructions),
      std::move(reloc_info), std::move(source_position_table), kind, tier}};
  code->MaybePrint();
  code->Validate();

  return code;
}

WasmCode* NativeModule::PublishCode(std::unique_ptr<WasmCode> code) {
  base::MutexGuard lock(&allocation_mutex_);
  return PublishCodeLocked(std::move(code));
}

WasmCode::Kind GetCodeKind(const WasmCompilationResult& result) {
  switch (result.kind) {
    case WasmCompilationResult::kWasmToJsWrapper:
      return WasmCode::Kind::kWasmToJsWrapper;
    case WasmCompilationResult::kInterpreterEntry:
      return WasmCode::Kind::kInterpreterEntry;
    case WasmCompilationResult::kFunction:
      return WasmCode::Kind::kFunction;
    default:
      UNREACHABLE();
  }
}

WasmCode* NativeModule::PublishCodeLocked(std::unique_ptr<WasmCode> code) {
  // The caller must hold the {allocation_mutex_}, thus we fail to lock it here.
  DCHECK(!allocation_mutex_.TryLock());

  if (!code->IsAnonymous() &&
      code->index() >= module_->num_imported_functions) {
    DCHECK_LT(code->index(), num_functions());

    code->RegisterTrapHandlerData();

    // Assume an order of execution tiers that represents the quality of their
    // generated code.
    static_assert(ExecutionTier::kNone < ExecutionTier::kInterpreter &&
                      ExecutionTier::kInterpreter < ExecutionTier::kLiftoff &&
                      ExecutionTier::kLiftoff < ExecutionTier::kTurbofan,
                  "Assume an order on execution tiers");

    // Update code table but avoid to fall back to less optimized code. We use
    // the new code if it was compiled with a higher tier.
    uint32_t slot_idx = code->index() - module_->num_imported_functions;
    WasmCode* prior_code = code_table_[slot_idx];
    bool update_code_table = !prior_code || prior_code->tier() < code->tier();
    if (update_code_table) {
      code_table_[slot_idx] = code.get();
      if (prior_code) {
        WasmCodeRefScope::AddRef(prior_code);
        // The code is added to the current {WasmCodeRefScope}, hence the ref
        // count cannot drop to zero here.
        CHECK(!prior_code->DecRef());
      }
    }

    // Populate optimized code to the jump table unless there is an active
    // redirection to the interpreter that should be preserved.
    DCHECK_IMPLIES(
        main_jump_table_ == nullptr,
        engine_->code_manager()->IsImplicitAllocationsDisabledForTesting());
    bool update_jump_table = update_code_table &&
                             !has_interpreter_redirection(code->index()) &&
                             main_jump_table_;

    // Ensure that interpreter entries always populate to the jump table.
    if (code->kind_ == WasmCode::Kind::kInterpreterEntry) {
      SetInterpreterRedirection(code->index());
      update_jump_table = true;
    }

    if (update_jump_table) {
      JumpTableAssembler::PatchJumpTableSlot(
          main_jump_table_->instruction_start(), slot_idx,
          code->instruction_start(), WasmCode::kFlushICache);
    }
  }
  WasmCodeRefScope::AddRef(code.get());
  WasmCode* result = code.get();
  owned_code_.emplace(result->instruction_start(), std::move(code));
  return result;
}

WasmCode* NativeModule::AddDeserializedCode(
    uint32_t index, Vector<const byte> instructions, uint32_t stack_slots,
    uint32_t tagged_parameter_slots, size_t safepoint_table_offset,
    size_t handler_table_offset, size_t constant_pool_offset,
    size_t code_comments_offset, size_t unpadded_binary_size,
    OwnedVector<ProtectedInstructionData> protected_instructions,
    OwnedVector<const byte> reloc_info,
    OwnedVector<const byte> source_position_table, WasmCode::Kind kind,
    ExecutionTier tier) {
  Vector<uint8_t> dst_code_bytes =
      code_allocator_.AllocateForCode(this, instructions.size());
  memcpy(dst_code_bytes.begin(), instructions.begin(), instructions.size());

  std::unique_ptr<WasmCode> code{new WasmCode{
      this, index, dst_code_bytes, stack_slots, tagged_parameter_slots,
      safepoint_table_offset, handler_table_offset, constant_pool_offset,
      code_comments_offset, unpadded_binary_size,
      std::move(protected_instructions), std::move(reloc_info),
      std::move(source_position_table), kind, tier}};

  // Note: we do not flush the i-cache here, since the code needs to be
  // relocated anyway. The caller is responsible for flushing the i-cache later.

  return PublishCode(std::move(code));
}

std::vector<WasmCode*> NativeModule::SnapshotCodeTable() const {
  base::MutexGuard lock(&allocation_mutex_);
  WasmCode** start = code_table_.get();
  WasmCode** end = start + module_->num_declared_functions;
  return std::vector<WasmCode*>{start, end};
}

WasmCode* NativeModule::GetCode(uint32_t index) const {
  base::MutexGuard guard(&allocation_mutex_);
  DCHECK_LT(index, num_functions());
  DCHECK_LE(module_->num_imported_functions, index);
  WasmCode* code = code_table_[index - module_->num_imported_functions];
  if (code) WasmCodeRefScope::AddRef(code);
  return code;
}

bool NativeModule::HasCode(uint32_t index) const {
  base::MutexGuard guard(&allocation_mutex_);
  DCHECK_LT(index, num_functions());
  DCHECK_LE(module_->num_imported_functions, index);
  return code_table_[index - module_->num_imported_functions] != nullptr;
}

void NativeModule::SetWasmSourceMap(
    std::unique_ptr<WasmModuleSourceMap> source_map) {
  source_map_ = std::move(source_map);
}

WasmModuleSourceMap* NativeModule::GetWasmSourceMap() const {
  return source_map_.get();
}

WasmCode* NativeModule::CreateEmptyJumpTableInRegion(
    uint32_t jump_table_size, base::AddressRegion region) {
  // Only call this if we really need a jump table.
  DCHECK_LT(0, jump_table_size);
  Vector<uint8_t> code_space =
      code_allocator_.AllocateForCodeInRegion(this, jump_table_size, region);
  DCHECK(!code_space.empty());
  ZapCode(reinterpret_cast<Address>(code_space.begin()), code_space.size());
  std::unique_ptr<WasmCode> code{new WasmCode{
      this,                                     // native_module
      kAnonymousFuncIndex,                      // index
      code_space,                               // instructions
      0,                                        // stack_slots
      0,                                        // tagged_parameter_slots
      0,                                        // safepoint_table_offset
      jump_table_size,                          // handler_table_offset
      jump_table_size,                          // constant_pool_offset
      jump_table_size,                          // code_comments_offset
      jump_table_size,                          // unpadded_binary_size
      OwnedVector<ProtectedInstructionData>{},  // protected_instructions
      OwnedVector<const uint8_t>{},             // reloc_info
      OwnedVector<const uint8_t>{},             // source_pos
      WasmCode::kJumpTable,                     // kind
      ExecutionTier::kNone}};                   // tier
  return PublishCode(std::move(code));
}

void NativeModule::AddCodeSpace(base::AddressRegion region) {
  // Each code space must be at least twice as large as the overhead per code
  // space. Otherwise, we are wasting too much memory.
  const bool is_first_code_space = code_space_data_.empty();
  const bool implicit_alloc_disabled =
      engine_->code_manager()->IsImplicitAllocationsDisabledForTesting();

#if defined(V8_OS_WIN64)
  // On some platforms, specifically Win64, we need to reserve some pages at
  // the beginning of an executable space.
  // See src/heap/spaces.cc, MemoryAllocator::InitializeCodePageAllocator() and
  // https://cs.chromium.org/chromium/src/components/crash/content/app/crashpad_win.cc?rcl=fd680447881449fba2edcf0589320e7253719212&l=204
  // for details.
  if (engine_->code_manager()
          ->CanRegisterUnwindInfoForNonABICompliantCodeRange() &&
      !implicit_alloc_disabled) {
    size_t size = Heap::GetCodeRangeReservedAreaSize();
    DCHECK_LT(0, size);
    Vector<byte> padding = code_allocator_.AllocateForCode(this, size);
    CHECK(region.contains(reinterpret_cast<Address>(padding.begin()),
                          padding.size()));
  }
#endif  // V8_OS_WIN64

  WasmCodeRefScope code_ref_scope;
  WasmCode* jump_table = nullptr;
  const uint32_t num_wasm_functions = module_->num_declared_functions;
  const bool has_functions = num_wasm_functions > 0;
  const bool needs_jump_table =
      has_functions && is_first_code_space && !implicit_alloc_disabled;

  if (needs_jump_table) {
    jump_table = CreateEmptyJumpTableInRegion(
        JumpTableAssembler::SizeForNumberOfSlots(num_wasm_functions), region);
    CHECK(region.contains(jump_table->instruction_start()));
  }

  if (is_first_code_space) main_jump_table_ = jump_table;

  code_space_data_.push_back(CodeSpaceData{region, jump_table});
}

namespace {
class NativeModuleWireBytesStorage final : public WireBytesStorage {
 public:
  explicit NativeModuleWireBytesStorage(
      std::shared_ptr<OwnedVector<const uint8_t>> wire_bytes)
      : wire_bytes_(std::move(wire_bytes)) {}

  Vector<const uint8_t> GetCode(WireBytesRef ref) const final {
    return wire_bytes_->as_vector().SubVector(ref.offset(), ref.end_offset());
  }

 private:
  const std::shared_ptr<OwnedVector<const uint8_t>> wire_bytes_;
};
}  // namespace

void NativeModule::SetWireBytes(OwnedVector<const uint8_t> wire_bytes) {
  auto shared_wire_bytes =
      std::make_shared<OwnedVector<const uint8_t>>(std::move(wire_bytes));
  wire_bytes_ = shared_wire_bytes;
  if (!shared_wire_bytes->empty()) {
    compilation_state_->SetWireBytesStorage(
        std::make_shared<NativeModuleWireBytesStorage>(
            std::move(shared_wire_bytes)));
  }
}

WasmCode* NativeModule::Lookup(Address pc) const {
  base::MutexGuard lock(&allocation_mutex_);
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
  uint32_t slot_idx = func_index - module_->num_imported_functions;
  DCHECK_GT(module_->num_declared_functions, slot_idx);
  return JumpTableAssembler::JumpSlotIndexToOffset(slot_idx);
}

Address NativeModule::GetCallTargetForFunction(uint32_t func_index) const {
  // Return the jump table slot for that function index.
  DCHECK_NOT_NULL(main_jump_table_);
  uint32_t slot_offset = GetJumpTableOffset(func_index);
  DCHECK_LT(slot_offset, main_jump_table_->instructions().size());
  return main_jump_table_->instruction_start() + slot_offset;
}

uint32_t NativeModule::GetFunctionIndexFromJumpTableSlot(
    Address slot_address) const {
  DCHECK(is_jump_table_slot(slot_address));
  uint32_t slot_offset = static_cast<uint32_t>(
      slot_address - main_jump_table_->instruction_start());
  uint32_t slot_idx = JumpTableAssembler::SlotOffsetToIndex(slot_offset);
  DCHECK_LT(slot_idx, module_->num_declared_functions);
  return module_->num_imported_functions + slot_idx;
}

const char* NativeModule::GetRuntimeStubName(Address runtime_stub_entry) const {
#define RETURN_NAME(Name)                                               \
  if (runtime_stub_entries_[WasmCode::k##Name] == runtime_stub_entry) { \
    return #Name;                                                       \
  }
#define RETURN_NAME_TRAP(Name) RETURN_NAME(ThrowWasm##Name)
  WASM_RUNTIME_STUB_LIST(RETURN_NAME, RETURN_NAME_TRAP)
#undef RETURN_NAME_TRAP
#undef RETURN_NAME
  return "<unknown>";
}

NativeModule::~NativeModule() {
  TRACE_HEAP("Deleting native module: %p\n", this);
  // Cancel all background compilation before resetting any field of the
  // NativeModule or freeing anything.
  compilation_state_->AbortCompilation();
  engine_->FreeNativeModule(this);
  // Free the import wrapper cache before releasing the {WasmCode} objects in
  // {owned_code_}. The destructor of {WasmImportWrapperCache} still needs to
  // decrease reference counts on the {WasmCode} objects.
  import_wrapper_cache_.reset();
}

WasmCodeManager::WasmCodeManager(WasmMemoryTracker* memory_tracker,
                                 size_t max_committed)
    : memory_tracker_(memory_tracker),
      max_committed_code_space_(max_committed),
      critical_committed_code_space_(max_committed / 2) {
  DCHECK_LE(max_committed, kMaxWasmCodeMemory);
}

#if defined(V8_OS_WIN64)
bool WasmCodeManager::CanRegisterUnwindInfoForNonABICompliantCodeRange() const {
  return win64_unwindinfo::CanRegisterUnwindInfoForNonABICompliantCodeRange() &&
         FLAG_win64_unwinding_info;
}
#endif  // V8_OS_WIN64

bool WasmCodeManager::Commit(base::AddressRegion region) {
  // TODO(v8:8462): Remove eager commit once perf supports remapping.
  if (FLAG_perf_prof) return true;
  DCHECK(IsAligned(region.begin(), CommitPageSize()));
  DCHECK(IsAligned(region.size(), CommitPageSize()));
  // Reserve the size. Use CAS loop to avoid overflow on
  // {total_committed_code_space_}.
  size_t old_value = total_committed_code_space_.load();
  while (true) {
    DCHECK_GE(max_committed_code_space_, old_value);
    if (region.size() > max_committed_code_space_ - old_value) return false;
    if (total_committed_code_space_.compare_exchange_weak(
            old_value, old_value + region.size())) {
      break;
    }
  }
  PageAllocator::Permission permission = FLAG_wasm_write_protect_code_memory
                                             ? PageAllocator::kReadWrite
                                             : PageAllocator::kReadWriteExecute;

  bool ret = SetPermissions(GetPlatformPageAllocator(), region.begin(),
                            region.size(), permission);
  TRACE_HEAP("Setting rw permissions for 0x%" PRIxPTR ":0x%" PRIxPTR "\n",
             region.begin(), region.end());

  if (!ret) {
    // Highly unlikely.
    total_committed_code_space_.fetch_sub(region.size());
    return false;
  }
  return true;
}

void WasmCodeManager::Decommit(base::AddressRegion region) {
  // TODO(v8:8462): Remove this once perf supports remapping.
  if (FLAG_perf_prof) return;
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
  if (!memory_tracker_->ReserveAddressSpace(size)) return {};
  if (hint == nullptr) hint = page_allocator->GetRandomMmapAddr();

  VirtualMemory mem(page_allocator, size, hint, allocate_page_size);
  if (!mem.IsReserved()) {
    memory_tracker_->ReleaseReservation(size);
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

void WasmCodeManager::SetMaxCommittedMemoryForTesting(size_t limit) {
  // This has to be set before committing any memory.
  DCHECK_EQ(0, total_committed_code_space_.load());
  max_committed_code_space_ = limit;
  critical_committed_code_space_.store(limit / 2);
}

// static
size_t WasmCodeManager::EstimateNativeModuleCodeSize(const WasmModule* module) {
  constexpr size_t kCodeSizeMultiplier = 4;
  constexpr size_t kCodeOverhead = 32;     // for prologue, stack check, ...
  constexpr size_t kStaticCodeSize = 512;  // runtime stubs, ...
  constexpr size_t kImportSize = 64 * kSystemPointerSize;

  size_t estimate = kStaticCodeSize;
  for (auto& function : module->functions) {
    estimate += kCodeOverhead + kCodeSizeMultiplier * function.code.length();
  }
  estimate +=
      JumpTableAssembler::SizeForNumberOfSlots(module->num_declared_functions);
  estimate += kImportSize * module->num_imported_functions;

  return estimate;
}

// static
size_t WasmCodeManager::EstimateNativeModuleNonCodeSize(
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
    size_t code_size_estimate, bool can_request_more,
    std::shared_ptr<const WasmModule> module) {
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

  // If the code must be contiguous, reserve enough address space up front.
  size_t code_vmem_size =
      kRequiresCodeRange ? kMaxWasmCodeMemory : code_size_estimate;
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
  new NativeModule(engine, enabled, can_request_more, std::move(code_space),
                   std::move(module), isolate->async_counters(), &ret);
  // The constructor initialized the shared_ptr.
  DCHECK_NOT_NULL(ret);
  TRACE_HEAP("New NativeModule %p: Mem: %" PRIuPTR ",+%zu\n", ret.get(), start,
             size);

#if defined(V8_OS_WIN64)
  if (CanRegisterUnwindInfoForNonABICompliantCodeRange() &&
      !implicit_allocations_disabled_for_testing_) {
    win64_unwindinfo::RegisterNonABICompliantCodeRange(
        reinterpret_cast<void*>(start), size);
  }
#endif  // V8_OS_WIN64

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
        int total_freed_mb = static_cast<int>(freed_size / MB);
        counters->wasm_module_freed_code_size_mb()->AddSample(total_freed_mb);
        int freed_percent = static_cast<int>(100 * freed_size / generated_size);
        counters->wasm_module_freed_code_size_percent()->AddSample(
            freed_percent);
      }
      break;
    }
  }
  histogram->AddSample(code_size_mb);
}

WasmCode* NativeModule::AddCompiledCode(WasmCompilationResult result) {
  return AddCompiledCode({&result, 1})[0];
}

std::vector<WasmCode*> NativeModule::AddCompiledCode(
    Vector<WasmCompilationResult> results) {
  DCHECK(!results.empty());
  // First, allocate code space for all the results.
  size_t total_code_space = 0;
  for (auto& result : results) {
    DCHECK(result.succeeded());
    total_code_space += RoundUp<kCodeAlignment>(result.code_desc.instr_size);
  }
  Vector<byte> code_space =
      code_allocator_.AllocateForCode(this, total_code_space);

  std::vector<std::unique_ptr<WasmCode>> generated_code;
  generated_code.reserve(results.size());

  // Now copy the generated code into the code space and relocate it.
  for (auto& result : results) {
    DCHECK_EQ(result.code_desc.buffer, result.instr_buffer.get());
    size_t code_size = RoundUp<kCodeAlignment>(result.code_desc.instr_size);
    Vector<byte> this_code_space = code_space.SubVector(0, code_size);
    code_space += code_size;
    generated_code.emplace_back(AddCodeWithCodeSpace(
        result.func_index, result.code_desc, result.frame_slot_count,
        result.tagged_parameter_slots, std::move(result.protected_instructions),
        std::move(result.source_positions), GetCodeKind(result),
        result.result_tier, this_code_space));
  }
  DCHECK_EQ(0, code_space.size());

  // Under the {allocation_mutex_}, publish the code. The published code is put
  // into the top-most surrounding {WasmCodeRefScope} by {PublishCodeLocked}.
  std::vector<WasmCode*> code_vector;
  code_vector.reserve(results.size());
  {
    base::MutexGuard lock(&allocation_mutex_);
    for (auto& result : generated_code)
      code_vector.push_back(PublishCodeLocked(std::move(result)));
  }

  return code_vector;
}

bool NativeModule::IsRedirectedToInterpreter(uint32_t func_index) {
  base::MutexGuard lock(&allocation_mutex_);
  return has_interpreter_redirection(func_index);
}

void NativeModule::FreeCode(Vector<WasmCode* const> codes) {
  // Free the code space.
  code_allocator_.FreeCode(codes);

  // Free the {WasmCode} objects. This will also unregister trap handler data.
  base::MutexGuard guard(&allocation_mutex_);
  for (WasmCode* code : codes) {
    DCHECK_EQ(1, owned_code_.count(code->instruction_start()));
    owned_code_.erase(code->instruction_start());
  }
}

void WasmCodeManager::FreeNativeModule(Vector<VirtualMemory> owned_code_space,
                                       size_t committed_size) {
  base::MutexGuard lock(&native_modules_mutex_);
  for (auto& code_space : owned_code_space) {
    DCHECK(code_space.IsReserved());
    TRACE_HEAP("VMem Release: 0x%" PRIxPTR ":0x%" PRIxPTR " (%zu)\n",
               code_space.address(), code_space.end(), code_space.size());

#if defined(V8_OS_WIN64)
    if (CanRegisterUnwindInfoForNonABICompliantCodeRange() &&
        !implicit_allocations_disabled_for_testing_) {
      win64_unwindinfo::UnregisterNonABICompliantCodeRange(
          reinterpret_cast<void*>(code_space.address()));
    }
#endif  // V8_OS_WIN64

    lookup_map_.erase(code_space.address());
    memory_tracker_->ReleaseReservation(code_space.size());
    code_space.Free();
    DCHECK(!code_space.IsReserved());
  }

  DCHECK(IsAligned(committed_size, CommitPageSize()));
  size_t old_committed = total_committed_code_space_.fetch_sub(committed_size);
  DCHECK_LE(committed_size, old_committed);
  USE(old_committed);
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
// enabled and need to be revisited to work with --wasm-shared-code as well.
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
  std::vector<WasmCode*> code_ptrs;
  code_ptrs.reserve(code_ptrs_.size());
  code_ptrs.assign(code_ptrs_.begin(), code_ptrs_.end());
  WasmCode::DecrementRefCount(VectorOf(code_ptrs));
}

// static
void WasmCodeRefScope::AddRef(WasmCode* code) {
  DCHECK_NOT_NULL(code);
  WasmCodeRefScope* current_scope = current_code_refs_scope;
  DCHECK_NOT_NULL(current_scope);
  auto entry = current_scope->code_ptrs_.insert(code);
  // If we added a new entry, increment the ref counter.
  if (entry.second) code->IncRef();
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
#undef TRACE_HEAP
