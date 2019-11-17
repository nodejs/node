// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/profiler/profile-generator.h"

#include <algorithm>

#include "src/objects/shared-function-info-inl.h"
#include "src/profiler/cpu-profiler.h"
#include "src/profiler/profile-generator-inl.h"
#include "src/tracing/trace-event.h"
#include "src/tracing/traced-value.h"

namespace v8 {
namespace internal {

void SourcePositionTable::SetPosition(int pc_offset, int line,
                                      int inlining_id) {
  DCHECK_GE(pc_offset, 0);
  DCHECK_GT(line, 0);  // The 1-based number of the source line.
  // It's possible that we map multiple source positions to a pc_offset in
  // optimized code. Usually these map to the same line, so there is no
  // difference here as we only store line number and not line/col in the form
  // of a script offset. Ignore any subsequent sets to the same offset.
  if (!pc_offsets_to_lines_.empty() &&
      pc_offsets_to_lines_.back().pc_offset == pc_offset) {
    return;
  }
  // Check that we are inserting in ascending order, so that the vector remains
  // sorted.
  DCHECK(pc_offsets_to_lines_.empty() ||
         pc_offsets_to_lines_.back().pc_offset < pc_offset);
  if (pc_offsets_to_lines_.empty() ||
      pc_offsets_to_lines_.back().line_number != line ||
      pc_offsets_to_lines_.back().inlining_id != inlining_id) {
    pc_offsets_to_lines_.push_back({pc_offset, line, inlining_id});
  }
}

int SourcePositionTable::GetSourceLineNumber(int pc_offset) const {
  if (pc_offsets_to_lines_.empty()) {
    return v8::CpuProfileNode::kNoLineNumberInfo;
  }
  auto it = std::lower_bound(
      pc_offsets_to_lines_.begin(), pc_offsets_to_lines_.end(),
      SourcePositionTuple{pc_offset, 0, SourcePosition::kNotInlined});
  if (it != pc_offsets_to_lines_.begin()) --it;
  return it->line_number;
}

int SourcePositionTable::GetInliningId(int pc_offset) const {
  if (pc_offsets_to_lines_.empty()) {
    return SourcePosition::kNotInlined;
  }
  auto it = std::lower_bound(
      pc_offsets_to_lines_.begin(), pc_offsets_to_lines_.end(),
      SourcePositionTuple{pc_offset, 0, SourcePosition::kNotInlined});
  if (it != pc_offsets_to_lines_.begin()) --it;
  return it->inlining_id;
}

void SourcePositionTable::print() const {
  base::OS::Print(" - source position table at %p\n", this);
  for (const SourcePositionTuple& pos_info : pc_offsets_to_lines_) {
    base::OS::Print("    %d --> line_number: %d inlining_id: %d\n",
                    pos_info.pc_offset, pos_info.line_number,
                    pos_info.inlining_id);
  }
}

const char* const CodeEntry::kWasmResourceNamePrefix = "wasm ";
const char* const CodeEntry::kEmptyResourceName = "";
const char* const CodeEntry::kEmptyBailoutReason = "";
const char* const CodeEntry::kNoDeoptReason = "";

const char* const CodeEntry::kProgramEntryName = "(program)";
const char* const CodeEntry::kIdleEntryName = "(idle)";
const char* const CodeEntry::kGarbageCollectorEntryName = "(garbage collector)";
const char* const CodeEntry::kUnresolvedFunctionName = "(unresolved function)";
const char* const CodeEntry::kRootEntryName = "(root)";

base::LazyDynamicInstance<CodeEntry, CodeEntry::ProgramEntryCreateTrait>::type
    CodeEntry::kProgramEntry = LAZY_DYNAMIC_INSTANCE_INITIALIZER;

base::LazyDynamicInstance<CodeEntry, CodeEntry::IdleEntryCreateTrait>::type
    CodeEntry::kIdleEntry = LAZY_DYNAMIC_INSTANCE_INITIALIZER;

base::LazyDynamicInstance<CodeEntry, CodeEntry::GCEntryCreateTrait>::type
    CodeEntry::kGCEntry = LAZY_DYNAMIC_INSTANCE_INITIALIZER;

base::LazyDynamicInstance<CodeEntry,
                          CodeEntry::UnresolvedEntryCreateTrait>::type
    CodeEntry::kUnresolvedEntry = LAZY_DYNAMIC_INSTANCE_INITIALIZER;

base::LazyDynamicInstance<CodeEntry, CodeEntry::RootEntryCreateTrait>::type
    CodeEntry::kRootEntry = LAZY_DYNAMIC_INSTANCE_INITIALIZER;

CodeEntry* CodeEntry::ProgramEntryCreateTrait::Create() {
  return new CodeEntry(CodeEventListener::FUNCTION_TAG,
                       CodeEntry::kProgramEntryName);
}

CodeEntry* CodeEntry::IdleEntryCreateTrait::Create() {
  return new CodeEntry(CodeEventListener::FUNCTION_TAG,
                       CodeEntry::kIdleEntryName);
}

CodeEntry* CodeEntry::GCEntryCreateTrait::Create() {
  return new CodeEntry(CodeEventListener::BUILTIN_TAG,
                       CodeEntry::kGarbageCollectorEntryName);
}

CodeEntry* CodeEntry::UnresolvedEntryCreateTrait::Create() {
  return new CodeEntry(CodeEventListener::FUNCTION_TAG,
                       CodeEntry::kUnresolvedFunctionName);
}

CodeEntry* CodeEntry::RootEntryCreateTrait::Create() {
  return new CodeEntry(CodeEventListener::FUNCTION_TAG,
                       CodeEntry::kRootEntryName);
}

uint32_t CodeEntry::GetHash() const {
  uint32_t hash = ComputeUnseededHash(tag());
  if (script_id_ != v8::UnboundScript::kNoScriptId) {
    hash ^= ComputeUnseededHash(static_cast<uint32_t>(script_id_));
    hash ^= ComputeUnseededHash(static_cast<uint32_t>(position_));
  } else {
    hash ^= ComputeUnseededHash(
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(name_)));
    hash ^= ComputeUnseededHash(
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(resource_name_)));
    hash ^= ComputeUnseededHash(line_number_);
  }
  return hash;
}

