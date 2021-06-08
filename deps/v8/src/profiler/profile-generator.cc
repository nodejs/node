// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/profiler/profile-generator.h"

#include <algorithm>

#include "include/v8-profiler.h"
#include "src/base/lazy-instance.h"
#include "src/codegen/source-position.h"
#include "src/objects/shared-function-info-inl.h"
#include "src/profiler/cpu-profiler.h"
#include "src/profiler/profile-generator-inl.h"
#include "src/profiler/profiler-stats.h"
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

const char* const CodeEntry::kEmptyResourceName = "";
const char* const CodeEntry::kEmptyBailoutReason = "";
const char* const CodeEntry::kNoDeoptReason = "";

const char* const CodeEntry::kProgramEntryName = "(program)";
const char* const CodeEntry::kIdleEntryName = "(idle)";
const char* const CodeEntry::kGarbageCollectorEntryName = "(garbage collector)";
const char* const CodeEntry::kUnresolvedFunctionName = "(unresolved function)";
const char* const CodeEntry::kRootEntryName = "(root)";

// static
CodeEntry* CodeEntry::program_entry() {
  static base::LeakyObject<CodeEntry> kProgramEntry(
      CodeEventListener::FUNCTION_TAG, CodeEntry::kProgramEntryName,
      CodeEntry::kEmptyResourceName, v8::CpuProfileNode::kNoLineNumberInfo,
      v8::CpuProfileNode::kNoColumnNumberInfo, nullptr, false,
      CodeEntry::CodeType::OTHER);
  return kProgramEntry.get();
}

// static
CodeEntry* CodeEntry::idle_entry() {
  static base::LeakyObject<CodeEntry> kIdleEntry(
      CodeEventListener::FUNCTION_TAG, CodeEntry::kIdleEntryName,
      CodeEntry::kEmptyResourceName, v8::CpuProfileNode::kNoLineNumberInfo,
      v8::CpuProfileNode::kNoColumnNumberInfo, nullptr, false,
      CodeEntry::CodeType::OTHER);
  return kIdleEntry.get();
}

// static
CodeEntry* CodeEntry::gc_entry() {
  static base::LeakyObject<CodeEntry> kGcEntry(
      CodeEventListener::BUILTIN_TAG, CodeEntry::kGarbageCollectorEntryName,
      CodeEntry::kEmptyResourceName, v8::CpuProfileNode::kNoLineNumberInfo,
      v8::CpuProfileNode::kNoColumnNumberInfo, nullptr, false,
      CodeEntry::CodeType::OTHER);
  return kGcEntry.get();
}

// static
CodeEntry* CodeEntry::unresolved_entry() {
  static base::LeakyObject<CodeEntry> kUnresolvedEntry(
      CodeEventListener::FUNCTION_TAG, CodeEntry::kUnresolvedFunctionName,
      CodeEntry::kEmptyResourceName, v8::CpuProfileNode::kNoLineNumberInfo,
      v8::CpuProfileNode::kNoColumnNumberInfo, nullptr, false,
      CodeEntry::CodeType::OTHER);
  return kUnresolvedEntry.get();
}

// static
CodeEntry* CodeEntry::root_entry() {
  static base::LeakyObject<CodeEntry> kRootEntry(
      CodeEventListener::FUNCTION_TAG, CodeEntry::kRootEntryName,
      CodeEntry::kEmptyResourceName, v8::CpuProfileNode::kNoLineNumberInfo,
      v8::CpuProfileNode::kNoColumnNumberInfo, nullptr, false,
      CodeEntry::CodeType::OTHER);
  return kRootEntry.get();
}

