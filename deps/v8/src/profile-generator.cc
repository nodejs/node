// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/profile-generator-inl.h"

#include "src/compiler.h"
#include "src/debug.h"
#include "src/deoptimizer.h"
#include "src/global-handles.h"
#include "src/sampler.h"
#include "src/scopeinfo.h"
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


CodeEntry::~CodeEntry() {
  delete no_frame_ranges_;
  delete line_info_;
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
  bit_field_ = TagField::update(bit_field_, Logger::BUILTIN_TAG);
  bit_field_ = BuiltinIdField::update(bit_field_, id);
}


int CodeEntry::GetSourceLine(int pc_offset) const {
  if (line_info_ && !line_info_->empty()) {
    return line_info_->GetSourceLineNumber(pc_offset);
  }
  return v8::CpuProfileNode::kNoLineNumberInfo;
}


void CodeEntry::FillFunctionInfo(SharedFunctionInfo* shared) {
  if (!shared->script()->IsScript()) return;
  Script* script = Script::cast(shared->script());
  set_script_id(script->id()->value());
  set_position(shared->start_position());
  set_bailout_reason(GetBailoutReason(shared->disable_optimization_reason()));
}


CpuProfileDeoptInfo CodeEntry::GetDeoptInfo() {
  DCHECK(has_deopt_info());

  CpuProfileDeoptInfo info;
  info.deopt_reason = deopt_reason_;
  if (inlined_function_infos_.empty()) {
    info.stack.push_back(CpuProfileDeoptFrame(
        {script_id_, position_ + deopt_position_.position()}));
    return info;
  }
  // Copy the only branch from the inlining tree where the deopt happened.
  SourcePosition position = deopt_position_;
  int inlining_id = InlinedFunctionInfo::kNoParentId;
  for (size_t i = 0; i < inlined_function_infos_.size(); ++i) {
    InlinedFunctionInfo& current_info = inlined_function_infos_.at(i);
    if (std::binary_search(current_info.deopt_pc_offsets.begin(),
                           current_info.deopt_pc_offsets.end(), pc_offset_)) {
      inlining_id = static_cast<int>(i);
      break;
    }
  }
  while (inlining_id != InlinedFunctionInfo::kNoParentId) {
    InlinedFunctionInfo& inlined_info = inlined_function_infos_.at(inlining_id);
    info.stack.push_back(
        CpuProfileDeoptFrame({inlined_info.script_id,
                              inlined_info.start_position + position.raw()}));
    position = inlined_info.inline_position;
    inlining_id = inlined_info.parent_id;
  }
  return info;
}


void ProfileNode::CollectDeoptInfo(CodeEntry* entry) {
  deopt_infos_.push_back(entry->GetDeoptInfo());
  entry->clear_deopt_info();
}


ProfileNode* ProfileNode::FindChild(CodeEntry* entry) {
  HashMap::Entry* map_entry = children_.Lookup(entry, CodeEntryHash(entry));
  return map_entry != NULL ?
      reinterpret_cast<ProfileNode*>(map_entry->value) : NULL;
}


ProfileNode* ProfileNode::FindOrAddChild(CodeEntry* entry) {
  HashMap::Entry* map_entry =
      children_.LookupOrInsert(entry, CodeEntryHash(entry));
  ProfileNode* node = reinterpret_cast<ProfileNode*>(map_entry->value);
  if (node == NULL) {
    // New node added.
    node = new ProfileNode(tree_, entry);
    map_entry->value = node;
    children_list_.Add(node);
  }
  return node;
}