bool CodeEntry::IsSameFunctionAs(const CodeEntry* entry) const {
  if (this == entry) return true;
  if (script_id_ != v8::UnboundScript::kNoScriptId) {
    return script_id_ == entry->script_id_ && position_ == entry->position_;
  }
  return name_ == entry->name_ && resource_name_ == entry->resource_name_ &&
         line_number_ == entry->line_number_;
}


void CodeEntry::SetBuiltinId(Builtins::Name id) {
  bit_field_ = TagField::update(bit_field_, CodeEventListener::BUILTIN_TAG);
  bit_field_ = BuiltinIdField::update(bit_field_, id);
}


int CodeEntry::GetSourceLine(int pc_offset) const {
  if (line_info_) return line_info_->GetSourceLineNumber(pc_offset);
  return v8::CpuProfileNode::kNoLineNumberInfo;
}

void CodeEntry::SetInlineStacks(
    std::unordered_set<std::unique_ptr<CodeEntry>, Hasher, Equals>
        inline_entries,
    std::unordered_map<int, std::vector<CodeEntryAndLineNumber>>
        inline_stacks) {
  EnsureRareData()->inline_entries_ = std::move(inline_entries);
  rare_data_->inline_stacks_ = std::move(inline_stacks);
}

const std::vector<CodeEntryAndLineNumber>* CodeEntry::GetInlineStack(
    int pc_offset) const {
  if (!line_info_) return nullptr;

  int inlining_id = line_info_->GetInliningId(pc_offset);
  if (inlining_id == SourcePosition::kNotInlined) return nullptr;
  DCHECK(rare_data_);

  auto it = rare_data_->inline_stacks_.find(inlining_id);
  return it != rare_data_->inline_stacks_.end() ? &it->second : nullptr;
}

void CodeEntry::set_deopt_info(
    const char* deopt_reason, int deopt_id,
    std::vector<CpuProfileDeoptFrame> inlined_frames) {
  DCHECK(!has_deopt_info());
  RareData* rare_data = EnsureRareData();
  rare_data->deopt_reason_ = deopt_reason;
  rare_data->deopt_id_ = deopt_id;
  rare_data->deopt_inlined_frames_ = std::move(inlined_frames);
}

void CodeEntry::FillFunctionInfo(SharedFunctionInfo shared) {
  if (!shared.script().IsScript()) return;
  Script script = Script::cast(shared.script());
  set_script_id(script.id());
  set_position(shared.StartPosition());
  if (shared.optimization_disabled()) {
    set_bailout_reason(GetBailoutReason(shared.disable_optimization_reason()));
  }
}

CpuProfileDeoptInfo CodeEntry::GetDeoptInfo() {
  DCHECK(has_deopt_info());

  CpuProfileDeoptInfo info;
  info.deopt_reason = rare_data_->deopt_reason_;
  DCHECK_NE(kNoDeoptimizationId, rare_data_->deopt_id_);
  if (rare_data_->deopt_inlined_frames_.empty()) {
    info.stack.push_back(CpuProfileDeoptFrame(
        {script_id_, static_cast<size_t>(std::max(0, position()))}));
  } else {
    info.stack = rare_data_->deopt_inlined_frames_;
  }
  return info;
}

CodeEntry::RareData* CodeEntry::EnsureRareData() {
  if (!rare_data_) {
    rare_data_.reset(new RareData());
  }
  return rare_data_.get();
}

