// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifdef ENABLE_LOGGING_AND_PROFILING

#include "v8.h"
#include "global-handles.h"
#include "scopeinfo.h"
#include "top.h"
#include "zone-inl.h"

#include "profile-generator-inl.h"

namespace v8 {
namespace internal {


TokenEnumerator::TokenEnumerator()
    : token_locations_(4),
      token_removed_(4) {
}


TokenEnumerator::~TokenEnumerator() {
  for (int i = 0; i < token_locations_.length(); ++i) {
    if (!token_removed_[i]) {
      GlobalHandles::ClearWeakness(token_locations_[i]);
      GlobalHandles::Destroy(token_locations_[i]);
    }
  }
}


int TokenEnumerator::GetTokenId(Object* token) {
  if (token == NULL) return TokenEnumerator::kNoSecurityToken;
  for (int i = 0; i < token_locations_.length(); ++i) {
    if (*token_locations_[i] == token && !token_removed_[i]) return i;
  }
  Handle<Object> handle = GlobalHandles::Create(token);
  // handle.location() points to a memory cell holding a pointer
  // to a token object in the V8's heap.
  GlobalHandles::MakeWeak(handle.location(), this, TokenRemovedCallback);
  token_locations_.Add(handle.location());
  token_removed_.Add(false);
  return token_locations_.length() - 1;
}


void TokenEnumerator::TokenRemovedCallback(v8::Persistent<v8::Value> handle,
                                           void* parameter) {
  reinterpret_cast<TokenEnumerator*>(parameter)->TokenRemoved(
      Utils::OpenHandle(*handle).location());
}


void TokenEnumerator::TokenRemoved(Object** token_location) {
  for (int i = 0; i < token_locations_.length(); ++i) {
    if (token_locations_[i] == token_location && !token_removed_[i]) {
      token_removed_[i] = true;
      return;
    }
  }
}


StringsStorage::StringsStorage()
    : names_(StringsMatch) {
}


StringsStorage::~StringsStorage() {
  for (HashMap::Entry* p = names_.Start();
       p != NULL;
       p = names_.Next(p)) {
    DeleteArray(reinterpret_cast<const char*>(p->value));
  }
}


const char* StringsStorage::GetName(String* name) {
  if (name->IsString()) {
    char* c_name =
        name->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL).Detach();
    HashMap::Entry* cache_entry = names_.Lookup(c_name, name->Hash(), true);
    if (cache_entry->value == NULL) {
      // New entry added.
      cache_entry->value = c_name;
    } else {
      DeleteArray(c_name);
    }
    return reinterpret_cast<const char*>(cache_entry->value);
  }
  return "";
}


const char* CodeEntry::kEmptyNamePrefix = "";
unsigned CodeEntry::next_call_uid_ = 1;


void CodeEntry::CopyData(const CodeEntry& source) {
  call_uid_ = source.call_uid_;
  tag_ = source.tag_;
  name_prefix_ = source.name_prefix_;
  name_ = source.name_;
  resource_name_ = source.resource_name_;
  line_number_ = source.line_number_;
}


ProfileNode* ProfileNode::FindChild(CodeEntry* entry) {
  HashMap::Entry* map_entry =
      children_.Lookup(entry, CodeEntryHash(entry), false);
  return map_entry != NULL ?
      reinterpret_cast<ProfileNode*>(map_entry->value) : NULL;
}


ProfileNode* ProfileNode::FindOrAddChild(CodeEntry* entry) {
  HashMap::Entry* map_entry =
      children_.Lookup(entry, CodeEntryHash(entry), true);
  if (map_entry->value == NULL) {
    // New node added.
    ProfileNode* new_node = new ProfileNode(tree_, entry);
    map_entry->value = new_node;
    children_list_.Add(new_node);
  }
  return reinterpret_cast<ProfileNode*>(map_entry->value);
}


double ProfileNode::GetSelfMillis() const {
  return tree_->TicksToMillis(self_ticks_);
}


double ProfileNode::GetTotalMillis() const {
  return tree_->TicksToMillis(total_ticks_);
}


void ProfileNode::Print(int indent) {
  OS::Print("%5u %5u %*c %s%s [%d]",
            total_ticks_, self_ticks_,
            indent, ' ',
            entry_->name_prefix(),
            entry_->name(),
            entry_->security_token_id());
  if (entry_->resource_name()[0] != '\0')
    OS::Print(" %s:%d", entry_->resource_name(), entry_->line_number());
  OS::Print("\n");
  for (HashMap::Entry* p = children_.Start();
       p != NULL;
       p = children_.Next(p)) {
    reinterpret_cast<ProfileNode*>(p->value)->Print(indent + 2);
  }
}


namespace {

class DeleteNodesCallback {
 public:
  void BeforeTraversingChild(ProfileNode*, ProfileNode*) { }

  void AfterAllChildrenTraversed(ProfileNode* node) {
    delete node;
  }

  void AfterChildTraversed(ProfileNode*, ProfileNode*) { }
};

}  // namespace


ProfileTree::ProfileTree()
    : root_entry_(Logger::FUNCTION_TAG,
                  "",
                  "(root)",
                  "",
                  0,
                  TokenEnumerator::kNoSecurityToken),
      root_(new ProfileNode(this, &root_entry_)) {
}


ProfileTree::~ProfileTree() {
  DeleteNodesCallback cb;
  TraverseDepthFirst(&cb);
}


void ProfileTree::AddPathFromEnd(const Vector<CodeEntry*>& path) {
  ProfileNode* node = root_;
  for (CodeEntry** entry = path.start() + path.length() - 1;
       entry != path.start() - 1;
       --entry) {
    if (*entry != NULL) {
      node = node->FindOrAddChild(*entry);
    }
  }
  node->IncrementSelfTicks();
}


void ProfileTree::AddPathFromStart(const Vector<CodeEntry*>& path) {
  ProfileNode* node = root_;
  for (CodeEntry** entry = path.start();
       entry != path.start() + path.length();
       ++entry) {
    if (*entry != NULL) {
      node = node->FindOrAddChild(*entry);
    }
  }
  node->IncrementSelfTicks();
}


namespace {

struct NodesPair {
  NodesPair(ProfileNode* src, ProfileNode* dst)
      : src(src), dst(dst) { }
  ProfileNode* src;
  ProfileNode* dst;
};


class FilteredCloneCallback {
 public:
  explicit FilteredCloneCallback(ProfileNode* dst_root, int security_token_id)
      : stack_(10),
        security_token_id_(security_token_id) {
    stack_.Add(NodesPair(NULL, dst_root));
  }

