// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/profiler/profile-generator.h"

#include "src/base/adapters.h"
#include "src/debug/debug.h"
#include "src/deoptimizer.h"
#include "src/global-handles.h"
#include "src/profiler/cpu-profiler.h"
#include "src/profiler/profile-generator-inl.h"
#include "src/tracing/trace-event.h"
#include "src/tracing/traced-value.h"
#include "src/unicode.h"

namespace v8 {
namespace internal {


JITLineInfoTable::JITLineInfoTable() {}


JITLineInfoTable::~JITLineInfoTable() {}


void JITLineInfoTable::SetPosition(int pc_offset, int line) {
  DCHECK(pc_offset >= 0);
  DCHECK(line > 0);  // The 1-based number of the source line.
  if (GetSourceLineNumber(pc_offset) != line) {
    pc_offset_map_.insert(std::make_pair(pc_offset, line));
  }
}


int JITLineInfoTable::GetSourceLineNumber(int pc_offset) const {
  PcOffsetMap::const_iterator it = pc_offset_map_.lower_bound(pc_offset);
  if (it == pc_offset_map_.end()) {
    if (pc_offset_map_.empty()) return v8::CpuProfileNode::kNoLineNumberInfo;
    return (--pc_offset_map_.end())->second;
  }
  return it->second;
}


const char* const CodeEntry::kEmptyNamePrefix = "";
const char* const CodeEntry::kEmptyResourceName = "";
const char* const CodeEntry::kEmptyBailoutReason = "";
const char* const CodeEntry::kNoDeoptReason = "";

const char* const CodeEntry::kProgramEntryName = "(program)";
const char* const CodeEntry::kIdleEntryName = "(idle)";
const char* const CodeEntry::kGarbageCollectorEntryName = "(garbage collector)";
const char* const CodeEntry::kUnresolvedFunctionName = "(unresolved function)";

base::LazyDynamicInstance<CodeEntry, CodeEntry::ProgramEntryCreateTrait>::type
    CodeEntry::kProgramEntry = LAZY_DYNAMIC_INSTANCE_INITIALIZER;

base::LazyDynamicInstance<CodeEntry, CodeEntry::IdleEntryCreateTrait>::type
    CodeEntry::kIdleEntry = LAZY_DYNAMIC_INSTANCE_INITIALIZER;

base::LazyDynamicInstance<CodeEntry, CodeEntry::GCEntryCreateTrait>::type
    CodeEntry::kGCEntry = LAZY_DYNAMIC_INSTANCE_INITIALIZER;

base::LazyDynamicInstance<CodeEntry,
                          CodeEntry::UnresolvedEntryCreateTrait>::type
    CodeEntry::kUnresolvedEntry = LAZY_DYNAMIC_INSTANCE_INITIALIZER;

CodeEntry* CodeEntry::ProgramEntryCreateTrait::Create() {
  return new CodeEntry(Logger::FUNCTION_TAG, CodeEntry::kProgramEntryName);
}

CodeEntry* CodeEntry::IdleEntryCreateTrait::Create() {
  return new CodeEntry(Logger::FUNCTION_TAG, CodeEntry::kIdleEntryName);
}

CodeEntry* CodeEntry::GCEntryCreateTrait::Create() {
  return new CodeEntry(Logger::BUILTIN_TAG,
                       CodeEntry::kGarbageCollectorEntryName);
}

CodeEntry* CodeEntry::UnresolvedEntryCreateTrait::Create() {
  return new CodeEntry(Logger::FUNCTION_TAG,
                       CodeEntry::kUnresolvedFunctionName);
}

CodeEntry::~CodeEntry() {
  delete line_info_;
  for (auto location : inline_locations_) {
    for (auto entry : location.second) {
      delete entry;
    }
  }
}


uint32_t CodeEntry::GetHash() const {
  uint32_t hash = ComputeIntegerHash(tag(), v8::internal::kZeroHashSeed);
  if (script_id_ != v8::UnboundScript::kNoScriptId) {
    hash ^= ComputeIntegerHash(static_cast<uint32_t>(script_id_),
                               v8::internal::kZeroHashSeed);
    hash ^= ComputeIntegerHash(static_cast<uint32_t>(position_),
                               v8::internal::kZeroHashSeed);
  } else {
    hash ^= ComputeIntegerHash(
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(name_prefix_)),
        v8::internal::kZeroHashSeed);
    hash ^= ComputeIntegerHash(
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(name_)),
        v8::internal::kZeroHashSeed);
    hash ^= ComputeIntegerHash(
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(resource_name_)),
        v8::internal::kZeroHashSeed);
    hash ^= ComputeIntegerHash(line_number_, v8::internal::kZeroHashSeed);
  }
  return hash;
}