void CodeEntry::print() const {
  base::OS::Print("CodeEntry: at %p\n", this);

  base::OS::Print(" - name: %s\n", name_);
  base::OS::Print(" - resource_name: %s\n", resource_name_);
  base::OS::Print(" - line_number: %d\n", line_number_);
  base::OS::Print(" - column_number: %d\n", column_number_);
  base::OS::Print(" - script_id: %d\n", script_id_);
  base::OS::Print(" - position: %d\n", position_);
  base::OS::Print(" - instruction_start: %p\n",
                  reinterpret_cast<void*>(instruction_start_));

  if (line_info_) {
    line_info_->print();
  }

  if (rare_data_) {
    base::OS::Print(" - deopt_reason: %s\n", rare_data_->deopt_reason_);
    base::OS::Print(" - bailout_reason: %s\n", rare_data_->bailout_reason_);
    base::OS::Print(" - deopt_id: %d\n", rare_data_->deopt_id_);

    if (!rare_data_->inline_stacks_.empty()) {
      base::OS::Print(" - inline stacks:\n");
      for (auto it = rare_data_->inline_stacks_.begin();
           it != rare_data_->inline_stacks_.end(); it++) {
        base::OS::Print("    inlining_id: [%d]\n", it->first);
        for (const auto& e : it->second) {
          base::OS::Print("     %s --> %d\n", e.code_entry->name(),
                          e.line_number);
        }
      }
    } else {
      base::OS::Print(" - inline stacks: (empty)\n");
    }

    if (!rare_data_->deopt_inlined_frames_.empty()) {
      base::OS::Print(" - deopt inlined frames:\n");
      for (const CpuProfileDeoptFrame& frame :
           rare_data_->deopt_inlined_frames_) {
        base::OS::Print("script_id: %d position: %zu\n", frame.script_id,
                        frame.position);
      }
    } else {
      base::OS::Print(" - deopt inlined frames: (empty)\n");
    }
  }
  base::OS::Print("\n");
}

void ProfileNode::CollectDeoptInfo(CodeEntry* entry) {
  deopt_infos_.push_back(entry->GetDeoptInfo());
  entry->clear_deopt_info();
}

ProfileNode* ProfileNode::FindChild(CodeEntry* entry, int line_number) {
  auto map_entry = children_.find({entry, line_number});
  return map_entry != children_.end() ? map_entry->second : nullptr;
}

ProfileNode* ProfileNode::FindOrAddChild(CodeEntry* entry, int line_number) {
  auto map_entry = children_.find({entry, line_number});
  if (map_entry == children_.end()) {
    ProfileNode* node = new ProfileNode(tree_, entry, this, line_number);
    children_[{entry, line_number}] = node;
    children_list_.push_back(node);
    return node;
  } else {
    return map_entry->second;
  }
}


void ProfileNode::IncrementLineTicks(int src_line) {
  if (src_line == v8::CpuProfileNode::kNoLineNumberInfo) return;
  // Increment a hit counter of a certain source line.
  // Add a new source line if not found.
  auto map_entry = line_ticks_.find(src_line);
  if (map_entry == line_ticks_.end()) {
    line_ticks_[src_line] = 1;
  } else {
    line_ticks_[src_line]++;
  }
}


bool ProfileNode::GetLineTicks(v8::CpuProfileNode::LineTick* entries,
                               unsigned int length) const {
  if (entries == nullptr || length == 0) return false;

  unsigned line_count = static_cast<unsigned>(line_ticks_.size());

  if (line_count == 0) return true;
  if (length < line_count) return false;

  v8::CpuProfileNode::LineTick* entry = entries;

  for (auto p = line_ticks_.begin(); p != line_ticks_.end(); p++, entry++) {
    entry->line = p->first;
    entry->hit_count = p->second;
  }

  return true;
}


void ProfileNode::Print(int indent) {
  int line_number = line_number_ != 0 ? line_number_ : entry_->line_number();
  base::OS::Print("%5u %*s %s:%d %d %d #%d", self_ticks_, indent, "",
                  entry_->name(), line_number, source_type(),
                  entry_->script_id(), id());
  if (entry_->resource_name()[0] != '\0')
    base::OS::Print(" %s:%d", entry_->resource_name(), entry_->line_number());
  base::OS::Print("\n");
  for (size_t i = 0; i < deopt_infos_.size(); ++i) {
    CpuProfileDeoptInfo& info = deopt_infos_[i];
    base::OS::Print(
        "%*s;;; deopted at script_id: %d position: %zu with reason '%s'.\n",
        indent + 10, "", info.stack[0].script_id, info.stack[0].position,
        info.deopt_reason);
    for (size_t index = 1; index < info.stack.size(); ++index) {
      base::OS::Print("%*s;;;     Inline point: script_id %d position: %zu.\n",
                      indent + 10, "", info.stack[index].script_id,
                      info.stack[index].position);
    }
  }
  const char* bailout_reason = entry_->bailout_reason();
  if (bailout_reason != GetBailoutReason(BailoutReason::kNoReason) &&
      bailout_reason != CodeEntry::kEmptyBailoutReason) {
    base::OS::Print("%*s bailed out due to '%s'\n", indent + 10, "",
                    bailout_reason);
  }
  for (auto child : children_) {
    child.second->Print(indent + 2);
  }
}


class DeleteNodesCallback {
 public:
  void BeforeTraversingChild(ProfileNode*, ProfileNode*) { }

  void AfterAllChildrenTraversed(ProfileNode* node) {
    delete node;
  }

  void AfterChildTraversed(ProfileNode*, ProfileNode*) { }
};

ProfileTree::ProfileTree(Isolate* isolate)
    : next_node_id_(1),
      root_(new ProfileNode(this, CodeEntry::root_entry(), nullptr)),
      isolate_(isolate),
      next_function_id_(1) {}