  void BeforeTraversingChild(ProfileNode* parent, ProfileNode* child) {
    if (IsTokenAcceptable(child->entry()->security_token_id(),
                          parent->entry()->security_token_id())) {
      ProfileNode* clone = stack_.last().dst->FindOrAddChild(child->entry());
      clone->IncreaseSelfTicks(child->self_ticks());
      stack_.Add(NodesPair(child, clone));
    } else {
      // Attribute ticks to parent node.
      stack_.last().dst->IncreaseSelfTicks(child->self_ticks());
    }
  }

  void AfterAllChildrenTraversed(ProfileNode* parent) { }

  void AfterChildTraversed(ProfileNode*, ProfileNode* child) {
    if (stack_.last().src == child) {
      stack_.RemoveLast();
    }
  }

 private:
  bool IsTokenAcceptable(int token, int parent_token) {
    if (token == TokenEnumerator::kNoSecurityToken
        || token == security_token_id_) return true;
    if (token == TokenEnumerator::kInheritsSecurityToken) {
      ASSERT(parent_token != TokenEnumerator::kInheritsSecurityToken);
      return parent_token == TokenEnumerator::kNoSecurityToken
          || parent_token == security_token_id_;
    }
    return false;
  }

  List<NodesPair> stack_;
  int security_token_id_;
};

}  // namespace

void ProfileTree::FilteredClone(ProfileTree* src, int security_token_id) {
  ms_to_ticks_scale_ = src->ms_to_ticks_scale_;
  FilteredCloneCallback cb(root_, security_token_id);
  src->TraverseDepthFirst(&cb);
  CalculateTotalTicks();
}


void ProfileTree::SetTickRatePerMs(double ticks_per_ms) {
  ms_to_ticks_scale_ = ticks_per_ms > 0 ? 1.0 / ticks_per_ms : 1.0;
}


namespace {

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

}  // namespace


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


namespace {

class CalculateTotalTicksCallback {
 public:
  void BeforeTraversingChild(ProfileNode*, ProfileNode*) { }

  void AfterAllChildrenTraversed(ProfileNode* node) {
    node->IncreaseTotalTicks(node->self_ticks());
  }