bool CodeEntry::IsSameFunctionAs(CodeEntry* entry) const {
  if (this == entry) return true;
  if (script_id_ != v8::UnboundScript::kNoScriptId) {
    return script_id_ == entry->script_id_ && position_ == entry->position_;
  }
  return name_prefix_ == entry->name_prefix_ && name_ == entry->name_ &&
         resource_name_ == entry->resource_name_ &&
         line_number_ == entry->line_number_;
}


void CodeEntry::SetBuiltinId(Builtins::Name id) {
  bit_field_ = TagField::update(bit_field_, CodeEventListener::BUILTIN_TAG);
  bit_field_ = BuiltinIdField::update(bit_field_, id);
}


int CodeEntry::GetSourceLine(int pc_offset) const {
  if (line_info_ && !line_info_->empty()) {
    return line_info_->GetSourceLineNumber(pc_offset);
  }
  return v8::CpuProfileNode::kNoLineNumberInfo;
}

void CodeEntry::AddInlineStack(int pc_offset,
                               std::vector<CodeEntry*> inline_stack) {
  inline_locations_.insert(std::make_pair(pc_offset, std::move(inline_stack)));
}

const std::vector<CodeEntry*>* CodeEntry::GetInlineStack(int pc_offset) const {
  auto it = inline_locations_.find(pc_offset);
  return it != inline_locations_.end() ? &it->second : NULL;
}

void CodeEntry::AddDeoptInlinedFrames(
    int deopt_id, std::vector<CpuProfileDeoptFrame> inlined_frames) {
  deopt_inlined_frames_.insert(
      std::make_pair(deopt_id, std::move(inlined_frames)));
}

bool CodeEntry::HasDeoptInlinedFramesFor(int deopt_id) const {
  return deopt_inlined_frames_.find(deopt_id) != deopt_inlined_frames_.end();
}

void CodeEntry::FillFunctionInfo(SharedFunctionInfo* shared) {
  if (!shared->script()->IsScript()) return;
  Script* script = Script::cast(shared->script());
  set_script_id(script->id());
  set_position(shared->start_position());
  set_bailout_reason(GetBailoutReason(shared->disable_optimization_reason()));
}

CpuProfileDeoptInfo CodeEntry::GetDeoptInfo() {
  DCHECK(has_deopt_info());

  CpuProfileDeoptInfo info;
  info.deopt_reason = deopt_reason_;
  DCHECK_NE(kNoDeoptimizationId, deopt_id_);
  if (deopt_inlined_frames_.find(deopt_id_) == deopt_inlined_frames_.end()) {
    info.stack.push_back(CpuProfileDeoptFrame(
        {script_id_, static_cast<size_t>(std::max(0, position()))}));
  } else {
    info.stack = deopt_inlined_frames_[deopt_id_];
  }
  return info;
}


void ProfileNode::CollectDeoptInfo(CodeEntry* entry) {
  deopt_infos_.push_back(entry->GetDeoptInfo());
  entry->clear_deopt_info();
}


ProfileNode* ProfileNode::FindChild(CodeEntry* entry) {
  base::HashMap::Entry* map_entry =
      children_.Lookup(entry, CodeEntryHash(entry));
  return map_entry != NULL ?
      reinterpret_cast<ProfileNode*>(map_entry->value) : NULL;
}


ProfileNode* ProfileNode::FindOrAddChild(CodeEntry* entry) {
  base::HashMap::Entry* map_entry =
      children_.LookupOrInsert(entry, CodeEntryHash(entry));
  ProfileNode* node = reinterpret_cast<ProfileNode*>(map_entry->value);
  if (!node) {
    node = new ProfileNode(tree_, entry, this);
    map_entry->value = node;
    children_list_.Add(node);
  }
  return node;
}


void ProfileNode::IncrementLineTicks(int src_line) {
  if (src_line == v8::CpuProfileNode::kNoLineNumberInfo) return;
  // Increment a hit counter of a certain source line.
  // Add a new source line if not found.
  base::HashMap::Entry* e =
      line_ticks_.LookupOrInsert(reinterpret_cast<void*>(src_line), src_line);
  DCHECK(e);
  e->value = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(e->value) + 1);
}