ProfileTree::~ProfileTree() {
  DeleteNodesCallback cb;
  TraverseDepthFirst(&cb);
}


unsigned ProfileTree::GetFunctionId(const ProfileNode* node) {
  CodeEntry* code_entry = node->entry();
  auto map_entry = function_ids_.find(code_entry);
  if (map_entry == function_ids_.end()) {
    return function_ids_[code_entry] = next_function_id_++;
  }
  return function_ids_[code_entry];
}

ProfileNode* ProfileTree::AddPathFromEnd(const std::vector<CodeEntry*>& path,
                                         int src_line, bool update_stats) {
  ProfileNode* node = root_;
  CodeEntry* last_entry = nullptr;
  for (auto it = path.rbegin(); it != path.rend(); ++it) {
    if (*it == nullptr) continue;
    last_entry = *it;
    node = node->FindOrAddChild(*it, v8::CpuProfileNode::kNoLineNumberInfo);
  }
  if (last_entry && last_entry->has_deopt_info()) {
    node->CollectDeoptInfo(last_entry);
  }
  if (update_stats) {
    node->IncrementSelfTicks();
    if (src_line != v8::CpuProfileNode::kNoLineNumberInfo) {
      node->IncrementLineTicks(src_line);
    }
  }
  return node;
}

ProfileNode* ProfileTree::AddPathFromEnd(const ProfileStackTrace& path,
                                         int src_line, bool update_stats,
                                         ProfilingMode mode,
                                         ContextFilter* context_filter) {
  ProfileNode* node = root_;
  CodeEntry* last_entry = nullptr;
  int parent_line_number = v8::CpuProfileNode::kNoLineNumberInfo;
  for (auto it = path.rbegin(); it != path.rend(); ++it) {
    if (it->entry.code_entry == nullptr) continue;
    if (context_filter && !context_filter->Accept(*it)) continue;
    last_entry = (*it).entry.code_entry;
    node = node->FindOrAddChild((*it).entry.code_entry, parent_line_number);
    parent_line_number = mode == ProfilingMode::kCallerLineNumbers
                             ? (*it).entry.line_number
                             : v8::CpuProfileNode::kNoLineNumberInfo;
  }
  if (last_entry && last_entry->has_deopt_info()) {
    node->CollectDeoptInfo(last_entry);
  }
  if (update_stats) {
    node->IncrementSelfTicks();
    if (src_line != v8::CpuProfileNode::kNoLineNumberInfo) {
      node->IncrementLineTicks(src_line);
    }
  }
  return node;
}

class Position {
 public:
  explicit Position(ProfileNode* node)
      : node(node), child_idx_(0) { }
  V8_INLINE ProfileNode* current_child() {
    return node->children()->at(child_idx_);
  }
  V8_INLINE bool has_current_child() {
    return child_idx_ < static_cast<int>(node->children()->size());
  }
  V8_INLINE void next_child() { ++child_idx_; }

  ProfileNode* node;
 private:
  int child_idx_;
};


// Non-recursive implementation of a depth-first post-order tree traversal.
template <typename Callback>
void ProfileTree::TraverseDepthFirst(Callback* callback) {
  std::vector<Position> stack;
  stack.emplace_back(root_);
  while (stack.size() > 0) {
    Position& current = stack.back();
    if (current.has_current_child()) {
      callback->BeforeTraversingChild(current.node, current.current_child());
      stack.emplace_back(current.current_child());
    } else {
      callback->AfterAllChildrenTraversed(current.node);
      if (stack.size() > 1) {
        Position& parent = stack[stack.size() - 2];
        callback->AfterChildTraversed(parent.node, current.node);
        parent.next_child();
      }
      // Remove child from the stack.
      stack.pop_back();
    }
  }
}

bool ContextFilter::Accept(const ProfileStackFrame& frame) {
  // If a frame should always be included in profiles (e.g. metadata frames),
  // skip the context check.
  if (!frame.filterable) return true;

  // Strip heap object tag from frame.
  return (frame.native_context & ~kHeapObjectTag) == native_context_address_;
}

void ContextFilter::OnMoveEvent(Address from_address, Address to_address) {
  if (native_context_address() != from_address) return;

  set_native_context_address(to_address);
}

using v8::tracing::TracedValue;

std::atomic<uint32_t> CpuProfile::last_id_;

CpuProfile::CpuProfile(CpuProfiler* profiler, const char* title,
                       CpuProfilingOptions options)
    : title_(title),
      options_(options),
      start_time_(base::TimeTicks::HighResolutionNow()),
      top_down_(profiler->isolate()),
      profiler_(profiler),
      streaming_next_sample_(0),
      id_(++last_id_) {
  auto value = TracedValue::Create();
  value->SetDouble("startTime",
                   (start_time_ - base::TimeTicks()).InMicroseconds());
  TRACE_EVENT_SAMPLE_WITH_ID1(TRACE_DISABLED_BY_DEFAULT("v8.cpu_profiler"),
                              "Profile", id_, "data", std::move(value));

  if (options_.has_filter_context()) {
    DisallowHeapAllocation no_gc;
    i::Address raw_filter_context =
        reinterpret_cast<i::Address>(options_.raw_filter_context());
    context_filter_ = std::make_unique<ContextFilter>(raw_filter_context);
  }
}