uint32_t CodeEntry::GetHash() const {
  uint32_t hash = 0;
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

void CodeEntry::ReleaseStrings(StringsStorage& strings) {
  if (name_) {
    strings.Release(name_);
    name_ = nullptr;
  }
  if (resource_name_) {
    strings.Release(resource_name_);
    resource_name_ = nullptr;
  }

  if (rare_data_) {
    // All inline entries are exclusively owned by the CodeEntry. They'll be
    // deallocated when the CodeEntry is deallocated.
    for (auto& entry : rare_data_->inline_entries_) {
      entry->ReleaseStrings(strings);
    }
  }
}

void CodeEntry::print() const {
  base::OS::Print("CodeEntry: at %p\n", this);

  base::OS::Print(" - name: %s\n", name_);
  base::OS::Print(" - resource_name: %s\n", resource_name_);
  base::OS::Print(" - line_number: %d\n", line_number_);
  base::OS::Print(" - column_number: %d\n", column_number_);
  base::OS::Print(" - script_id: %d\n", script_id_);
  base::OS::Print(" - position: %d\n", position_);

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

CpuProfileNode::SourceType ProfileNode::source_type() const {
  // Handle metadata and VM state code entry types.
  if (entry_ == CodeEntry::program_entry() ||
      entry_ == CodeEntry::idle_entry() || entry_ == CodeEntry::gc_entry() ||
      entry_ == CodeEntry::root_entry()) {
    return CpuProfileNode::kInternal;
  }
  if (entry_ == CodeEntry::unresolved_entry())
    return CpuProfileNode::kUnresolved;

  // Otherwise, resolve based on logger tag.
  switch (entry_->tag()) {
    case CodeEventListener::EVAL_TAG:
    case CodeEventListener::SCRIPT_TAG:
    case CodeEventListener::LAZY_COMPILE_TAG:
    case CodeEventListener::FUNCTION_TAG:
      return CpuProfileNode::kScript;
    case CodeEventListener::BUILTIN_TAG:
    case CodeEventListener::HANDLER_TAG:
    case CodeEventListener::BYTECODE_HANDLER_TAG:
    case CodeEventListener::NATIVE_FUNCTION_TAG:
    case CodeEventListener::NATIVE_SCRIPT_TAG:
    case CodeEventListener::NATIVE_LAZY_COMPILE_TAG:
      return CpuProfileNode::kBuiltin;
    case CodeEventListener::CALLBACK_TAG:
      return CpuProfileNode::kCallback;
    case CodeEventListener::REG_EXP_TAG:
    case CodeEventListener::STUB_TAG:
    case CodeEventListener::CODE_CREATION_EVENT:
    case CodeEventListener::CODE_DISABLE_OPT_EVENT:
    case CodeEventListener::CODE_MOVE_EVENT:
    case CodeEventListener::CODE_DELETE_EVENT:
    case CodeEventListener::CODE_MOVING_GC:
    case CodeEventListener::SHARED_FUNC_MOVE_EVENT:
    case CodeEventListener::SNAPSHOT_CODE_NAME_EVENT:
    case CodeEventListener::TICK_EVENT:
    case CodeEventListener::NUMBER_OF_LOG_EVENTS:
      return CpuProfileNode::kInternal;
  }
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

void ProfileNode::Print(int indent) const {
  int line_number = line_number_ != 0 ? line_number_ : entry_->line_number();
  base::OS::Print("%5u %*s %s:%d %d %d #%d", self_ticks_, indent, "",
                  entry_->name(), line_number, source_type(),
                  entry_->script_id(), id());
  if (entry_->resource_name()[0] != '\0')
    base::OS::Print(" %s:%d", entry_->resource_name(), entry_->line_number());
  base::OS::Print("\n");
  for (const CpuProfileDeoptInfo& info : deopt_infos_) {
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
      isolate_(isolate) {}

ProfileTree::~ProfileTree() {
  DeleteNodesCallback cb;
  TraverseDepthFirst(&cb);
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
                                         ProfilingMode mode) {
  ProfileNode* node = root_;
  CodeEntry* last_entry = nullptr;
  int parent_line_number = v8::CpuProfileNode::kNoLineNumberInfo;
  for (auto it = path.rbegin(); it != path.rend(); ++it) {
    if (it->code_entry == nullptr) continue;
    last_entry = it->code_entry;
    node = node->FindOrAddChild(it->code_entry, parent_line_number);
    parent_line_number = mode == ProfilingMode::kCallerLineNumbers
                             ? it->line_number
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

using v8::tracing::TracedValue;

std::atomic<uint32_t> CpuProfile::last_id_;

CpuProfile::CpuProfile(CpuProfiler* profiler, const char* title,
                       CpuProfilingOptions options,
                       std::unique_ptr<DiscardedSamplesDelegate> delegate)
    : title_(title),
      options_(options),
      delegate_(std::move(delegate)),
      start_time_(base::TimeTicks::HighResolutionNow()),
      top_down_(profiler->isolate()),
      profiler_(profiler),
      streaming_next_sample_(0),
      id_(++last_id_) {
  // The startTime timestamp is not converted to Perfetto's clock domain and
  // will get out of sync with other timestamps Perfetto knows about, including
  // the automatic trace event "ts" timestamp. startTime is included for
  // backward compatibility with the tracing protocol but the value of "ts"
  // should be used instead (it is recorded nearly immediately after).
  auto value = TracedValue::Create();
  value->SetDouble("startTime", start_time_.since_origin().InMicroseconds());
  TRACE_EVENT_SAMPLE_WITH_ID1(TRACE_DISABLED_BY_DEFAULT("v8.cpu_profiler"),
                              "Profile", id_, "data", std::move(value));
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

  ProfileNode* top_frame_node =
      top_down_.AddPathFromEnd(path, src_line, update_stats, options_.mode());

  bool should_record_sample =
      !timestamp.IsNull() && timestamp >= start_time_ &&
      (options_.max_samples() == CpuProfilingOptions::kNoSampleLimit ||
       samples_.size() < options_.max_samples());

  if (should_record_sample) {
    samples_.push_back({top_frame_node, timestamp, src_line});
  }

  if (!should_record_sample && delegate_ != nullptr) {
    const auto task_runner = V8::GetCurrentPlatform()->GetForegroundTaskRunner(
        reinterpret_cast<v8::Isolate*>(profiler_->isolate()));

    task_runner->PostTask(std::make_unique<CpuProfileMaxSamplesCallbackTask>(
        std::move(delegate_)));
    // std::move ensures that the delegate_ will be null on the next sample,
    // so we don't post a task multiple times.
  }

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
  value->SetString("codeType", entry->code_type_string());
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
    // timeDeltas are computed within CLOCK_MONOTONIC. However, trace event
    // "ts" timestamps are converted to CLOCK_BOOTTIME by Perfetto. To get
    // absolute timestamps in CLOCK_BOOTTIME from timeDeltas, add them to
    // the "ts" timestamp from the initial "Profile" trace event sent by
    // CpuProfile::CpuProfile().
    //
    // Note that if the system is suspended and resumed while samples_ is
    // captured, timeDeltas derived after resume will not be convertible to
    // correct CLOCK_BOOTTIME time values (for instance, producing
    // CLOCK_BOOTTIME time values in the middle of the suspended period).
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
  StreamPendingTraceEvents();
  auto value = TracedValue::Create();
  // The endTime timestamp is not converted to Perfetto's clock domain and will
  // get out of sync with other timestamps Perfetto knows about, including the
  // automatic trace event "ts" timestamp. endTime is included for backward
  // compatibility with the tracing protocol: its presence in "data" is used by
  // devtools to identify the last ProfileChunk but the value of "ts" should be
  // used instead (it is recorded nearly immediately after).
  value->SetDouble("endTime", end_time_.since_origin().InMicroseconds());
  TRACE_EVENT_SAMPLE_WITH_ID1(TRACE_DISABLED_BY_DEFAULT("v8.cpu_profiler"),
                              "ProfileChunk", id_, "data", std::move(value));
}

void CpuProfile::Print() const {
  base::OS::Print("[Top down]:\n");
  top_down_.Print();
  ProfilerStats::Instance()->Print();
  ProfilerStats::Instance()->Clear();
}

CodeMap::CodeMap(StringsStorage& function_and_resource_names)
    : function_and_resource_names_(function_and_resource_names) {}

CodeMap::~CodeMap() { Clear(); }

void CodeMap::Clear() {
  for (auto& slot : code_map_) {
    if (CodeEntry* entry = slot.second.entry) {
      entry->ReleaseStrings(function_and_resource_names_);
      delete entry;
    } else {
      // We expect all entries in the code mapping to contain a CodeEntry.
      UNREACHABLE();
    }
  }

  // Free all CodeEntry objects that are no longer in the map, but in a profile.
  // TODO(acomminos): Remove this deque after we refcount CodeEntry objects.
  for (CodeEntry* entry : used_entries_) {
    DCHECK(entry->used());
    DeleteCodeEntry(entry);
  }

  code_map_.clear();
  used_entries_.clear();
}

void CodeMap::AddCode(Address addr, CodeEntry* entry, unsigned size) {
  code_map_.emplace(addr, CodeEntryMapInfo{entry, size});
  entry->set_instruction_start(addr);
}

bool CodeMap::RemoveCode(CodeEntry* entry) {
  auto range = code_map_.equal_range(entry->instruction_start());
  for (auto i = range.first; i != range.second; ++i) {
    if (i->second.entry == entry) {
      if (!entry->used()) {
        DeleteCodeEntry(entry);
      } else {
        used_entries_.push_back(entry);
      }
      code_map_.erase(i);
      return true;
    }
  }
  return false;
}

void CodeMap::ClearCodesInRange(Address start, Address end) {
  auto left = code_map_.upper_bound(start);
  if (left != code_map_.begin()) {
    --left;
    if (left->first + left->second.size <= start) ++left;
  }
  auto right = left;
  for (; right != code_map_.end() && right->first < end; ++right) {
    if (!right->second.entry->used()) {
      DeleteCodeEntry(right->second.entry);
    } else {
      used_entries_.push_back(right->second.entry);
    }
  }
  code_map_.erase(left, right);
}

CodeEntry* CodeMap::FindEntry(Address addr, Address* out_instruction_start) {
  // Note that an address may correspond to multiple CodeEntry objects. An
  // arbitrary selection is made (as per multimap spec) in the event of a
  // collision.
  auto it = code_map_.upper_bound(addr);
  if (it == code_map_.begin()) return nullptr;
  --it;
  Address start_address = it->first;
  Address end_address = start_address + it->second.size;
  CodeEntry* ret = addr < end_address ? it->second.entry : nullptr;
  DCHECK(!ret || (addr >= start_address && addr < end_address));
  if (ret && out_instruction_start) *out_instruction_start = start_address;
  return ret;
}

void CodeMap::MoveCode(Address from, Address to) {
  if (from == to) return;

  auto range = code_map_.equal_range(from);
  // Instead of iterating until |range.second|, iterate the number of elements.
  // This is because the |range.second| may no longer be the element past the
  // end of the equal elements range after insertions.
  size_t distance = std::distance(range.first, range.second);
  auto it = range.first;
  while (distance--) {
    CodeEntryMapInfo& info = it->second;
    DCHECK(info.entry);
    DCHECK_EQ(info.entry->instruction_start(), from);
    info.entry->set_instruction_start(to);

    DCHECK(from + info.size <= to || to + info.size <= from);
    code_map_.emplace(to, info);
    it++;
  }

  code_map_.erase(range.first, it);
}

void CodeMap::DeleteCodeEntry(CodeEntry* entry) {
  entry->ReleaseStrings(function_and_resource_names_);
  delete entry;
}

void CodeMap::Print() {
  for (const auto& pair : code_map_) {
    base::OS::Print("%p %5d %s\n", reinterpret_cast<void*>(pair.first),
                    pair.second.size, pair.second.entry->name());
  }
}

CpuProfilesCollection::CpuProfilesCollection(Isolate* isolate)
    : profiler_(nullptr), current_profiles_semaphore_(1) {}

CpuProfilingStatus CpuProfilesCollection::StartProfiling(
    const char* title, CpuProfilingOptions options,
    std::unique_ptr<DiscardedSamplesDelegate> delegate) {
  current_profiles_semaphore_.Wait();

  if (static_cast<int>(current_profiles_.size()) >= kMaxSimultaneousProfiles) {
    current_profiles_semaphore_.Signal();

    return CpuProfilingStatus::kErrorTooManyProfilers;
  }
  for (const std::unique_ptr<CpuProfile>& profile : current_profiles_) {
    if (strcmp(profile->title(), title) == 0) {
      // Ignore attempts to start profile with the same title...
      current_profiles_semaphore_.Signal();
      // ... though return kAlreadyStarted to force it collect a sample.
      return CpuProfilingStatus::kAlreadyStarted;
    }
  }

  current_profiles_.emplace_back(
      new CpuProfile(profiler_, title, options, std::move(delegate)));
  current_profiles_semaphore_.Signal();
  return CpuProfilingStatus::kStarted;
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

}  // namespace internal
}  // namespace v8