  void AfterChildTraversed(ProfileNode* parent, ProfileNode* child) {
    parent->IncreaseTotalTicks(child->total_ticks());
  }
};

}  // namespace


void ProfileTree::CalculateTotalTicks() {
  CalculateTotalTicksCallback cb;
  TraverseDepthFirst(&cb);
}


void ProfileTree::ShortPrint() {
  OS::Print("root: %u %u %.2fms %.2fms\n",
            root_->total_ticks(), root_->self_ticks(),
            root_->GetTotalMillis(), root_->GetSelfMillis());
}


void CpuProfile::AddPath(const Vector<CodeEntry*>& path) {
  top_down_.AddPathFromEnd(path);
  bottom_up_.AddPathFromStart(path);
}


void CpuProfile::CalculateTotalTicks() {
  top_down_.CalculateTotalTicks();
  bottom_up_.CalculateTotalTicks();
}


void CpuProfile::SetActualSamplingRate(double actual_sampling_rate) {
  top_down_.SetTickRatePerMs(actual_sampling_rate);
  bottom_up_.SetTickRatePerMs(actual_sampling_rate);
}


CpuProfile* CpuProfile::FilteredClone(int security_token_id) {
  ASSERT(security_token_id != TokenEnumerator::kNoSecurityToken);
  CpuProfile* clone = new CpuProfile(title_, uid_);
  clone->top_down_.FilteredClone(&top_down_, security_token_id);
  clone->bottom_up_.FilteredClone(&bottom_up_, security_token_id);
  return clone;
}


void CpuProfile::ShortPrint() {
  OS::Print("top down ");
  top_down_.ShortPrint();
  OS::Print("bottom up ");
  bottom_up_.ShortPrint();
}


void CpuProfile::Print() {
  OS::Print("[Top down]:\n");
  top_down_.Print();
  OS::Print("[Bottom up]:\n");
  bottom_up_.Print();
}


const CodeMap::CodeTreeConfig::Key CodeMap::CodeTreeConfig::kNoKey = NULL;
const CodeMap::CodeTreeConfig::Value CodeMap::CodeTreeConfig::kNoValue =
    CodeMap::CodeEntryInfo(NULL, 0);


void CodeMap::AddAlias(Address start, CodeEntry* entry, Address code_start) {
  CodeTree::Locator locator;
  if (tree_.Find(code_start, &locator)) {
    const CodeEntryInfo& code_info = locator.value();
    entry->CopyData(*code_info.entry);
    tree_.Insert(start, &locator);
    locator.set_value(CodeEntryInfo(entry, code_info.size));
  }
}


CodeEntry* CodeMap::FindEntry(Address addr) {
  CodeTree::Locator locator;
  if (tree_.FindGreatestLessThan(addr, &locator)) {
    // locator.key() <= addr. Need to check that addr is within entry.
    const CodeEntryInfo& entry = locator.value();
    if (addr < (locator.key() + entry.size))
      return entry.entry;
  }
  return NULL;
}


void CodeMap::CodeTreePrinter::Call(
    const Address& key, const CodeMap::CodeEntryInfo& value) {
  OS::Print("%p %5d %s\n", key, value.size, value.entry->name());
}


void CodeMap::Print() {
  CodeTreePrinter printer;
  tree_.ForEach(&printer);
}


CpuProfilesCollection::CpuProfilesCollection()
    : profiles_uids_(UidsMatch),
      current_profiles_semaphore_(OS::CreateSemaphore(1)) {
  // Create list of unabridged profiles.
  profiles_by_token_.Add(new List<CpuProfile*>());
}


static void DeleteArgsCountName(char** name_ptr) {
  DeleteArray(*name_ptr);
}


static void DeleteCodeEntry(CodeEntry** entry_ptr) {
  delete *entry_ptr;
}

static void DeleteCpuProfile(CpuProfile** profile_ptr) {
  delete *profile_ptr;
}

static void DeleteProfilesList(List<CpuProfile*>** list_ptr) {
  (*list_ptr)->Iterate(DeleteCpuProfile);
  delete *list_ptr;
}

CpuProfilesCollection::~CpuProfilesCollection() {
  delete current_profiles_semaphore_;
  current_profiles_.Iterate(DeleteCpuProfile);
  profiles_by_token_.Iterate(DeleteProfilesList);
  code_entries_.Iterate(DeleteCodeEntry);
  args_count_names_.Iterate(DeleteArgsCountName);
}


bool CpuProfilesCollection::StartProfiling(const char* title, unsigned uid) {
  ASSERT(uid > 0);
  current_profiles_semaphore_->Wait();
  for (int i = 0; i < current_profiles_.length(); ++i) {
    if (strcmp(current_profiles_[i]->title(), title) == 0) {
      // Ignore attempts to start profile with the same title.
      current_profiles_semaphore_->Signal();
      return false;
    }
  }
  current_profiles_.Add(new CpuProfile(title, uid));
  current_profiles_semaphore_->Signal();
  return true;
}


bool CpuProfilesCollection::StartProfiling(String* title, unsigned uid) {
  return StartProfiling(GetName(title), uid);
}


CpuProfile* CpuProfilesCollection::StopProfiling(int security_token_id,
                                                 const char* title,
                                                 double actual_sampling_rate) {
  const int title_len = StrLength(title);
  CpuProfile* profile = NULL;
  current_profiles_semaphore_->Wait();
  for (int i = current_profiles_.length() - 1; i >= 0; --i) {
    if (title_len == 0 || strcmp(current_profiles_[i]->title(), title) == 0) {
      profile = current_profiles_.Remove(i);
      break;
    }
  }
  current_profiles_semaphore_->Signal();

  if (profile != NULL) {
    profile->CalculateTotalTicks();
    profile->SetActualSamplingRate(actual_sampling_rate);
    List<CpuProfile*>* unabridged_list =
        profiles_by_token_[TokenToIndex(TokenEnumerator::kNoSecurityToken)];
    unabridged_list->Add(profile);
    HashMap::Entry* entry =
        profiles_uids_.Lookup(reinterpret_cast<void*>(profile->uid()),
                              static_cast<uint32_t>(profile->uid()),
                              true);
    ASSERT(entry->value == NULL);
    entry->value = reinterpret_cast<void*>(unabridged_list->length() - 1);
    return GetProfile(security_token_id, profile->uid());
  }
  return NULL;
}


CpuProfile* CpuProfilesCollection::StopProfiling(int security_token_id,
                                                 String* title,
                                                 double actual_sampling_rate) {
  return StopProfiling(security_token_id, GetName(title), actual_sampling_rate);
}


CpuProfile* CpuProfilesCollection::GetProfile(int security_token_id,
                                              unsigned uid) {
  HashMap::Entry* entry = profiles_uids_.Lookup(reinterpret_cast<void*>(uid),
                                                static_cast<uint32_t>(uid),
                                                false);
  int index;
  if (entry != NULL) {
    index = static_cast<int>(reinterpret_cast<intptr_t>(entry->value));
  } else {
    return NULL;
  }
  List<CpuProfile*>* unabridged_list =
      profiles_by_token_[TokenToIndex(TokenEnumerator::kNoSecurityToken)];
  if (security_token_id == TokenEnumerator::kNoSecurityToken) {
    return unabridged_list->at(index);
  }
  List<CpuProfile*>* list = GetProfilesList(security_token_id);
  if (list->at(index) == NULL) {
      list->at(index) =
          unabridged_list->at(index)->FilteredClone(security_token_id);
  }
  return list->at(index);
}


int CpuProfilesCollection::TokenToIndex(int security_token_id) {
  ASSERT(TokenEnumerator::kNoSecurityToken == -1);
  return security_token_id + 1;  // kNoSecurityToken -> 0, 0 -> 1, ...
}


List<CpuProfile*>* CpuProfilesCollection::GetProfilesList(
    int security_token_id) {
  const int index = TokenToIndex(security_token_id);
  const int lists_to_add = index - profiles_by_token_.length() + 1;
  if (lists_to_add > 0) profiles_by_token_.AddBlock(NULL, lists_to_add);
  List<CpuProfile*>* unabridged_list =
      profiles_by_token_[TokenToIndex(TokenEnumerator::kNoSecurityToken)];
  const int current_count = unabridged_list->length();
  if (profiles_by_token_[index] == NULL) {
    profiles_by_token_[index] = new List<CpuProfile*>(current_count);
  }
  List<CpuProfile*>* list = profiles_by_token_[index];
  const int profiles_to_add = current_count - list->length();
  if (profiles_to_add > 0) list->AddBlock(NULL, profiles_to_add);
  return list;
}


List<CpuProfile*>* CpuProfilesCollection::Profiles(int security_token_id) {
  List<CpuProfile*>* unabridged_list =
      profiles_by_token_[TokenToIndex(TokenEnumerator::kNoSecurityToken)];
  if (security_token_id == TokenEnumerator::kNoSecurityToken) {
    return unabridged_list;
  }
  List<CpuProfile*>* list = GetProfilesList(security_token_id);
  const int current_count = unabridged_list->length();
  for (int i = 0; i < current_count; ++i) {
    if (list->at(i) == NULL) {
      list->at(i) = unabridged_list->at(i)->FilteredClone(security_token_id);
    }
  }
  return list;
}


CodeEntry* CpuProfilesCollection::NewCodeEntry(Logger::LogEventsAndTags tag,
                                               String* name,
                                               String* resource_name,
                                               int line_number) {
  CodeEntry* entry = new CodeEntry(tag,
                                   CodeEntry::kEmptyNamePrefix,
                                   GetFunctionName(name),
                                   GetName(resource_name),
                                   line_number,
                                   TokenEnumerator::kNoSecurityToken);
  code_entries_.Add(entry);
  return entry;
}


CodeEntry* CpuProfilesCollection::NewCodeEntry(Logger::LogEventsAndTags tag,
                                               const char* name) {
  CodeEntry* entry = new CodeEntry(tag,
                                   CodeEntry::kEmptyNamePrefix,
                                   GetFunctionName(name),
                                   "",
                                   v8::CpuProfileNode::kNoLineNumberInfo,
                                   TokenEnumerator::kNoSecurityToken);
  code_entries_.Add(entry);
  return entry;
}


CodeEntry* CpuProfilesCollection::NewCodeEntry(Logger::LogEventsAndTags tag,
                                               const char* name_prefix,
                                               String* name) {
  CodeEntry* entry = new CodeEntry(tag,
                                   name_prefix,
                                   GetName(name),
                                   "",
                                   v8::CpuProfileNode::kNoLineNumberInfo,
                                   TokenEnumerator::kInheritsSecurityToken);
  code_entries_.Add(entry);
  return entry;
}


CodeEntry* CpuProfilesCollection::NewCodeEntry(Logger::LogEventsAndTags tag,
                                               int args_count) {
  CodeEntry* entry = new CodeEntry(tag,
                                   "args_count: ",
                                   GetName(args_count),
                                   "",
                                   v8::CpuProfileNode::kNoLineNumberInfo,
                                   TokenEnumerator::kInheritsSecurityToken);
  code_entries_.Add(entry);
  return entry;
}


CodeEntry* CpuProfilesCollection::NewCodeEntry(int security_token_id) {
  CodeEntry* entry = new CodeEntry(security_token_id);
  code_entries_.Add(entry);
  return entry;
}


const char* CpuProfilesCollection::GetName(int args_count) {
  ASSERT(args_count >= 0);
  if (args_count_names_.length() <= args_count) {
    args_count_names_.AddBlock(
        NULL, args_count - args_count_names_.length() + 1);
  }
  if (args_count_names_[args_count] == NULL) {
    const int kMaximumNameLength = 32;
    char* name = NewArray<char>(kMaximumNameLength);
    OS::SNPrintF(Vector<char>(name, kMaximumNameLength), "%d", args_count);
    args_count_names_[args_count] = name;
  }
  return args_count_names_[args_count];
}


void CpuProfilesCollection::AddPathToCurrentProfiles(
    const Vector<CodeEntry*>& path) {
  // As starting / stopping profiles is rare relatively to this
  // method, we don't bother minimizing the duration of lock holding,
  // e.g. copying contents of the list to a local vector.
  current_profiles_semaphore_->Wait();
  for (int i = 0; i < current_profiles_.length(); ++i) {
    current_profiles_[i]->AddPath(path);
  }
  current_profiles_semaphore_->Signal();
}


void SampleRateCalculator::Tick() {
  if (--wall_time_query_countdown_ == 0)
    UpdateMeasurements(OS::TimeCurrentMillis());
}


void SampleRateCalculator::UpdateMeasurements(double current_time) {
  if (measurements_count_++ != 0) {
    const double measured_ticks_per_ms =
        (kWallTimeQueryIntervalMs * ticks_per_ms_) /
        (current_time - last_wall_time_);
    // Update the average value.
    ticks_per_ms_ +=
        (measured_ticks_per_ms - ticks_per_ms_) / measurements_count_;
    // Update the externally accessible result.
    result_ = static_cast<AtomicWord>(ticks_per_ms_ * kResultScale);
  }
  last_wall_time_ = current_time;
  wall_time_query_countdown_ =
      static_cast<unsigned>(kWallTimeQueryIntervalMs * ticks_per_ms_);
}


const char* ProfileGenerator::kAnonymousFunctionName = "(anonymous function)";
const char* ProfileGenerator::kProgramEntryName = "(program)";
const char* ProfileGenerator::kGarbageCollectorEntryName =
  "(garbage collector)";


ProfileGenerator::ProfileGenerator(CpuProfilesCollection* profiles)
    : profiles_(profiles),
      program_entry_(
          profiles->NewCodeEntry(Logger::FUNCTION_TAG, kProgramEntryName)),
      gc_entry_(
          profiles->NewCodeEntry(Logger::BUILTIN_TAG,
                                 kGarbageCollectorEntryName)) {
}


void ProfileGenerator::RecordTickSample(const TickSample& sample) {
  // Allocate space for stack frames + pc + function + vm-state.
  ScopedVector<CodeEntry*> entries(sample.frames_count + 3);
  // As actual number of decoded code entries may vary, initialize
  // entries vector with NULL values.
  CodeEntry** entry = entries.start();
  memset(entry, 0, entries.length() * sizeof(*entry));
  if (sample.pc != NULL) {
    *entry++ = code_map_.FindEntry(sample.pc);

    if (sample.function != NULL) {
      *entry = code_map_.FindEntry(sample.function);
      if (*entry != NULL && !(*entry)->is_js_function()) {
        *entry = NULL;
      } else {
        CodeEntry* pc_entry = *entries.start();
        if (pc_entry == NULL) {
          *entry = NULL;
        } else if (pc_entry->is_js_function()) {
          // Use function entry in favor of pc entry, as function
          // entry has security token.
          *entries.start() = NULL;
        }
      }
      entry++;
    }

    for (const Address *stack_pos = sample.stack,
           *stack_end = stack_pos + sample.frames_count;
         stack_pos != stack_end;
         ++stack_pos) {
      *entry++ = code_map_.FindEntry(*stack_pos);
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

  profiles_->AddPathToCurrentProfiles(entries);
}


HeapGraphEdge::HeapGraphEdge(Type type,
                             const char* name,
                             HeapEntry* from,
                             HeapEntry* to)
    : type_(type), name_(name), from_(from), to_(to) {
  ASSERT(type_ == CONTEXT_VARIABLE || type_ == PROPERTY || type_ == INTERNAL);
}


HeapGraphEdge::HeapGraphEdge(int index,
                             HeapEntry* from,
                             HeapEntry* to)
    : type_(ELEMENT), index_(index), from_(from), to_(to) {
}


static void DeleteHeapGraphEdge(HeapGraphEdge** edge_ptr) {
  delete *edge_ptr;
}


static void DeleteHeapGraphPath(HeapGraphPath** path_ptr) {
  delete *path_ptr;
}


HeapEntry::~HeapEntry() {
  children_.Iterate(DeleteHeapGraphEdge);
  retaining_paths_.Iterate(DeleteHeapGraphPath);
}


void HeapEntry::AddEdge(HeapGraphEdge* edge) {
  children_.Add(edge);
  edge->to()->retainers_.Add(edge);
}


void HeapEntry::SetClosureReference(const char* name, HeapEntry* entry) {
  AddEdge(
      new HeapGraphEdge(HeapGraphEdge::CONTEXT_VARIABLE, name, this, entry));
}


void HeapEntry::SetElementReference(int index, HeapEntry* entry) {
  AddEdge(new HeapGraphEdge(index, this, entry));
}


void HeapEntry::SetInternalReference(const char* name, HeapEntry* entry) {
  AddEdge(new HeapGraphEdge(HeapGraphEdge::INTERNAL, name, this, entry));
}


void HeapEntry::SetPropertyReference(const char* name, HeapEntry* entry) {
  AddEdge(new HeapGraphEdge(HeapGraphEdge::PROPERTY, name, this, entry));
}


void HeapEntry::SetAutoIndexReference(HeapEntry* entry) {
  SetElementReference(next_auto_index_++, entry);
}


int HeapEntry::TotalSize() {
  return total_size_ != kUnknownSize ? total_size_ : CalculateTotalSize();
}


int HeapEntry::NonSharedTotalSize() {
  return non_shared_total_size_ != kUnknownSize ?
      non_shared_total_size_ : CalculateNonSharedTotalSize();
}


int HeapEntry::CalculateTotalSize() {
  snapshot_->ClearPaint();
  List<HeapEntry*> list(10);
  list.Add(this);
  total_size_ = self_size_;
  this->PaintReachable();
  while (!list.is_empty()) {
    HeapEntry* entry = list.RemoveLast();
    const int children_count = entry->children_.length();
    for (int i = 0; i < children_count; ++i) {
      HeapEntry* child = entry->children_[i]->to();
      if (!child->painted_reachable()) {
        list.Add(child);
        child->PaintReachable();
        total_size_ += child->self_size_;
      }
    }
  }
  return total_size_;
}


namespace {

class NonSharedSizeCalculator {
 public:
  NonSharedSizeCalculator()
      : non_shared_total_size_(0) {
  }

  int non_shared_total_size() const { return non_shared_total_size_; }

  void Apply(HeapEntry* entry) {
    if (entry->painted_reachable()) {
      non_shared_total_size_ += entry->self_size();
    }
  }

 private:
  int non_shared_total_size_;
};

}  // namespace

int HeapEntry::CalculateNonSharedTotalSize() {
  // To calculate non-shared total size, first we paint all reachable
  // nodes in one color, then we paint all nodes reachable from other
  // nodes with a different color. Then we consider only nodes painted
  // with the first color for caclulating the total size.
  snapshot_->ClearPaint();
  List<HeapEntry*> list(10);
  list.Add(this);
  this->PaintReachable();
  while (!list.is_empty()) {
    HeapEntry* entry = list.RemoveLast();
    const int children_count = entry->children_.length();
    for (int i = 0; i < children_count; ++i) {
      HeapEntry* child = entry->children_[i]->to();
      if (!child->painted_reachable()) {
        list.Add(child);
        child->PaintReachable();
      }
    }
  }

  List<HeapEntry*> list2(10);
  if (this != snapshot_->root()) {
    list2.Add(snapshot_->root());
    snapshot_->root()->PaintReachableFromOthers();
  }
  while (!list2.is_empty()) {
    HeapEntry* entry = list2.RemoveLast();
    const int children_count = entry->children_.length();
    for (int i = 0; i < children_count; ++i) {
      HeapEntry* child = entry->children_[i]->to();
      if (child != this && child->not_painted_reachable_from_others()) {
        list2.Add(child);
        child->PaintReachableFromOthers();
      }
    }
  }

  NonSharedSizeCalculator calculator;
  snapshot_->IterateEntries(&calculator);
  return calculator.non_shared_total_size();
}


class CachedHeapGraphPath {
 public:
  CachedHeapGraphPath()
      : nodes_(NodesMatch) { }
  CachedHeapGraphPath(const CachedHeapGraphPath& src)
      : nodes_(NodesMatch, &HashMap::DefaultAllocator, src.nodes_.capacity()),
        path_(src.path_.length() + 1) {
    for (HashMap::Entry* p = src.nodes_.Start();
         p != NULL;
         p = src.nodes_.Next(p)) {
      nodes_.Lookup(p->key, p->hash, true);
    }
    path_.AddAll(src.path_);
  }
  void Add(HeapGraphEdge* edge) {
    nodes_.Lookup(edge->to(), Hash(edge->to()), true);
    path_.Add(edge);
  }
  bool ContainsNode(HeapEntry* node) {
    return nodes_.Lookup(node, Hash(node), false) != NULL;
  }
  const List<HeapGraphEdge*>* path() const { return &path_; }

 private:
  static uint32_t Hash(HeapEntry* entry) {
    return static_cast<uint32_t>(reinterpret_cast<intptr_t>(entry));
  }
  static bool NodesMatch(void* key1, void* key2) { return key1 == key2; }

  HashMap nodes_;
  List<HeapGraphEdge*> path_;
};


const List<HeapGraphPath*>* HeapEntry::GetRetainingPaths() {
  if (retaining_paths_.length() == 0 && retainers_.length() != 0) {
    CachedHeapGraphPath path;
    FindRetainingPaths(this, &path);
  }
  return &retaining_paths_;
}


void HeapEntry::FindRetainingPaths(HeapEntry* node,
                                   CachedHeapGraphPath* prev_path) {
  for (int i = 0; i < node->retainers_.length(); ++i) {
    HeapGraphEdge* ret_edge = node->retainers_[i];
    if (prev_path->ContainsNode(ret_edge->from())) continue;
    if (ret_edge->from() != snapshot_->root()) {
      CachedHeapGraphPath path(*prev_path);
      path.Add(ret_edge);
      FindRetainingPaths(ret_edge->from(), &path);
    } else {
      HeapGraphPath* ret_path = new HeapGraphPath(*prev_path->path());
      ret_path->Set(0, ret_edge);
      retaining_paths_.Add(ret_path);
    }
  }
}


static void RemoveEdge(List<HeapGraphEdge*>* list, HeapGraphEdge* edge) {
  for (int i = 0; i < list->length(); ) {
    if (list->at(i) == edge) {
      list->Remove(i);
      return;
    } else {
      ++i;
    }
  }
  UNREACHABLE();
}


void HeapEntry::RemoveChild(HeapGraphEdge* edge) {
  RemoveEdge(&children_, edge);
  delete edge;
}


void HeapEntry::RemoveRetainer(HeapGraphEdge* edge) {
  RemoveEdge(&retainers_, edge);
}


void HeapEntry::CutEdges() {
  for (int i = 0; i < children_.length(); ++i) {
    HeapGraphEdge* edge = children_[i];
    edge->to()->RemoveRetainer(edge);
  }
  children_.Iterate(DeleteHeapGraphEdge);
  children_.Clear();

  for (int i = 0; i < retainers_.length(); ++i) {
    HeapGraphEdge* edge = retainers_[i];
    edge->from()->RemoveChild(edge);
  }
  retainers_.Clear();
}


void HeapEntry::Print(int max_depth, int indent) {
  OS::Print("%6d %6d %6d ", self_size_, TotalSize(), NonSharedTotalSize());
  if (type_ != STRING) {
    OS::Print("%s %.40s\n", TypeAsString(), name_);
  } else {
    OS::Print("\"");
    const char* c = name_;
    while (*c && (c - name_) <= 40) {
      if (*c != '\n')
        OS::Print("%c", *c);
      else
        OS::Print("\\n");
      ++c;
    }
    OS::Print("\"\n");
  }
  if (--max_depth == 0) return;
  const int children_count = children_.length();
  for (int i = 0; i < children_count; ++i) {
    HeapGraphEdge* edge = children_[i];
    switch (edge->type()) {
      case HeapGraphEdge::CONTEXT_VARIABLE:
        OS::Print("  %*c #%s: ", indent, ' ', edge->name());
        break;
      case HeapGraphEdge::ELEMENT:
        OS::Print("  %*c %d: ", indent, ' ', edge->index());
        break;
      case HeapGraphEdge::INTERNAL:
        OS::Print("  %*c $%s: ", indent, ' ', edge->name());
        break;
      case HeapGraphEdge::PROPERTY:
        OS::Print("  %*c %s: ", indent, ' ', edge->name());
        break;
      default:
        OS::Print("!!! unknown edge type: %d ", edge->type());
    }
    edge->to()->Print(max_depth, indent + 2);
  }
}


const char* HeapEntry::TypeAsString() {
  switch (type_) {
    case INTERNAL: return "/internal/";
    case OBJECT: return "/object/";
    case CLOSURE: return "/closure/";
    case STRING: return "/string/";
    case CODE: return "/code/";
    case ARRAY: return "/array/";
    default: return "???";
  }
}


HeapGraphPath::HeapGraphPath(const List<HeapGraphEdge*>& path)
    : path_(path.length() + 1) {
  Add(NULL);
  for (int i = path.length() - 1; i >= 0; --i) {
    Add(path[i]);
  }
}


void HeapGraphPath::Print() {
  path_[0]->from()->Print(1, 0);
  for (int i = 0; i < path_.length(); ++i) {
    OS::Print(" -> ");
    HeapGraphEdge* edge = path_[i];
    switch (edge->type()) {
      case HeapGraphEdge::CONTEXT_VARIABLE:
        OS::Print("[#%s] ", edge->name());
        break;
      case HeapGraphEdge::ELEMENT:
        OS::Print("[%d] ", edge->index());
        break;
      case HeapGraphEdge::INTERNAL:
        OS::Print("[$%s] ", edge->name());
        break;
      case HeapGraphEdge::PROPERTY:
        OS::Print("[%s] ", edge->name());
        break;
      default:
        OS::Print("!!! unknown edge type: %d ", edge->type());
    }
    edge->to()->Print(1, 0);
  }
  OS::Print("\n");
}


class IndexedReferencesExtractor : public ObjectVisitor {
 public:
  IndexedReferencesExtractor(HeapSnapshot* snapshot, HeapEntry* parent)
      : snapshot_(snapshot),
        parent_(parent) {
  }

  void VisitPointer(Object** o) {
    if (!(*o)->IsHeapObject()) return;
    HeapEntry* entry = snapshot_->GetEntry(HeapObject::cast(*o));
    if (entry != NULL) {
      parent_->SetAutoIndexReference(entry);
    }
  }

  void VisitPointers(Object** start, Object** end) {
    for (Object** p = start; p < end; p++) VisitPointer(p);
  }

 private:
  HeapSnapshot* snapshot_;
  HeapEntry* parent_;
};


HeapEntriesMap::HeapEntriesMap()
    : entries_(HeapObjectsMatch) {
}


HeapEntriesMap::~HeapEntriesMap() {
  for (HashMap::Entry* p = entries_.Start();
       p != NULL;
       p = entries_.Next(p)) {
    if (!IsAlias(p->value)) delete reinterpret_cast<HeapEntry*>(p->value);
  }
}


void HeapEntriesMap::Alias(HeapObject* object, HeapEntry* entry) {
  HashMap::Entry* cache_entry = entries_.Lookup(object, Hash(object), true);
  if (cache_entry->value == NULL)
    cache_entry->value = reinterpret_cast<void*>(
        reinterpret_cast<intptr_t>(entry) | kAliasTag);
}


void HeapEntriesMap::Apply(void (HeapEntry::*Func)(void)) {
  for (HashMap::Entry* p = entries_.Start();
       p != NULL;
       p = entries_.Next(p)) {
    if (!IsAlias(p->value)) (reinterpret_cast<HeapEntry*>(p->value)->*Func)();
  }
}


HeapEntry* HeapEntriesMap::Map(HeapObject* object) {
  HashMap::Entry* cache_entry = entries_.Lookup(object, Hash(object), false);
  return cache_entry != NULL ?
      reinterpret_cast<HeapEntry*>(
          reinterpret_cast<intptr_t>(cache_entry->value) & (~kAliasTag)) : NULL;
}


void HeapEntriesMap::Pair(HeapObject* object, HeapEntry* entry) {
  HashMap::Entry* cache_entry = entries_.Lookup(object, Hash(object), true);
  ASSERT(cache_entry->value == NULL);
  cache_entry->value = entry;
}


HeapSnapshot::HeapSnapshot(HeapSnapshotsCollection* collection,
                           const char* title,
                           unsigned uid)
    : collection_(collection),
      title_(title),
      uid_(uid),
      root_(this) {
}


void HeapSnapshot::ClearPaint() {
  root_.ClearPaint();
  entries_.Apply(&HeapEntry::ClearPaint);
}


HeapEntry* HeapSnapshot::GetEntry(Object* obj) {
  if (!obj->IsHeapObject()) return NULL;
  HeapObject* object = HeapObject::cast(obj);

  {
    HeapEntry* existing = FindEntry(object);
    if (existing != NULL) return existing;
  }

  // Add new entry.
  if (object->IsJSFunction()) {
    JSFunction* func = JSFunction::cast(object);
    SharedFunctionInfo* shared = func->shared();
    String* name = String::cast(shared->name())->length() > 0 ?
        String::cast(shared->name()) : shared->inferred_name();
    return AddEntry(object, HeapEntry::CLOSURE, collection_->GetName(name));
  } else if (object->IsJSObject()) {
    return AddEntry(object,
                    HeapEntry::OBJECT,
                    collection_->GetName(
                        JSObject::cast(object)->constructor_name()));
  } else if (object->IsJSGlobalPropertyCell()) {
    HeapEntry* value = GetEntry(JSGlobalPropertyCell::cast(object)->value());
    // If GPC references an object that we have interest in, add the object.
    // We don't store HeapEntries for GPCs. Instead, we make our hash map
    // to point to object's HeapEntry by GPCs address.
    if (value != NULL) AddEntryAlias(object, value);
    return value;
  } else if (object->IsString()) {
    return AddEntry(object,
                    HeapEntry::STRING,
                    collection_->GetName(String::cast(object)));
  } else if (object->IsCode()) {
    return AddEntry(object, HeapEntry::CODE);
  } else if (object->IsSharedFunctionInfo()) {
    SharedFunctionInfo* shared = SharedFunctionInfo::cast(object);
    String* name = String::cast(shared->name())->length() > 0 ?
        String::cast(shared->name()) : shared->inferred_name();
    return AddEntry(object, HeapEntry::CODE, collection_->GetName(name));
  } else if (object->IsScript()) {
    Script* script = Script::cast(object);
    return AddEntry(object,
                    HeapEntry::CODE,
                    script->name()->IsString() ?
                    collection_->GetName(String::cast(script->name())) : "");
  } else if (object->IsFixedArray()) {
    return AddEntry(object, HeapEntry::ARRAY);
  }
  // No interest in this object.
  return NULL;
}


void HeapSnapshot::SetClosureReference(HeapEntry* parent,
                                       String* reference_name,
                                       Object* child) {
  HeapEntry* child_entry = GetEntry(child);
  if (child_entry != NULL) {
    parent->SetClosureReference(
        collection_->GetName(reference_name), child_entry);
  }
}


void HeapSnapshot::SetElementReference(HeapEntry* parent,
                                       int index,
                                       Object* child) {
  HeapEntry* child_entry = GetEntry(child);
  if (child_entry != NULL) {
    parent->SetElementReference(index, child_entry);
  }
}


void HeapSnapshot::SetInternalReference(HeapEntry* parent,
                                        const char* reference_name,
                                        Object* child) {
  HeapEntry* child_entry = GetEntry(child);
  if (child_entry != NULL) {
    parent->SetInternalReference(reference_name, child_entry);
  }
}


void HeapSnapshot::SetPropertyReference(HeapEntry* parent,
                                        String* reference_name,
                                        Object* child) {
  HeapEntry* child_entry = GetEntry(child);
  if (child_entry != NULL) {
    parent->SetPropertyReference(
        collection_->GetName(reference_name), child_entry);
  }
}


HeapEntry* HeapSnapshot::AddEntry(HeapObject* object,
                                  HeapEntry::Type type,
                                  const char* name) {
  HeapEntry* entry = new HeapEntry(this,
                                   type,
                                   name,
                                   GetObjectSize(object),
                                   GetObjectSecurityToken(object));
  entries_.Pair(object, entry);

  // Detect, if this is a JS global object of the current context, and
  // add it to snapshot's roots. There can be several JS global objects
  // in a context.
  if (object->IsJSGlobalProxy()) {
    int global_security_token = GetGlobalSecurityToken();
    int object_security_token =
        collection_->token_enumerator()->GetTokenId(
            Context::cast(
                JSGlobalProxy::cast(object)->context())->security_token());
    if (object_security_token == TokenEnumerator::kNoSecurityToken
        || object_security_token == global_security_token) {
      HeapEntry* global_object_entry =
          GetEntry(HeapObject::cast(object->map()->prototype()));
      ASSERT(global_object_entry != NULL);
      root_.SetAutoIndexReference(global_object_entry);
    }
  }

  return entry;
}


namespace {

class EdgesCutter {
 public:
  explicit EdgesCutter(int global_security_token)
      : global_security_token_(global_security_token) {
  }

  void Apply(HeapEntry* entry) {
    if (entry->security_token_id() != TokenEnumerator::kNoSecurityToken
        && entry->security_token_id() != global_security_token_) {
      entry->CutEdges();
    }
  }

 private:
  const int global_security_token_;
};

}  // namespace

void HeapSnapshot::CutObjectsFromForeignSecurityContexts() {
  EdgesCutter cutter(GetGlobalSecurityToken());
  entries_.Apply(&cutter);
}


int HeapSnapshot::GetGlobalSecurityToken() {
  return collection_->token_enumerator()->GetTokenId(
      Top::context()->global()->global_context()->security_token());
}


int HeapSnapshot::GetObjectSize(HeapObject* obj) {
  return obj->IsJSObject() ?
      CalculateNetworkSize(JSObject::cast(obj)) : obj->Size();
}


int HeapSnapshot::GetObjectSecurityToken(HeapObject* obj) {
  if (obj->IsGlobalContext()) {
    return collection_->token_enumerator()->GetTokenId(
        Context::cast(obj)->security_token());
  } else {
    return TokenEnumerator::kNoSecurityToken;
  }
}


int HeapSnapshot::CalculateNetworkSize(JSObject* obj) {
  int size = obj->Size();
  // If 'properties' and 'elements' are non-empty (thus, non-shared),
  // take their size into account.
  if (FixedArray::cast(obj->properties())->length() != 0) {
    size += obj->properties()->Size();
  }
  if (FixedArray::cast(obj->elements())->length() != 0) {
    size += obj->elements()->Size();
  }
  // For functions, also account non-empty context and literals sizes.
  if (obj->IsJSFunction()) {
    JSFunction* f = JSFunction::cast(obj);
    if (f->unchecked_context()->IsContext()) {
      size += f->context()->Size();
    }
    if (f->literals()->length() != 0) {
      size += f->literals()->Size();
    }
  }
  return size;
}


void HeapSnapshot::Print(int max_depth) {
  root_.Print(max_depth, 0);
}


HeapSnapshotsCollection::HeapSnapshotsCollection()
    : snapshots_uids_(HeapSnapshotsMatch),
      token_enumerator_(new TokenEnumerator()) {
}


static void DeleteHeapSnapshot(HeapSnapshot** snapshot_ptr) {
  delete *snapshot_ptr;
}


HeapSnapshotsCollection::~HeapSnapshotsCollection() {
  delete token_enumerator_;
  snapshots_.Iterate(DeleteHeapSnapshot);
}


HeapSnapshot* HeapSnapshotsCollection::NewSnapshot(const char* name,
                                                   unsigned uid) {
  HeapSnapshot* snapshot = new HeapSnapshot(this, name, uid);
  snapshots_.Add(snapshot);
  HashMap::Entry* entry =
      snapshots_uids_.Lookup(reinterpret_cast<void*>(snapshot->uid()),
                             static_cast<uint32_t>(snapshot->uid()),
                             true);
  ASSERT(entry->value == NULL);
  entry->value = snapshot;
  return snapshot;
}


HeapSnapshot* HeapSnapshotsCollection::GetSnapshot(unsigned uid) {
  HashMap::Entry* entry = snapshots_uids_.Lookup(reinterpret_cast<void*>(uid),
                                                 static_cast<uint32_t>(uid),
                                                 false);
  return entry != NULL ? reinterpret_cast<HeapSnapshot*>(entry->value) : NULL;
}


HeapSnapshotGenerator::HeapSnapshotGenerator(HeapSnapshot* snapshot)
    : snapshot_(snapshot) {
}


void HeapSnapshotGenerator::GenerateSnapshot() {
  AssertNoAllocation no_alloc;

  // Iterate heap contents.
  HeapIterator iterator;
  for (HeapObject* obj = iterator.next(); obj != NULL; obj = iterator.next()) {
    ExtractReferences(obj);
  }

  snapshot_->CutObjectsFromForeignSecurityContexts();
}


void HeapSnapshotGenerator::ExtractReferences(HeapObject* obj) {
  HeapEntry* entry = snapshot_->GetEntry(obj);
  if (entry == NULL) return;
  if (entry->visited()) return;

  if (obj->IsJSObject()) {
    JSObject* js_obj = JSObject::cast(obj);
    ExtractClosureReferences(js_obj, entry);
    ExtractPropertyReferences(js_obj, entry);
    ExtractElementReferences(js_obj, entry);
    snapshot_->SetPropertyReference(
        entry, Heap::prototype_symbol(), js_obj->map()->prototype());
  } else if (obj->IsJSGlobalPropertyCell()) {
    JSGlobalPropertyCell* cell = JSGlobalPropertyCell::cast(obj);
    snapshot_->SetElementReference(entry, 0, cell->value());
  } else if (obj->IsString()) {
    if (obj->IsConsString()) {
      ConsString* cs = ConsString::cast(obj);
      snapshot_->SetElementReference(entry, 0, cs->first());
      snapshot_->SetElementReference(entry, 1, cs->second());
    }
  } else if (obj->IsCode() || obj->IsSharedFunctionInfo() || obj->IsScript()) {
    IndexedReferencesExtractor refs_extractor(snapshot_, entry);
    obj->Iterate(&refs_extractor);
  } else if (obj->IsFixedArray()) {
    IndexedReferencesExtractor refs_extractor(snapshot_, entry);
    obj->Iterate(&refs_extractor);
  }
  entry->MarkAsVisited();
}


void HeapSnapshotGenerator::ExtractClosureReferences(JSObject* js_obj,
                                                     HeapEntry* entry) {
  if (js_obj->IsJSFunction()) {
    HandleScope hs;
    JSFunction* func = JSFunction::cast(js_obj);
    Context* context = func->context();
    ZoneScope zscope(DELETE_ON_EXIT);
    SerializedScopeInfo* serialized_scope_info =
        context->closure()->shared()->scope_info();
    ScopeInfo<ZoneListAllocationPolicy> zone_scope_info(serialized_scope_info);
    int locals_number = zone_scope_info.NumberOfLocals();
    for (int i = 0; i < locals_number; ++i) {
      String* local_name = *zone_scope_info.LocalName(i);
      int idx = serialized_scope_info->ContextSlotIndex(local_name, NULL);
      if (idx >= 0 && idx < context->length()) {
        snapshot_->SetClosureReference(entry, local_name, context->get(idx));
      }
    }
    snapshot_->SetInternalReference(entry, "code", func->shared());
  }
}


void HeapSnapshotGenerator::ExtractPropertyReferences(JSObject* js_obj,
                                                      HeapEntry* entry) {
  if (js_obj->HasFastProperties()) {
    DescriptorArray* descs = js_obj->map()->instance_descriptors();
    for (int i = 0; i < descs->number_of_descriptors(); i++) {
      switch (descs->GetType(i)) {
        case FIELD: {
          int index = descs->GetFieldIndex(i);
          snapshot_->SetPropertyReference(
              entry, descs->GetKey(i), js_obj->FastPropertyAt(index));
          break;
        }
        case CONSTANT_FUNCTION:
          snapshot_->SetPropertyReference(
              entry, descs->GetKey(i), descs->GetConstantFunction(i));
          break;
        default: ;
      }
    }
  } else {
    StringDictionary* dictionary = js_obj->property_dictionary();
    int length = dictionary->Capacity();
    for (int i = 0; i < length; ++i) {
      Object* k = dictionary->KeyAt(i);
      if (dictionary->IsKey(k)) {
        snapshot_->SetPropertyReference(
            entry, String::cast(k), dictionary->ValueAt(i));
      }
    }
  }
}


void HeapSnapshotGenerator::ExtractElementReferences(JSObject* js_obj,
                                                     HeapEntry* entry) {
  if (js_obj->HasFastElements()) {
    FixedArray* elements = FixedArray::cast(js_obj->elements());
    int length = js_obj->IsJSArray() ?
        Smi::cast(JSArray::cast(js_obj)->length())->value() :
        elements->length();
    for (int i = 0; i < length; ++i) {
      if (!elements->get(i)->IsTheHole()) {
        snapshot_->SetElementReference(entry, i, elements->get(i));
      }
    }
  } else if (js_obj->HasDictionaryElements()) {
    NumberDictionary* dictionary = js_obj->element_dictionary();
    int length = dictionary->Capacity();
    for (int i = 0; i < length; ++i) {
      Object* k = dictionary->KeyAt(i);
      if (dictionary->IsKey(k)) {
        ASSERT(k->IsNumber());
        uint32_t index = static_cast<uint32_t>(k->Number());
        snapshot_->SetElementReference(entry, index, dictionary->ValueAt(i));
      }
    }
  }
}

} }  // namespace v8::internal

#endif  // ENABLE_LOGGING_AND_PROFILING