bool CpuProfile::CheckSubsample(base::TimeDelta source_sampling_interval) {
  DCHECK_GE(source_sampling_interval, base::TimeDelta());

  // If the sampling source's sampling interval is 0, record as many samples
  // are possible irrespective of the profile's sampling interval. Manually
  // taken samples (via CollectSample) fall into this case as well.
  if (source_sampling_interval.IsZero()) return true;

  next_sample_delta_ -= source_sampling_interval;
  if (next_sample_delta_ <= base::TimeDelta()) {
    next_sample_delta_ =
        base::TimeDelta::FromMicroseconds(options_.sampling_interval_us());
    return true;
  }
  return false;
}

void CpuProfile::AddPath(base::TimeTicks timestamp,
                         const ProfileStackTrace& path, int src_line,
                         bool update_stats, base::TimeDelta sampling_interval) {
  if (!CheckSubsample(sampling_interval)) return;

  ProfileNode* top_frame_node = top_down_.AddPathFromEnd(
      path, src_line, update_stats, options_.mode(), context_filter_.get());

  bool should_record_sample =
      !timestamp.IsNull() && timestamp >= start_time_ &&
      (options_.max_samples() == CpuProfilingOptions::kNoSampleLimit ||
       samples_.size() < options_.max_samples());

  if (should_record_sample)
    samples_.push_back({top_frame_node, timestamp, src_line});

  const int kSamplesFlushCount = 100;
  const int kNodesFlushCount = 10;
  if (samples_.size() - streaming_next_sample_ >= kSamplesFlushCount ||
      top_down_.pending_nodes_count() >= kNodesFlushCount) {
    StreamPendingTraceEvents();
  }
}

namespace {

void BuildNodeValue(const ProfileNode* node, TracedValue* value) {
  const CodeEntry* entry = node->entry();
  value->BeginDictionary("callFrame");
  value->SetString("functionName", entry->name());
  if (*entry->resource_name()) {
    value->SetString("url", entry->resource_name());
  }
  value->SetInteger("scriptId", entry->script_id());
  if (entry->line_number()) {
    value->SetInteger("lineNumber", entry->line_number() - 1);
  }
  if (entry->column_number()) {
    value->SetInteger("columnNumber", entry->column_number() - 1);
  }
  value->EndDictionary();
  value->SetInteger("id", node->id());
  if (node->parent()) {
    value->SetInteger("parent", node->parent()->id());
  }
  const char* deopt_reason = entry->bailout_reason();
  if (deopt_reason && deopt_reason[0] && strcmp(deopt_reason, "no reason")) {
    value->SetString("deoptReason", deopt_reason);
  }
}

}  // namespace

void CpuProfile::StreamPendingTraceEvents() {
  std::vector<const ProfileNode*> pending_nodes = top_down_.TakePendingNodes();
  if (pending_nodes.empty() && samples_.empty()) return;
  auto value = TracedValue::Create();

  if (!pending_nodes.empty() || streaming_next_sample_ != samples_.size()) {
    value->BeginDictionary("cpuProfile");
    if (!pending_nodes.empty()) {
      value->BeginArray("nodes");
      for (auto node : pending_nodes) {
        value->BeginDictionary();
        BuildNodeValue(node, value.get());
        value->EndDictionary();
      }
      value->EndArray();
    }
    if (streaming_next_sample_ != samples_.size()) {
      value->BeginArray("samples");
      for (size_t i = streaming_next_sample_; i < samples_.size(); ++i) {
        value->AppendInteger(samples_[i].node->id());
      }
      value->EndArray();
    }
    value->EndDictionary();
  }
  if (streaming_next_sample_ != samples_.size()) {
    value->BeginArray("timeDeltas");
    base::TimeTicks lastTimestamp =
        streaming_next_sample_ ? samples_[streaming_next_sample_ - 1].timestamp
                               : start_time();
    for (size_t i = streaming_next_sample_; i < samples_.size(); ++i) {
      value->AppendInteger(static_cast<int>(
          (samples_[i].timestamp - lastTimestamp).InMicroseconds()));
      lastTimestamp = samples_[i].timestamp;
    }
    value->EndArray();
    bool has_non_zero_lines =
        std::any_of(samples_.begin() + streaming_next_sample_, samples_.end(),
                    [](const SampleInfo& sample) { return sample.line != 0; });
    if (has_non_zero_lines) {
      value->BeginArray("lines");
      for (size_t i = streaming_next_sample_; i < samples_.size(); ++i) {
        value->AppendInteger(samples_[i].line);
      }
      value->EndArray();
    }
    streaming_next_sample_ = samples_.size();
  }

  TRACE_EVENT_SAMPLE_WITH_ID1(TRACE_DISABLED_BY_DEFAULT("v8.cpu_profiler"),
                              "ProfileChunk", id_, "data", std::move(value));
}