void ProfileNode::IncrementLineTicks(int src_line) {
  if (src_line == v8::CpuProfileNode::kNoLineNumberInfo) return;
  // Increment a hit counter of a certain source line.
  // Add a new source line if not found.
  HashMap::Entry* e =
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

  for (HashMap::Entry* p = line_ticks_.Start(); p != NULL;
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
    base::OS::Print(
        "%*s;;; deopted at script_id: %d position: %d with reason '%s'.\n",
        indent + 10, "", info.stack[0].script_id, info.stack[0].position,
        info.deopt_reason);
    for (size_t index = 1; index < info.stack.size(); ++index) {
      base::OS::Print("%*s;;;     Inline point: script_id %d position: %d.\n",
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
  for (HashMap::Entry* p = children_.Start();
       p != NULL;
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


ProfileTree::ProfileTree()
    : root_entry_(Logger::FUNCTION_TAG, "(root)"),
      next_node_id_(1),
      root_(new ProfileNode(this, &root_entry_)),
      next_function_id_(1),
      function_ids_(ProfileNode::CodeEntriesMatch) {}


ProfileTree::~ProfileTree() {
  DeleteNodesCallback cb;
  TraverseDepthFirst(&cb);
}


unsigned ProfileTree::GetFunctionId(const ProfileNode* node) {
  CodeEntry* code_entry = node->entry();
  HashMap::Entry* entry =
      function_ids_.LookupOrInsert(code_entry, code_entry->GetHash());
  if (!entry->value) {
    entry->value = reinterpret_cast<void*>(next_function_id_++);
  }
  return static_cast<unsigned>(reinterpret_cast<uintptr_t>(entry->value));
}


ProfileNode* ProfileTree::AddPathFromEnd(const Vector<CodeEntry*>& path,
                                         int src_line) {
  ProfileNode* node = root_;
  CodeEntry* last_entry = NULL;
  for (CodeEntry** entry = path.start() + path.length() - 1;
       entry != path.start() - 1;
       --entry) {
    if (*entry != NULL) {
      node = node->FindOrAddChild(*entry);
      last_entry = *entry;
    }
  }
  if (last_entry && last_entry->has_deopt_info()) {
    node->CollectDeoptInfo(last_entry);
  }
  node->IncrementSelfTicks();
  if (src_line != v8::CpuProfileNode::kNoLineNumberInfo) {
    node->IncrementLineTicks(src_line);
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


CpuProfile::CpuProfile(const char* title, bool record_samples)
    : title_(title),
      record_samples_(record_samples),
      start_time_(base::TimeTicks::HighResolutionNow()) {
}


void CpuProfile::AddPath(base::TimeTicks timestamp,
                         const Vector<CodeEntry*>& path, int src_line) {
  ProfileNode* top_frame_node = top_down_.AddPathFromEnd(path, src_line);
  if (record_samples_) {
    timestamps_.Add(timestamp);
    samples_.Add(top_frame_node);
  }
}


void CpuProfile::CalculateTotalTicksAndSamplingRate() {
  end_time_ = base::TimeTicks::HighResolutionNow();
}


void CpuProfile::Print() {
  base::OS::Print("[Top down]:\n");
  top_down_.Print();
}


const CodeMap::CodeTreeConfig::Key CodeMap::CodeTreeConfig::kNoKey = NULL;


void CodeMap::AddCode(Address addr, CodeEntry* entry, unsigned size) {
  DeleteAllCoveredCode(addr, addr + size);
  CodeTree::Locator locator;
  tree_.Insert(addr, &locator);
  locator.set_value(CodeEntryInfo(entry, size));
}


void CodeMap::DeleteAllCoveredCode(Address start, Address end) {
  List<Address> to_delete;
  Address addr = end - 1;
  while (addr >= start) {
    CodeTree::Locator locator;
    if (!tree_.FindGreatestLessThan(addr, &locator)) break;
    Address start2 = locator.key(), end2 = start2 + locator.value().size;
    if (start2 < end && start < end2) to_delete.Add(start2);
    addr = start2 - 1;
  }
  for (int i = 0; i < to_delete.length(); ++i) tree_.Remove(to_delete[i]);
}


CodeEntry* CodeMap::FindEntry(Address addr, Address* start) {
  CodeTree::Locator locator;
  if (tree_.FindGreatestLessThan(addr, &locator)) {
    // locator.key() <= addr. Need to check that addr is within entry.
    const CodeEntryInfo& entry = locator.value();
    if (addr < (locator.key() + entry.size)) {
      if (start) {
        *start = locator.key();
      }
      return entry.entry;
    }
  }
  return NULL;
}


void CodeMap::MoveCode(Address from, Address to) {
  if (from == to) return;
  CodeTree::Locator locator;
  if (!tree_.Find(from, &locator)) return;
  CodeEntryInfo entry = locator.value();
  tree_.Remove(from);
  AddCode(to, entry.entry, entry.size);
}


void CodeMap::CodeTreePrinter::Call(
    const Address& key, const CodeMap::CodeEntryInfo& value) {
  base::OS::Print("%p %5d %s\n", key, value.size, value.entry->name());
}


void CodeMap::Print() {
  CodeTreePrinter printer;
  tree_.ForEach(&printer);
}


CpuProfilesCollection::CpuProfilesCollection(Heap* heap)
    : function_and_resource_names_(heap),
      current_profiles_semaphore_(1) {
}


static void DeleteCodeEntry(CodeEntry** entry_ptr) {
  delete *entry_ptr;
}


static void DeleteCpuProfile(CpuProfile** profile_ptr) {
  delete *profile_ptr;
}


CpuProfilesCollection::~CpuProfilesCollection() {
  finished_profiles_.Iterate(DeleteCpuProfile);
  current_profiles_.Iterate(DeleteCpuProfile);
  code_entries_.Iterate(DeleteCodeEntry);
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
  current_profiles_.Add(new CpuProfile(title, record_samples));
  current_profiles_semaphore_.Signal();
  return true;
}


CpuProfile* CpuProfilesCollection::StopProfiling(const char* title) {
  const int title_len = StrLength(title);
  CpuProfile* profile = NULL;
  current_profiles_semaphore_.Wait();
  for (int i = current_profiles_.length() - 1; i >= 0; --i) {
    if (title_len == 0 || strcmp(current_profiles_[i]->title(), title) == 0) {
      profile = current_profiles_.Remove(i);
      break;
    }
  }
  current_profiles_semaphore_.Signal();

  if (profile == NULL) return NULL;
  profile->CalculateTotalTicksAndSamplingRate();
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
    base::TimeTicks timestamp, const Vector<CodeEntry*>& path, int src_line) {
  // As starting / stopping profiles is rare relatively to this
  // method, we don't bother minimizing the duration of lock holding,
  // e.g. copying contents of the list to a local vector.
  current_profiles_semaphore_.Wait();
  for (int i = 0; i < current_profiles_.length(); ++i) {
    current_profiles_[i]->AddPath(timestamp, path, src_line);
  }
  current_profiles_semaphore_.Signal();
}


CodeEntry* CpuProfilesCollection::NewCodeEntry(
    Logger::LogEventsAndTags tag, const char* name, const char* name_prefix,
    const char* resource_name, int line_number, int column_number,
    JITLineInfoTable* line_info, Address instruction_start) {
  CodeEntry* code_entry =
      new CodeEntry(tag, name, name_prefix, resource_name, line_number,
                    column_number, line_info, instruction_start);
  code_entries_.Add(code_entry);
  return code_entry;
}


const char* const ProfileGenerator::kProgramEntryName =
    "(program)";
const char* const ProfileGenerator::kIdleEntryName =
    "(idle)";
const char* const ProfileGenerator::kGarbageCollectorEntryName =
    "(garbage collector)";
const char* const ProfileGenerator::kUnresolvedFunctionName =
    "(unresolved function)";


ProfileGenerator::ProfileGenerator(CpuProfilesCollection* profiles)
    : profiles_(profiles),
      program_entry_(
          profiles->NewCodeEntry(Logger::FUNCTION_TAG, kProgramEntryName)),
      idle_entry_(
          profiles->NewCodeEntry(Logger::FUNCTION_TAG, kIdleEntryName)),
      gc_entry_(
          profiles->NewCodeEntry(Logger::BUILTIN_TAG,
                                 kGarbageCollectorEntryName)),
      unresolved_entry_(
          profiles->NewCodeEntry(Logger::FUNCTION_TAG,
                                 kUnresolvedFunctionName)) {
}


void ProfileGenerator::RecordTickSample(const TickSample& sample) {
  // Allocate space for stack frames + pc + function + vm-state.
  ScopedVector<CodeEntry*> entries(sample.frames_count + 3);
  // As actual number of decoded code entries may vary, initialize
  // entries vector with NULL values.
  CodeEntry** entry = entries.start();
  memset(entry, 0, entries.length() * sizeof(*entry));

  // The ProfileNode knows nothing about all versions of generated code for
  // the same JS function. The line number information associated with
  // the latest version of generated code is used to find a source line number
  // for a JS function. Then, the detected source line is passed to
  // ProfileNode to increase the tick count for this source line.
  int src_line = v8::CpuProfileNode::kNoLineNumberInfo;
  bool src_line_not_found = true;

  if (sample.pc != NULL) {
    if (sample.has_external_callback && sample.state == EXTERNAL &&
        sample.top_frame_type == StackFrame::EXIT) {
      // Don't use PC when in external callback code, as it can point
      // inside callback's code, and we will erroneously report
      // that a callback calls itself.
      *entry++ = code_map_.FindEntry(sample.external_callback);
    } else {
      Address start;
      CodeEntry* pc_entry = code_map_.FindEntry(sample.pc, &start);
      // If pc is in the function code before it set up stack frame or after the
      // frame was destroyed SafeStackFrameIterator incorrectly thinks that
      // ebp contains return address of the current function and skips caller's
      // frame. Check for this case and just skip such samples.
      if (pc_entry) {
        List<OffsetRange>* ranges = pc_entry->no_frame_ranges();
        int pc_offset =
            static_cast<int>(sample.pc - pc_entry->instruction_start());
        if (ranges) {
          for (int i = 0; i < ranges->length(); i++) {
            OffsetRange& range = ranges->at(i);
            if (range.from <= pc_offset && pc_offset < range.to) {
              return;
            }
          }
        }
        src_line = pc_entry->GetSourceLine(pc_offset);
        if (src_line == v8::CpuProfileNode::kNoLineNumberInfo) {
          src_line = pc_entry->line_number();
        }
        src_line_not_found = false;
        *entry++ = pc_entry;

        if (pc_entry->builtin_id() == Builtins::kFunctionCall ||
            pc_entry->builtin_id() == Builtins::kFunctionApply) {
          // When current function is FunctionCall or FunctionApply builtin the
          // top frame is either frame of the calling JS function or internal
          // frame. In the latter case we know the caller for sure but in the
          // former case we don't so we simply replace the frame with
          // 'unresolved' entry.
          if (sample.top_frame_type == StackFrame::JAVA_SCRIPT) {
            *entry++ = unresolved_entry_;
          }
        }
      }
    }

    for (const Address* stack_pos = sample.stack,
           *stack_end = stack_pos + sample.frames_count;
         stack_pos != stack_end;
         ++stack_pos) {
      Address start = NULL;
      *entry = code_map_.FindEntry(*stack_pos, &start);

      // Skip unresolved frames (e.g. internal frame) and get source line of
      // the first JS caller.
      if (src_line_not_found && *entry) {
        int pc_offset =
            static_cast<int>(*stack_pos - (*entry)->instruction_start());
        src_line = (*entry)->GetSourceLine(pc_offset);
        if (src_line == v8::CpuProfileNode::kNoLineNumberInfo) {
          src_line = (*entry)->line_number();
        }
        src_line_not_found = false;
      }

      entry++;
    }
  }

  if (FLAG_prof_browser_mode) {
    bool no_symbolized_entries = true;
    for (CodeEntry** e = entries.start(); e != entry; ++e) {
      if (*e != NULL) {
        no_symbolized_entries = false;
        break;
      }
    }
    // If no frames were symbolized, put the VM state entry in.
    if (no_symbolized_entries) {
      *entry++ = EntryForVMState(sample.state);
    }
  }

  profiles_->AddPathToCurrentProfiles(sample.timestamp, entries, src_line);
}


CodeEntry* ProfileGenerator::EntryForVMState(StateTag tag) {
  switch (tag) {
    case GC:
      return gc_entry_;
    case JS:
    case COMPILER:
    // DOM events handlers are reported as OTHER / EXTERNAL entries.
    // To avoid confusing people, let's put all these entries into
    // one bucket.
    case OTHER:
    case EXTERNAL:
      return program_entry_;
    case IDLE:
      return idle_entry_;
    default: return NULL;
  }
}

} }  // namespace v8::internal