bool ProfileNode::GetLineTicks(v8::CpuProfileNode::LineTick* entries,
                               unsigned int length) const {
  if (entries == NULL || length == 0) return false;

  unsigned line_count = line_ticks_.occupancy();

  if (line_count == 0) return true;
  if (length < line_count) return false;

  v8::CpuProfileNode::LineTick* entry = entries;

  for (base::HashMap::Entry *p = line_ticks_.Start(); p != NULL;
       p = line_ticks_.Next(p), entry++) {
    entry->line =
        static_cast<unsigned int>(reinterpret_cast<uintptr_t>(p->key));
    entry->hit_count =
        static_cast<unsigned int>(reinterpret_cast<uintptr_t>(p->value));
  }

  return true;
}


void ProfileNode::Print(int indent) {
  base::OS::Print("%5u %*s %s%s %d #%d", self_ticks_, indent, "",
                  entry_->name_prefix(), entry_->name(), entry_->script_id(),
                  id());
  if (entry_->resource_name()[0] != '\0')
    base::OS::Print(" %s:%d", entry_->resource_name(), entry_->line_number());
  base::OS::Print("\n");
  for (size_t i = 0; i < deopt_infos_.size(); ++i) {
    CpuProfileDeoptInfo& info = deopt_infos_[i];
    base::OS::Print("%*s;;; deopted at script_id: %d position: %" PRIuS
                    " with reason '%s'.\n",
                    indent + 10, "", info.stack[0].script_id,
                    info.stack[0].position, info.deopt_reason);
    for (size_t index = 1; index < info.stack.size(); ++index) {
      base::OS::Print("%*s;;;     Inline point: script_id %d position: %" PRIuS
                      ".\n",
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
  for (base::HashMap::Entry* p = children_.Start(); p != NULL;
       p = children_.Next(p)) {
    reinterpret_cast<ProfileNode*>(p->value)->Print(indent + 2);
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
    : root_entry_(CodeEventListener::FUNCTION_TAG, "(root)"),
      next_node_id_(1),
      root_(new ProfileNode(this, &root_entry_, nullptr)),
      isolate_(isolate),
      next_function_id_(1),
      function_ids_(ProfileNode::CodeEntriesMatch) {}

ProfileTree::~ProfileTree() {
  DeleteNodesCallback cb;
  TraverseDepthFirst(&cb);
}


unsigned ProfileTree::GetFunctionId(const ProfileNode* node) {
  CodeEntry* code_entry = node->entry();
  base::HashMap::Entry* entry =
      function_ids_.LookupOrInsert(code_entry, code_entry->GetHash());
  if (!entry->value) {
    entry->value = reinterpret_cast<void*>(next_function_id_++);
  }
  return static_cast<unsigned>(reinterpret_cast<uintptr_t>(entry->value));
}

ProfileNode* ProfileTree::AddPathFromEnd(const std::vector<CodeEntry*>& path,
                                         int src_line, bool update_stats) {
  ProfileNode* node = root_;
  CodeEntry* last_entry = NULL;
  for (auto it = path.rbegin(); it != path.rend(); ++it) {
    if (*it == NULL) continue;
    last_entry = *it;
    node = node->FindOrAddChild(*it);
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


struct NodesPair {
  NodesPair(ProfileNode* src, ProfileNode* dst)
      : src(src), dst(dst) { }
  ProfileNode* src;
  ProfileNode* dst;
};


class Position {
 public:
  explicit Position(ProfileNode* node)
      : node(node), child_idx_(0) { }
  INLINE(ProfileNode* current_child()) {
    return node->children()->at(child_idx_);
  }
  INLINE(bool has_current_child()) {
    return child_idx_ < node->children()->length();
  }
  INLINE(void next_child()) { ++child_idx_; }

  ProfileNode* node;
 private:
  int child_idx_;
};


// Non-recursive implementation of a depth-first post-order tree traversal.
template <typename Callback>
void ProfileTree::TraverseDepthFirst(Callback* callback) {
  List<Position> stack(10);
  stack.Add(Position(root_));
  while (stack.length() > 0) {
    Position& current = stack.last();
    if (current.has_current_child()) {
      callback->BeforeTraversingChild(current.node, current.current_child());
      stack.Add(Position(current.current_child()));
    } else {
      callback->AfterAllChildrenTraversed(current.node);
      if (stack.length() > 1) {
        Position& parent = stack[stack.length() - 2];
        callback->AfterChildTraversed(parent.node, current.node);
        parent.next_child();
      }
      // Remove child from the stack.
      stack.RemoveLast();
    }
  }
}

using v8::tracing::TracedValue;

CpuProfile::CpuProfile(CpuProfiler* profiler, const char* title,
                       bool record_samples)
    : title_(title),
      record_samples_(record_samples),
      start_time_(base::TimeTicks::HighResolutionNow()),
      top_down_(profiler->isolate()),
      profiler_(profiler),
      streaming_next_sample_(0) {
  auto value = TracedValue::Create();
  value->SetDouble("startTime",
                   (start_time_ - base::TimeTicks()).InMicroseconds());
  TRACE_EVENT_SAMPLE_WITH_ID1(TRACE_DISABLED_BY_DEFAULT("v8.cpu_profiler"),
                              "Profile", this, "data", std::move(value));
}

void CpuProfile::AddPath(base::TimeTicks timestamp,
                         const std::vector<CodeEntry*>& path, int src_line,
                         bool update_stats) {
  ProfileNode* top_frame_node =
      top_down_.AddPathFromEnd(path, src_line, update_stats);
  if (record_samples_ && !timestamp.IsNull()) {
    timestamps_.Add(timestamp);
    samples_.Add(top_frame_node);
  }
  const int kSamplesFlushCount = 100;
  const int kNodesFlushCount = 10;
  if (samples_.length() - streaming_next_sample_ >= kSamplesFlushCount ||
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
  if (pending_nodes.empty() && !samples_.length()) return;
  auto value = TracedValue::Create();

  if (!pending_nodes.empty() || streaming_next_sample_ != samples_.length()) {
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
    if (streaming_next_sample_ != samples_.length()) {
      value->BeginArray("samples");
      for (int i = streaming_next_sample_; i < samples_.length(); ++i) {
        value->AppendInteger(samples_[i]->id());
      }
      value->EndArray();
    }
    value->EndDictionary();
  }
  if (streaming_next_sample_ != samples_.length()) {
    value->BeginArray("timeDeltas");
    base::TimeTicks lastTimestamp =
        streaming_next_sample_ ? timestamps_[streaming_next_sample_ - 1]
                               : start_time();
    for (int i = streaming_next_sample_; i < timestamps_.length(); ++i) {
      value->AppendInteger(
          static_cast<int>((timestamps_[i] - lastTimestamp).InMicroseconds()));
      lastTimestamp = timestamps_[i];
    }
    value->EndArray();
    DCHECK(samples_.length() == timestamps_.length());
    streaming_next_sample_ = samples_.length();
  }

  TRACE_EVENT_SAMPLE_WITH_ID1(TRACE_DISABLED_BY_DEFAULT("v8.cpu_profiler"),
                              "ProfileChunk", this, "data", std::move(value));
}

void CpuProfile::FinishProfile() {
  end_time_ = base::TimeTicks::HighResolutionNow();
  StreamPendingTraceEvents();
  auto value = TracedValue::Create();
  value->SetDouble("endTime", (end_time_ - base::TimeTicks()).InMicroseconds());
  TRACE_EVENT_SAMPLE_WITH_ID1(TRACE_DISABLED_BY_DEFAULT("v8.cpu_profiler"),
                              "ProfileChunk", this, "data", std::move(value));
}

void CpuProfile::Print() {
  base::OS::Print("[Top down]:\n");
  top_down_.Print();
}

void CodeMap::AddCode(Address addr, CodeEntry* entry, unsigned size) {
  DeleteAllCoveredCode(addr, addr + size);
  code_map_.insert({addr, CodeEntryInfo(entry, size)});
}

void CodeMap::DeleteAllCoveredCode(Address start, Address end) {
  auto left = code_map_.upper_bound(start);
  if (left != code_map_.begin()) {
    --left;
    if (left->first + left->second.size <= start) ++left;
  }
  auto right = left;
  while (right != code_map_.end() && right->first < end) ++right;
  code_map_.erase(left, right);
}

CodeEntry* CodeMap::FindEntry(Address addr) {
  auto it = code_map_.upper_bound(addr);
  if (it == code_map_.begin()) return nullptr;
  --it;
  Address end_address = it->first + it->second.size;
  return addr < end_address ? it->second.entry : nullptr;
}

void CodeMap::MoveCode(Address from, Address to) {
  if (from == to) return;
  auto it = code_map_.find(from);
  if (it == code_map_.end()) return;
  CodeEntryInfo info = it->second;
  code_map_.erase(it);
  AddCode(to, info.entry, info.size);
}

void CodeMap::Print() {
  for (auto it = code_map_.begin(); it != code_map_.end(); ++it) {
    base::OS::Print("%p %5d %s\n", static_cast<void*>(it->first),
                    it->second.size, it->second.entry->name());
  }
}

CpuProfilesCollection::CpuProfilesCollection(Isolate* isolate)
    : resource_names_(isolate->heap()),
      profiler_(nullptr),
      current_profiles_semaphore_(1) {}

static void DeleteCpuProfile(CpuProfile** profile_ptr) {
  delete *profile_ptr;
}


CpuProfilesCollection::~CpuProfilesCollection() {
  finished_profiles_.Iterate(DeleteCpuProfile);
  current_profiles_.Iterate(DeleteCpuProfile);
}


bool CpuProfilesCollection::StartProfiling(const char* title,
                                           bool record_samples) {
  current_profiles_semaphore_.Wait();
  if (current_profiles_.length() >= kMaxSimultaneousProfiles) {
    current_profiles_semaphore_.Signal();
    return false;
  }
  for (int i = 0; i < current_profiles_.length(); ++i) {
    if (strcmp(current_profiles_[i]->title(), title) == 0) {
      // Ignore attempts to start profile with the same title...
      current_profiles_semaphore_.Signal();
      // ... though return true to force it collect a sample.
      return true;
    }
  }
  current_profiles_.Add(new CpuProfile(profiler_, title, record_samples));
  current_profiles_semaphore_.Signal();
  return true;
}


CpuProfile* CpuProfilesCollection::StopProfiling(const char* title) {
  const int title_len = StrLength(title);
  CpuProfile* profile = nullptr;
  current_profiles_semaphore_.Wait();
  for (int i = current_profiles_.length() - 1; i >= 0; --i) {
    if (title_len == 0 || strcmp(current_profiles_[i]->title(), title) == 0) {
      profile = current_profiles_.Remove(i);
      break;
    }
  }
  current_profiles_semaphore_.Signal();

  if (!profile) return nullptr;
  profile->FinishProfile();
  finished_profiles_.Add(profile);
  return profile;
}


bool CpuProfilesCollection::IsLastProfile(const char* title) {
  // Called from VM thread, and only it can mutate the list,
  // so no locking is needed here.
  if (current_profiles_.length() != 1) return false;
  return StrLength(title) == 0
      || strcmp(current_profiles_[0]->title(), title) == 0;
}


void CpuProfilesCollection::RemoveProfile(CpuProfile* profile) {
  // Called from VM thread for a completed profile.
  for (int i = 0; i < finished_profiles_.length(); i++) {
    if (profile == finished_profiles_[i]) {
      finished_profiles_.Remove(i);
      return;
    }
  }
  UNREACHABLE();
}

void CpuProfilesCollection::AddPathToCurrentProfiles(
    base::TimeTicks timestamp, const std::vector<CodeEntry*>& path,
    int src_line, bool update_stats) {
  // As starting / stopping profiles is rare relatively to this
  // method, we don't bother minimizing the duration of lock holding,
  // e.g. copying contents of the list to a local vector.
  current_profiles_semaphore_.Wait();
  for (int i = 0; i < current_profiles_.length(); ++i) {
    current_profiles_[i]->AddPath(timestamp, path, src_line, update_stats);
  }
  current_profiles_semaphore_.Signal();
}

ProfileGenerator::ProfileGenerator(Isolate* isolate,
                                   CpuProfilesCollection* profiles)
    : isolate_(isolate), profiles_(profiles) {
  RuntimeCallStats* rcs = isolate_->counters()->runtime_call_stats();
  for (int i = 0; i < RuntimeCallStats::counters_count; ++i) {
    RuntimeCallCounter* counter = &(rcs->*(RuntimeCallStats::counters[i]));
    DCHECK(counter->name());
    auto entry = new CodeEntry(CodeEventListener::FUNCTION_TAG, counter->name(),
                               CodeEntry::kEmptyNamePrefix, "native V8Runtime");
    code_map_.AddCode(reinterpret_cast<Address>(counter), entry, 1);
  }
}

void ProfileGenerator::RecordTickSample(const TickSample& sample) {
  std::vector<CodeEntry*> entries;
  // Conservatively reserve space for stack frames + pc + function + vm-state.
  // There could in fact be more of them because of inlined entries.
  entries.reserve(sample.frames_count + 3);

  // The ProfileNode knows nothing about all versions of generated code for
  // the same JS function. The line number information associated with
  // the latest version of generated code is used to find a source line number
  // for a JS function. Then, the detected source line is passed to
  // ProfileNode to increase the tick count for this source line.
  int src_line = v8::CpuProfileNode::kNoLineNumberInfo;
  bool src_line_not_found = true;

  if (sample.pc != nullptr) {
    if (sample.has_external_callback && sample.state == EXTERNAL) {
      // Don't use PC when in external callback code, as it can point
      // inside callback's code, and we will erroneously report
      // that a callback calls itself.
      entries.push_back(FindEntry(sample.external_callback_entry));
    } else {
      CodeEntry* pc_entry = FindEntry(sample.pc);
      // If there is no pc_entry we're likely in native code.
      // Find out, if top of stack was pointing inside a JS function
      // meaning that we have encountered a frameless invocation.
      if (!pc_entry && !sample.has_external_callback) {
        pc_entry = FindEntry(sample.tos);
      }
      // If pc is in the function code before it set up stack frame or after the
      // frame was destroyed SafeStackFrameIterator incorrectly thinks that
      // ebp contains return address of the current function and skips caller's
      // frame. Check for this case and just skip such samples.
      if (pc_entry) {
        int pc_offset = static_cast<int>(reinterpret_cast<Address>(sample.pc) -
                                         pc_entry->instruction_start());
        src_line = pc_entry->GetSourceLine(pc_offset);
        if (src_line == v8::CpuProfileNode::kNoLineNumberInfo) {
          src_line = pc_entry->line_number();
        }
        src_line_not_found = false;
        entries.push_back(pc_entry);

        if (pc_entry->builtin_id() == Builtins::kFunctionPrototypeApply ||
            pc_entry->builtin_id() == Builtins::kFunctionPrototypeCall) {
          // When current function is either the Function.prototype.apply or the
          // Function.prototype.call builtin the top frame is either frame of
          // the calling JS function or internal frame.
          // In the latter case we know the caller for sure but in the
          // former case we don't so we simply replace the frame with
          // 'unresolved' entry.
          if (!sample.has_external_callback) {
            entries.push_back(CodeEntry::unresolved_entry());
          }
        }
      }
    }

    for (unsigned i = 0; i < sample.frames_count; ++i) {
      Address stack_pos = reinterpret_cast<Address>(sample.stack[i]);
      CodeEntry* entry = FindEntry(stack_pos);
      if (entry) {
        // Find out if the entry has an inlining stack associated.
        int pc_offset =
            static_cast<int>(stack_pos - entry->instruction_start());
        const std::vector<CodeEntry*>* inline_stack =
            entry->GetInlineStack(pc_offset);
        if (inline_stack) {
          entries.insert(entries.end(), inline_stack->rbegin(),
                         inline_stack->rend());
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
      }
      entries.push_back(entry);
    }
  }

  if (FLAG_prof_browser_mode) {
    bool no_symbolized_entries = true;
    for (auto e : entries) {
      if (e != NULL) {
        no_symbolized_entries = false;
        break;
      }
    }
    // If no frames were symbolized, put the VM state entry in.
    if (no_symbolized_entries) {
      entries.push_back(EntryForVMState(sample.state));
    }
  }

  profiles_->AddPathToCurrentProfiles(sample.timestamp, entries, src_line,
                                      sample.update_stats);
}

CodeEntry* ProfileGenerator::FindEntry(void* address) {
  return code_map_.FindEntry(reinterpret_cast<Address>(address));
}

CodeEntry* ProfileGenerator::EntryForVMState(StateTag tag) {
  switch (tag) {
    case GC:
      return CodeEntry::gc_entry();
    case JS:
    case COMPILER:
    // DOM events handlers are reported as OTHER / EXTERNAL entries.
    // To avoid confusing people, let's put all these entries into
    // one bucket.
    case OTHER:
    case EXTERNAL:
      return CodeEntry::program_entry();
    case IDLE:
      return CodeEntry::idle_entry();
    default: return NULL;
  }
}

}  // namespace internal
}  // namespace v8