void CpuProfile::FinishProfile() {
  end_time_ = base::TimeTicks::HighResolutionNow();
  // Stop tracking context movements after profiling stops.
  context_filter_ = nullptr;
  StreamPendingTraceEvents();
  auto value = TracedValue::Create();
  value->SetDouble("endTime", (end_time_ - base::TimeTicks()).InMicroseconds());
  TRACE_EVENT_SAMPLE_WITH_ID1(TRACE_DISABLED_BY_DEFAULT("v8.cpu_profiler"),
                              "ProfileChunk", id_, "data", std::move(value));
}

void CpuProfile::Print() {
  base::OS::Print("[Top down]:\n");
  top_down_.Print();
}

CodeMap::CodeMap() = default;

CodeMap::~CodeMap() {
  // First clean the free list as it's otherwise impossible to tell
  // the slot type.
  unsigned free_slot = free_list_head_;
  while (free_slot != kNoFreeSlot) {
    unsigned next_slot = code_entries_[free_slot].next_free_slot;
    code_entries_[free_slot].entry = nullptr;
    free_slot = next_slot;
  }
  for (auto slot : code_entries_) delete slot.entry;
}

void CodeMap::AddCode(Address addr, CodeEntry* entry, unsigned size) {
  ClearCodesInRange(addr, addr + size);
  unsigned index = AddCodeEntry(addr, entry);
  code_map_.emplace(addr, CodeEntryMapInfo{index, size});
  DCHECK(entry->instruction_start() == kNullAddress ||
         addr == entry->instruction_start());
}

void CodeMap::ClearCodesInRange(Address start, Address end) {
  auto left = code_map_.upper_bound(start);
  if (left != code_map_.begin()) {
    --left;
    if (left->first + left->second.size <= start) ++left;
  }
  auto right = left;
  for (; right != code_map_.end() && right->first < end; ++right) {
    if (!entry(right->second.index)->used()) {
      DeleteCodeEntry(right->second.index);
    }
  }
  code_map_.erase(left, right);
}

CodeEntry* CodeMap::FindEntry(Address addr) {
  auto it = code_map_.upper_bound(addr);
  if (it == code_map_.begin()) return nullptr;
  --it;
  Address start_address = it->first;
  Address end_address = start_address + it->second.size;
  CodeEntry* ret = addr < end_address ? entry(it->second.index) : nullptr;
  if (ret && ret->instruction_start() != kNullAddress) {
    DCHECK_EQ(start_address, ret->instruction_start());
    DCHECK(addr >= start_address && addr < end_address);
  }
  return ret;
}

void CodeMap::MoveCode(Address from, Address to) {
  if (from == to) return;
  auto it = code_map_.find(from);
  if (it == code_map_.end()) return;
  CodeEntryMapInfo info = it->second;
  code_map_.erase(it);
  DCHECK(from + info.size <= to || to + info.size <= from);
  ClearCodesInRange(to, to + info.size);
  code_map_.emplace(to, info);

  CodeEntry* entry = code_entries_[info.index].entry;
  entry->set_instruction_start(to);
}

unsigned CodeMap::AddCodeEntry(Address start, CodeEntry* entry) {
  if (free_list_head_ == kNoFreeSlot) {
    code_entries_.push_back(CodeEntrySlotInfo{entry});
    return static_cast<unsigned>(code_entries_.size()) - 1;
  }
  unsigned index = free_list_head_;
  free_list_head_ = code_entries_[index].next_free_slot;
  code_entries_[index].entry = entry;
  return index;
}

void CodeMap::DeleteCodeEntry(unsigned index) {
  delete code_entries_[index].entry;
  code_entries_[index].next_free_slot = free_list_head_;
  free_list_head_ = index;
}

void CodeMap::Print() {
  for (const auto& pair : code_map_) {
    base::OS::Print("%p %5d %s\n", reinterpret_cast<void*>(pair.first),
                    pair.second.size, entry(pair.second.index)->name());
  }
}

CpuProfilesCollection::CpuProfilesCollection(Isolate* isolate)
    : profiler_(nullptr), current_profiles_semaphore_(1) {}

bool CpuProfilesCollection::StartProfiling(const char* title,
                                           CpuProfilingOptions options) {
  current_profiles_semaphore_.Wait();
  if (static_cast<int>(current_profiles_.size()) >= kMaxSimultaneousProfiles) {
    current_profiles_semaphore_.Signal();
    return false;
  }
  for (const std::unique_ptr<CpuProfile>& profile : current_profiles_) {
    if (strcmp(profile->title(), title) == 0) {
      // Ignore attempts to start profile with the same title...
      current_profiles_semaphore_.Signal();
      // ... though return true to force it collect a sample.
      return true;
    }
  }
  current_profiles_.emplace_back(new CpuProfile(profiler_, title, options));
  current_profiles_semaphore_.Signal();
  return true;
}

CpuProfile* CpuProfilesCollection::StopProfiling(const char* title) {
  const bool empty_title = (title[0] == '\0');
  CpuProfile* profile = nullptr;
  current_profiles_semaphore_.Wait();

  auto it = std::find_if(current_profiles_.rbegin(), current_profiles_.rend(),
                         [&](const std::unique_ptr<CpuProfile>& p) {
                           return empty_title || strcmp(p->title(), title) == 0;
                         });

  if (it != current_profiles_.rend()) {
    (*it)->FinishProfile();
    profile = it->get();
    finished_profiles_.push_back(std::move(*it));
    // Convert reverse iterator to matching forward iterator.
    current_profiles_.erase(--(it.base()));
  }

  current_profiles_semaphore_.Signal();
  return profile;
}


bool CpuProfilesCollection::IsLastProfile(const char* title) {
  // Called from VM thread, and only it can mutate the list,
  // so no locking is needed here.
  if (current_profiles_.size() != 1) return false;
  return title[0] == '\0' || strcmp(current_profiles_[0]->title(), title) == 0;
}


void CpuProfilesCollection::RemoveProfile(CpuProfile* profile) {
  // Called from VM thread for a completed profile.
  auto pos =
      std::find_if(finished_profiles_.begin(), finished_profiles_.end(),
                   [&](const std::unique_ptr<CpuProfile>& finished_profile) {
                     return finished_profile.get() == profile;
                   });
  DCHECK(pos != finished_profiles_.end());
  finished_profiles_.erase(pos);
}

namespace {

int64_t GreatestCommonDivisor(int64_t a, int64_t b) {
  return b ? GreatestCommonDivisor(b, a % b) : a;
}

}  // namespace

base::TimeDelta CpuProfilesCollection::GetCommonSamplingInterval() const {
  DCHECK(profiler_);

  int64_t base_sampling_interval_us =
      profiler_->sampling_interval().InMicroseconds();
  if (base_sampling_interval_us == 0) return base::TimeDelta();

  int64_t interval_us = 0;
  for (const auto& profile : current_profiles_) {
    // Snap the profile's requested sampling interval to the next multiple of
    // the base sampling interval.
    int64_t profile_interval_us =
        std::max<int64_t>(
            (profile->sampling_interval_us() + base_sampling_interval_us - 1) /
                base_sampling_interval_us,
            1) *
        base_sampling_interval_us;
    interval_us = GreatestCommonDivisor(interval_us, profile_interval_us);
  }
  return base::TimeDelta::FromMicroseconds(interval_us);
}

void CpuProfilesCollection::AddPathToCurrentProfiles(
    base::TimeTicks timestamp, const ProfileStackTrace& path, int src_line,
    bool update_stats, base::TimeDelta sampling_interval) {
  // As starting / stopping profiles is rare relatively to this
  // method, we don't bother minimizing the duration of lock holding,
  // e.g. copying contents of the list to a local vector.
  current_profiles_semaphore_.Wait();
  for (const std::unique_ptr<CpuProfile>& profile : current_profiles_) {
    profile->AddPath(timestamp, path, src_line, update_stats,
                     sampling_interval);
  }
  current_profiles_semaphore_.Signal();
}

void CpuProfilesCollection::UpdateNativeContextAddressForCurrentProfiles(
    Address from, Address to) {
  current_profiles_semaphore_.Wait();
  for (const std::unique_ptr<CpuProfile>& profile : current_profiles_) {
    if (auto* context_filter = profile->context_filter()) {
      context_filter->OnMoveEvent(from, to);
    }
  }
  current_profiles_semaphore_.Signal();
}

ProfileGenerator::ProfileGenerator(CpuProfilesCollection* profiles,
                                   CodeMap* code_map)
    : profiles_(profiles), code_map_(code_map) {}

void ProfileGenerator::RecordTickSample(const TickSample& sample) {
  ProfileStackTrace stack_trace;
  // Conservatively reserve space for stack frames + pc + function + vm-state.
  // There could in fact be more of them because of inlined entries.
  stack_trace.reserve(sample.frames_count + 3);

  // The ProfileNode knows nothing about all versions of generated code for
  // the same JS function. The line number information associated with
  // the latest version of generated code is used to find a source line number
  // for a JS function. Then, the detected source line is passed to
  // ProfileNode to increase the tick count for this source line.
  const int no_line_info = v8::CpuProfileNode::kNoLineNumberInfo;
  int src_line = no_line_info;
  bool src_line_not_found = true;

  if (sample.pc != nullptr) {
    if (sample.has_external_callback && sample.state == EXTERNAL) {
      // Don't use PC when in external callback code, as it can point
      // inside a callback's code, and we will erroneously report
      // that a callback calls itself.
      stack_trace.push_back({{FindEntry(reinterpret_cast<Address>(
                                  sample.external_callback_entry)),
                              no_line_info},
                             reinterpret_cast<Address>(sample.top_context),
                             true});
    } else {
      Address attributed_pc = reinterpret_cast<Address>(sample.pc);
      CodeEntry* pc_entry = FindEntry(attributed_pc);
      // If there is no pc_entry, we're likely in native code. Find out if the
      // top of the stack (the return address) was pointing inside a JS
      // function, meaning that we have encountered a frameless invocation.
      if (!pc_entry && !sample.has_external_callback) {
        attributed_pc = reinterpret_cast<Address>(sample.tos);
        pc_entry = FindEntry(attributed_pc);
      }
      // If pc is in the function code before it set up stack frame or after the
      // frame was destroyed, SafeStackFrameIterator incorrectly thinks that
      // ebp contains the return address of the current function and skips the
      // caller's frame. Check for this case and just skip such samples.
      if (pc_entry) {
        int pc_offset =
            static_cast<int>(attributed_pc - pc_entry->instruction_start());
        // TODO(petermarshall): pc_offset can still be negative in some cases.
        src_line = pc_entry->GetSourceLine(pc_offset);
        if (src_line == v8::CpuProfileNode::kNoLineNumberInfo) {
          src_line = pc_entry->line_number();
        }
        src_line_not_found = false;
        stack_trace.push_back({{pc_entry, src_line},
                               reinterpret_cast<Address>(sample.top_context),
                               true});

        if (pc_entry->builtin_id() == Builtins::kFunctionPrototypeApply ||
            pc_entry->builtin_id() == Builtins::kFunctionPrototypeCall) {
          // When current function is either the Function.prototype.apply or the
          // Function.prototype.call builtin the top frame is either frame of
          // the calling JS function or internal frame.
          // In the latter case we know the caller for sure but in the
          // former case we don't so we simply replace the frame with
          // 'unresolved' entry.
          if (!sample.has_external_callback) {
            stack_trace.push_back(
                {{CodeEntry::unresolved_entry(), no_line_info},
                 kNullAddress,
                 true});
          }
        }
      }
    }

    for (unsigned i = 0; i < sample.frames_count; ++i) {
      Address stack_pos = reinterpret_cast<Address>(sample.stack[i]);
      Address native_context = reinterpret_cast<Address>(sample.contexts[i]);
      CodeEntry* entry = FindEntry(stack_pos);
      int line_number = no_line_info;
      if (entry) {
        // Find out if the entry has an inlining stack associated.
        int pc_offset =
            static_cast<int>(stack_pos - entry->instruction_start());
        // TODO(petermarshall): pc_offset can still be negative in some cases.
        const std::vector<CodeEntryAndLineNumber>* inline_stack =
            entry->GetInlineStack(pc_offset);
        if (inline_stack) {
          int most_inlined_frame_line_number = entry->GetSourceLine(pc_offset);
          for (auto entry : *inline_stack) {
            // Set the native context of inlined frames to be equal to that of
            // their parent. This is safe, as functions cannot inline themselves
            // into a parent from another native context.
            stack_trace.push_back({entry, native_context, true});
          }

          // This is a bit of a messy hack. The line number for the most-inlined
          // frame (the function at the end of the chain of function calls) has
          // the wrong line number in inline_stack. The actual line number in
          // this function is stored in the SourcePositionTable in entry. We fix
          // up the line number for the most-inlined frame here.
          // TODO(petermarshall): Remove this and use a tree with a node per
          // inlining_id.
          DCHECK(!inline_stack->empty());
          size_t index = stack_trace.size() - inline_stack->size();
          stack_trace[index].entry.line_number = most_inlined_frame_line_number;
        }
        // Skip unresolved frames (e.g. internal frame) and get source line of
        // the first JS caller.
        if (src_line_not_found) {
          src_line = entry->GetSourceLine(pc_offset);
          if (src_line == v8::CpuProfileNode::kNoLineNumberInfo) {
            src_line = entry->line_number();
          }
          src_line_not_found = false;
        }
        line_number = entry->GetSourceLine(pc_offset);

        // The inline stack contains the top-level function i.e. the same
        // function as entry. We don't want to add it twice. The one from the
        // inline stack has the correct line number for this particular inlining
        // so we use it instead of pushing entry to stack_trace.
        if (inline_stack) continue;
      }
      stack_trace.push_back({{entry, line_number}, native_context, true});
    }
  }

  if (FLAG_prof_browser_mode) {
    bool no_symbolized_entries = true;
    for (auto e : stack_trace) {
      if (e.entry.code_entry != nullptr) {
        no_symbolized_entries = false;
        break;
      }
    }
    // If no frames were symbolized, put the VM state entry in.
    if (no_symbolized_entries) {
      stack_trace.push_back(
          {{EntryForVMState(sample.state), no_line_info}, kNullAddress, false});
    }
  }

  profiles_->AddPathToCurrentProfiles(sample.timestamp, stack_trace, src_line,
                                      sample.update_stats,
                                      sample.sampling_interval);
}

void ProfileGenerator::UpdateNativeContextAddress(Address from, Address to) {
  profiles_->UpdateNativeContextAddressForCurrentProfiles(from, to);
}

CodeEntry* ProfileGenerator::EntryForVMState(StateTag tag) {
  switch (tag) {
    case GC:
      return CodeEntry::gc_entry();
    case JS:
    case PARSER:
    case COMPILER:
    case BYTECODE_COMPILER:
    // DOM events handlers are reported as OTHER / EXTERNAL entries.
    // To avoid confusing people, let's put all these entries into
    // one bucket.
    case OTHER:
    case EXTERNAL:
      return CodeEntry::program_entry();
    case IDLE:
      return CodeEntry::idle_entry();
  }
  UNREACHABLE();
}

}  // namespace internal
}  // namespace v8
