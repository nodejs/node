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
#include "unicode.h"
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
  handle.Dispose();
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


static void DeleteIndexName(char** name_ptr) {
  DeleteArray(*name_ptr);
}


StringsStorage::~StringsStorage() {
  for (HashMap::Entry* p = names_.Start();
       p != NULL;
       p = names_.Next(p)) {
    DeleteArray(reinterpret_cast<const char*>(p->value));
  }
  index_names_.Iterate(DeleteIndexName);
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


const char* StringsStorage::GetName(int index) {
  ASSERT(index >= 0);
  if (index_names_.length() <= index) {
    index_names_.AddBlock(
        NULL, index - index_names_.length() + 1);
  }
  if (index_names_[index] == NULL) {
    const int kMaximumNameLength = 32;
    char* name = NewArray<char>(kMaximumNameLength);
    OS::SNPrintF(Vector<char>(name, kMaximumNameLength), "%d", index);
    index_names_[index] = name;
  }
  return index_names_[index];
}


const char* CodeEntry::kEmptyNamePrefix = "";


void CodeEntry::CopyData(const CodeEntry& source) {
  tag_ = source.tag_;
  name_prefix_ = source.name_prefix_;
  name_ = source.name_;
  resource_name_ = source.resource_name_;
  line_number_ = source.line_number_;
}


uint32_t CodeEntry::GetCallUid() const {
  uint32_t hash = ComputeIntegerHash(tag_);
  hash ^= ComputeIntegerHash(
      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(name_prefix_)));
  hash ^= ComputeIntegerHash(
      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(name_)));
  hash ^= ComputeIntegerHash(
      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(resource_name_)));
  hash ^= ComputeIntegerHash(line_number_);
  return hash;
}


bool CodeEntry::IsSameAs(CodeEntry* entry) const {
  return this == entry
      || (tag_ == entry->tag_
          && name_prefix_ == entry->name_prefix_
          && name_ == entry->name_
          && resource_name_ == entry->resource_name_
          && line_number_ == entry->line_number_);
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


class DeleteNodesCallback {
 public:
  void BeforeTraversingChild(ProfileNode*, ProfileNode*) { }

  void AfterAllChildrenTraversed(ProfileNode* node) {
    delete node;
  }

  void AfterChildTraversed(ProfileNode*, ProfileNode*) { }
};


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

void ProfileTree::FilteredClone(ProfileTree* src, int security_token_id) {
  ms_to_ticks_scale_ = src->ms_to_ticks_scale_;
  FilteredCloneCallback cb(root_, security_token_id);
  src->TraverseDepthFirst(&cb);
  CalculateTotalTicks();
}


void ProfileTree::SetTickRatePerMs(double ticks_per_ms) {
  ms_to_ticks_scale_ = ticks_per_ms > 0 ? 1.0 / ticks_per_ms : 1.0;
}


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
    if (tree_.Insert(start, &locator)) {
      entry->CopyData(*code_info.entry);
      locator.set_value(CodeEntryInfo(entry, code_info.size));
    }
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
}


bool CpuProfilesCollection::StartProfiling(const char* title, unsigned uid) {
  ASSERT(uid > 0);
  current_profiles_semaphore_->Wait();
  if (current_profiles_.length() >= kMaxSimultaneousProfiles) {
    current_profiles_semaphore_->Signal();
    return false;
  }
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
    (*list)[index] =
        unabridged_list->at(index)->FilteredClone(security_token_id);
  }
  return list->at(index);
}


bool CpuProfilesCollection::IsLastProfile(const char* title) {
  // Called from VM thread, and only it can mutate the list,
  // so no locking is needed here.
  if (current_profiles_.length() != 1) return false;
  return StrLength(title) == 0
      || strcmp(current_profiles_[0]->title(), title) == 0;
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
      (*list)[i] = unabridged_list->at(i)->FilteredClone(security_token_id);
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


void HeapGraphEdge::Init(
    int child_index, Type type, const char* name, HeapEntry* to) {
  ASSERT(type == kContextVariable
         || type == kProperty
         || type == kInternal
         || type == kShortcut);
  child_index_ = child_index;
  type_ = type;
  name_ = name;
  to_ = to;
}


void HeapGraphEdge::Init(int child_index, Type type, int index, HeapEntry* to) {
  ASSERT(type == kElement || type == kHidden);
  child_index_ = child_index;
  type_ = type;
  index_ = index;
  to_ = to;
}


void HeapGraphEdge::Init(int child_index, int index, HeapEntry* to) {
  Init(child_index, kElement, index, to);
}


HeapEntry* HeapGraphEdge::From() {
  return reinterpret_cast<HeapEntry*>(this - child_index_) - 1;
}


void HeapEntry::Init(HeapSnapshot* snapshot,
                     Type type,
                     const char* name,
                     uint64_t id,
                     int self_size,
                     int children_count,
                     int retainers_count) {
  snapshot_ = snapshot;
  type_ = type;
  painted_ = kUnpainted;
  name_ = name;
  self_size_ = self_size;
  retained_size_ = 0;
  children_count_ = children_count;
  retainers_count_ = retainers_count;
  dominator_ = NULL;

  union {
    uint64_t set_id;
    Id stored_id;
  } id_adaptor = {id};
  id_ = id_adaptor.stored_id;
}


void HeapEntry::SetNamedReference(HeapGraphEdge::Type type,
                                  int child_index,
                                  const char* name,
                                  HeapEntry* entry,
                                  int retainer_index) {
  children_arr()[child_index].Init(child_index, type, name, entry);
  entry->retainers_arr()[retainer_index] = children_arr() + child_index;
}


void HeapEntry::SetIndexedReference(HeapGraphEdge::Type type,
                                    int child_index,
                                    int index,
                                    HeapEntry* entry,
                                    int retainer_index) {
  children_arr()[child_index].Init(child_index, type, index, entry);
  entry->retainers_arr()[retainer_index] = children_arr() + child_index;
}


void HeapEntry::SetUnidirElementReference(
    int child_index, int index, HeapEntry* entry) {
  children_arr()[child_index].Init(child_index, index, entry);
}


int HeapEntry::RetainedSize(bool exact) {
  if (exact && (retained_size_ & kExactRetainedSizeTag) == 0) {
    CalculateExactRetainedSize();
  }
  return retained_size_ & (~kExactRetainedSizeTag);
}


List<HeapGraphPath*>* HeapEntry::GetRetainingPaths() {
  return snapshot_->GetRetainingPaths(this);
}


template<class Visitor>
void HeapEntry::ApplyAndPaintAllReachable(Visitor* visitor) {
  List<HeapEntry*> list(10);
  list.Add(this);
  this->paint_reachable();
  visitor->Apply(this);
  while (!list.is_empty()) {
    HeapEntry* entry = list.RemoveLast();
    Vector<HeapGraphEdge> children = entry->children();
    for (int i = 0; i < children.length(); ++i) {
      if (children[i].type() == HeapGraphEdge::kShortcut) continue;
      HeapEntry* child = children[i].to();
      if (!child->painted_reachable()) {
        list.Add(child);
        child->paint_reachable();
        visitor->Apply(child);
      }
    }
  }
}


class NullClass {
 public:
  void Apply(HeapEntry* entry) { }
};

void HeapEntry::PaintAllReachable() {
  NullClass null;
  ApplyAndPaintAllReachable(&null);
}


void HeapEntry::Print(int max_depth, int indent) {
  OS::Print("%6d %6d [%llu] ", self_size(), RetainedSize(false), id());
  if (type() != kString) {
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
  Vector<HeapGraphEdge> ch = children();
  for (int i = 0; i < ch.length(); ++i) {
    HeapGraphEdge& edge = ch[i];
    switch (edge.type()) {
      case HeapGraphEdge::kContextVariable:
        OS::Print("  %*c #%s: ", indent, ' ', edge.name());
        break;
      case HeapGraphEdge::kElement:
        OS::Print("  %*c %d: ", indent, ' ', edge.index());
        break;
      case HeapGraphEdge::kInternal:
        OS::Print("  %*c $%s: ", indent, ' ', edge.name());
        break;
      case HeapGraphEdge::kProperty:
        OS::Print("  %*c %s: ", indent, ' ', edge.name());
        break;
      case HeapGraphEdge::kHidden:
        OS::Print("  %*c $%d: ", indent, ' ', edge.index());
        break;
      case HeapGraphEdge::kShortcut:
        OS::Print("  %*c ^%s: ", indent, ' ', edge.name());
        break;
      default:
        OS::Print("!!! unknown edge type: %d ", edge.type());
    }
    edge.to()->Print(max_depth, indent + 2);
  }
}


const char* HeapEntry::TypeAsString() {
  switch (type()) {
    case kHidden: return "/hidden/";
    case kObject: return "/object/";
    case kClosure: return "/closure/";
    case kString: return "/string/";
    case kCode: return "/code/";
    case kArray: return "/array/";
    case kRegExp: return "/regexp/";
    case kHeapNumber: return "/number/";
    default: return "???";
  }
}


int HeapEntry::EntriesSize(int entries_count,
                           int children_count,
                           int retainers_count) {
  return sizeof(HeapEntry) * entries_count         // NOLINT
      + sizeof(HeapGraphEdge) * children_count     // NOLINT
      + sizeof(HeapGraphEdge*) * retainers_count;  // NOLINT
}


class RetainedSizeCalculator {
 public:
  RetainedSizeCalculator()
      : retained_size_(0) {
  }

  int reained_size() const { return retained_size_; }

  void Apply(HeapEntry** entry_ptr) {
    if ((*entry_ptr)->painted_reachable()) {
      retained_size_ += (*entry_ptr)->self_size();
    }
  }

 private:
  int retained_size_;
};

void HeapEntry::CalculateExactRetainedSize() {
  // To calculate retained size, first we paint all reachable nodes in
  // one color, then we paint (or re-paint) all nodes reachable from
  // other nodes with a different color. Then we sum up self sizes of
  // nodes painted with the first color.
  snapshot()->ClearPaint();
  PaintAllReachable();

  List<HeapEntry*> list(10);
  HeapEntry* root = snapshot()->root();
  if (this != root) {
    list.Add(root);
    root->paint_reachable_from_others();
  }
  while (!list.is_empty()) {
    HeapEntry* curr = list.RemoveLast();
    Vector<HeapGraphEdge> children = curr->children();
    for (int i = 0; i < children.length(); ++i) {
      if (children[i].type() == HeapGraphEdge::kShortcut) continue;
      HeapEntry* child = children[i].to();
      if (child != this && child->not_painted_reachable_from_others()) {
        list.Add(child);
        child->paint_reachable_from_others();
      }
    }
  }

  RetainedSizeCalculator ret_size_calc;
  snapshot()->IterateEntries(&ret_size_calc);
  retained_size_ = ret_size_calc.reained_size();
  ASSERT((retained_size_ & kExactRetainedSizeTag) == 0);
  retained_size_ |= kExactRetainedSizeTag;
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


List<HeapGraphPath*>* HeapEntry::CalculateRetainingPaths() {
  List<HeapGraphPath*>* retaining_paths = new List<HeapGraphPath*>(4);
  CachedHeapGraphPath path;
  FindRetainingPaths(&path, retaining_paths);
  return retaining_paths;
}


void HeapEntry::FindRetainingPaths(CachedHeapGraphPath* prev_path,
                                   List<HeapGraphPath*>* retaining_paths) {
  Vector<HeapGraphEdge*> rets = retainers();
  for (int i = 0; i < rets.length(); ++i) {
    HeapGraphEdge* ret_edge = rets[i];
    if (prev_path->ContainsNode(ret_edge->From())) continue;
    if (ret_edge->From() != snapshot()->root()) {
      CachedHeapGraphPath path(*prev_path);
      path.Add(ret_edge);
      ret_edge->From()->FindRetainingPaths(&path, retaining_paths);
    } else {
      HeapGraphPath* ret_path = new HeapGraphPath(*prev_path->path());
      ret_path->Set(0, ret_edge);
      retaining_paths->Add(ret_path);
    }
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
  path_[0]->From()->Print(1, 0);
  for (int i = 0; i < path_.length(); ++i) {
    OS::Print(" -> ");
    HeapGraphEdge* edge = path_[i];
    switch (edge->type()) {
      case HeapGraphEdge::kContextVariable:
        OS::Print("[#%s] ", edge->name());
        break;
      case HeapGraphEdge::kElement:
      case HeapGraphEdge::kHidden:
        OS::Print("[%d] ", edge->index());
        break;
      case HeapGraphEdge::kInternal:
        OS::Print("[$%s] ", edge->name());
        break;
      case HeapGraphEdge::kProperty:
        OS::Print("[%s] ", edge->name());
        break;
      case HeapGraphEdge::kShortcut:
        OS::Print("[^%s] ", edge->name());
        break;
      default:
        OS::Print("!!! unknown edge type: %d ", edge->type());
    }
    edge->to()->Print(1, 0);
  }
  OS::Print("\n");
}


HeapObject *const HeapSnapshot::kInternalRootObject =
    reinterpret_cast<HeapObject*>(1);
HeapObject *const HeapSnapshot::kGcRootsObject =
    reinterpret_cast<HeapObject*>(2);


// It is very important to keep objects that form a heap snapshot
// as small as possible.
namespace {  // Avoid littering the global namespace.

template <size_t ptr_size> struct SnapshotSizeConstants;

template <> struct SnapshotSizeConstants<4> {
  static const int kExpectedHeapGraphEdgeSize = 12;
  static const int kExpectedHeapEntrySize = 36;
};

template <> struct SnapshotSizeConstants<8> {
  static const int kExpectedHeapGraphEdgeSize = 24;
  static const int kExpectedHeapEntrySize = 48;
};

}  // namespace

HeapSnapshot::HeapSnapshot(HeapSnapshotsCollection* collection,
                           HeapSnapshot::Type type,
                           const char* title,
                           unsigned uid)
    : collection_(collection),
      type_(type),
      title_(title),
      uid_(uid),
      root_entry_(NULL),
      gc_roots_entry_(NULL),
      raw_entries_(NULL),
      entries_sorted_(false),
      retaining_paths_(HeapEntry::Match) {
  STATIC_ASSERT(
      sizeof(HeapGraphEdge) ==
      SnapshotSizeConstants<sizeof(void*)>::kExpectedHeapGraphEdgeSize);  // NOLINT
  STATIC_ASSERT(
      sizeof(HeapEntry) ==
      SnapshotSizeConstants<sizeof(void*)>::kExpectedHeapEntrySize);  // NOLINT
}


static void DeleteHeapGraphPath(HeapGraphPath** path_ptr) {
  delete *path_ptr;
}

HeapSnapshot::~HeapSnapshot() {
  DeleteArray(raw_entries_);
  for (HashMap::Entry* p = retaining_paths_.Start();
       p != NULL;
       p = retaining_paths_.Next(p)) {
    List<HeapGraphPath*>* list =
        reinterpret_cast<List<HeapGraphPath*>*>(p->value);
    list->Iterate(DeleteHeapGraphPath);
    delete list;
  }
}


void HeapSnapshot::AllocateEntries(int entries_count,
                                   int children_count,
                                   int retainers_count) {
  ASSERT(raw_entries_ == NULL);
  raw_entries_ = NewArray<char>(
      HeapEntry::EntriesSize(entries_count, children_count, retainers_count));
#ifdef DEBUG
  raw_entries_size_ =
      HeapEntry::EntriesSize(entries_count, children_count, retainers_count);
#endif
}


HeapEntry* HeapSnapshot::AddEntry(HeapObject* object,
                                  int children_count,
                                  int retainers_count) {
  if (object == kInternalRootObject) {
    ASSERT(root_entry_ == NULL);
    ASSERT(retainers_count == 0);
    return (root_entry_ = AddEntry(HeapEntry::kObject,
                                   "",
                                   HeapObjectsMap::kInternalRootObjectId,
                                   0,
                                   children_count,
                                   retainers_count));
  } else if (object == kGcRootsObject) {
    ASSERT(gc_roots_entry_ == NULL);
    return (gc_roots_entry_ = AddEntry(HeapEntry::kObject,
                                       "(GC roots)",
                                       HeapObjectsMap::kGcRootsObjectId,
                                       0,
                                       children_count,
                                       retainers_count));
  } else if (object->IsJSFunction()) {
    JSFunction* func = JSFunction::cast(object);
    SharedFunctionInfo* shared = func->shared();
    return AddEntry(object,
                    HeapEntry::kClosure,
                    collection_->GetName(String::cast(shared->name())),
                    children_count,
                    retainers_count);
  } else if (object->IsJSRegExp()) {
    JSRegExp* re = JSRegExp::cast(object);
    return AddEntry(object,
                    HeapEntry::kRegExp,
                    collection_->GetName(re->Pattern()),
                    children_count,
                    retainers_count);
  } else if (object->IsJSObject()) {
    return AddEntry(object,
                    HeapEntry::kObject,
                    collection_->GetName(GetConstructorNameForHeapProfile(
                        JSObject::cast(object))),
                    children_count,
                    retainers_count);
  } else if (object->IsString()) {
    return AddEntry(object,
                    HeapEntry::kString,
                    collection_->GetName(String::cast(object)),
                    children_count,
                    retainers_count);
  } else if (object->IsCode()) {
    return AddEntry(object,
                    HeapEntry::kCode,
                    "",
                    children_count,
                    retainers_count);
  } else if (object->IsSharedFunctionInfo()) {
    SharedFunctionInfo* shared = SharedFunctionInfo::cast(object);
    return AddEntry(object,
                    HeapEntry::kCode,
                    collection_->GetName(String::cast(shared->name())),
                    children_count,
                    retainers_count);
  } else if (object->IsScript()) {
    Script* script = Script::cast(object);
    return AddEntry(object,
                    HeapEntry::kCode,
                    script->name()->IsString() ?
                    collection_->GetName(String::cast(script->name())) : "",
                    children_count,
                    retainers_count);
  } else if (object->IsFixedArray()) {
    return AddEntry(object,
                    HeapEntry::kArray,
                    "",
                    children_count,
                    retainers_count);
  } else if (object->IsHeapNumber()) {
    return AddEntry(object,
                    HeapEntry::kHeapNumber,
                    "number",
                    children_count,
                    retainers_count);
  }
  return AddEntry(object,
                  HeapEntry::kHidden,
                  "system",
                  children_count,
                  retainers_count);
}


static void HeapEntryClearPaint(HeapEntry** entry_ptr) {
  (*entry_ptr)->clear_paint();
}

void HeapSnapshot::ClearPaint() {
  entries_.Iterate(HeapEntryClearPaint);
}


HeapEntry* HeapSnapshot::AddEntry(HeapObject* object,
                                  HeapEntry::Type type,
                                  const char* name,
                                  int children_count,
                                  int retainers_count) {
  return AddEntry(type,
                  name,
                  collection_->GetObjectId(object->address()),
                  object->Size(),
                  children_count,
                  retainers_count);
}


HeapEntry* HeapSnapshot::AddEntry(HeapEntry::Type type,
                                  const char* name,
                                  uint64_t id,
                                  int size,
                                  int children_count,
                                  int retainers_count) {
  HeapEntry* entry = GetNextEntryToInit();
  entry->Init(this, type, name, id, size, children_count, retainers_count);
  return entry;
}


void HeapSnapshot::SetDominatorsToSelf() {
  for (int i = 0; i < entries_.length(); ++i) {
    HeapEntry* entry = entries_[i];
    if (entry->dominator() == NULL) entry->set_dominator(entry);
  }
}


HeapEntry* HeapSnapshot::GetNextEntryToInit() {
  if (entries_.length() > 0) {
    HeapEntry* last_entry = entries_.last();
    entries_.Add(reinterpret_cast<HeapEntry*>(
        reinterpret_cast<char*>(last_entry) + last_entry->EntrySize()));
  } else {
    entries_.Add(reinterpret_cast<HeapEntry*>(raw_entries_));
  }
  ASSERT(reinterpret_cast<char*>(entries_.last()) <
         (raw_entries_ + raw_entries_size_));
  return entries_.last();
}


HeapSnapshotsDiff* HeapSnapshot::CompareWith(HeapSnapshot* snapshot) {
  return collection_->CompareSnapshots(this, snapshot);
}


HeapEntry* HeapSnapshot::GetEntryById(uint64_t id) {
  // GetSortedEntriesList is used in diff algorithm and sorts
  // entries by their id.
  List<HeapEntry*>* entries_by_id = GetSortedEntriesList();

  // Perform a binary search by id.
  int low = 0;
  int high = entries_by_id->length() - 1;
  while (low <= high) {
    int mid =
        (static_cast<unsigned int>(low) + static_cast<unsigned int>(high)) >> 1;
    uint64_t mid_id = entries_by_id->at(mid)->id();
    if (mid_id > id)
      high = mid - 1;
    else if (mid_id < id)
      low = mid + 1;
    else
      return entries_by_id->at(mid);
  }
  return NULL;
}


List<HeapGraphPath*>* HeapSnapshot::GetRetainingPaths(HeapEntry* entry) {
  HashMap::Entry* p =
      retaining_paths_.Lookup(entry, HeapEntry::Hash(entry), true);
  if (p->value == NULL) {
    p->value = entry->CalculateRetainingPaths();
  }
  return reinterpret_cast<List<HeapGraphPath*>*>(p->value);
}


template<class T>
static int SortByIds(const T* entry1_ptr,
                     const T* entry2_ptr) {
  if ((*entry1_ptr)->id() == (*entry2_ptr)->id()) return 0;
  return (*entry1_ptr)->id() < (*entry2_ptr)->id() ? -1 : 1;
}

List<HeapEntry*>* HeapSnapshot::GetSortedEntriesList() {
  if (!entries_sorted_) {
    entries_.Sort(SortByIds);
    entries_sorted_ = true;
  }
  return &entries_;
}


void HeapSnapshot::Print(int max_depth) {
  root()->Print(max_depth, 0);
}


const uint64_t HeapObjectsMap::kInternalRootObjectId = 0;
const uint64_t HeapObjectsMap::kGcRootsObjectId = 1;
// Increase kFirstAvailableObjectId if new 'special' objects appear.
const uint64_t HeapObjectsMap::kFirstAvailableObjectId = 2;

HeapObjectsMap::HeapObjectsMap()
    : initial_fill_mode_(true),
      next_id_(kFirstAvailableObjectId),
      entries_map_(AddressesMatch),
      entries_(new List<EntryInfo>()) { }


HeapObjectsMap::~HeapObjectsMap() {
  delete entries_;
}


void HeapObjectsMap::SnapshotGenerationFinished() {
    initial_fill_mode_ = false;
    RemoveDeadEntries();
}


uint64_t HeapObjectsMap::FindObject(Address addr) {
  if (!initial_fill_mode_) {
    uint64_t existing = FindEntry(addr);
    if (existing != 0) return existing;
  }
  uint64_t id = next_id_++;
  AddEntry(addr, id);
  return id;
}


void HeapObjectsMap::MoveObject(Address from, Address to) {
  if (from == to) return;
  HashMap::Entry* entry = entries_map_.Lookup(from, AddressHash(from), false);
  if (entry != NULL) {
    void* value = entry->value;
    entries_map_.Remove(from, AddressHash(from));
    entry = entries_map_.Lookup(to, AddressHash(to), true);
    // We can have an entry at the new location, it is OK, as GC can overwrite
    // dead objects with alive objects being moved.
    entry->value = value;
  }
}


void HeapObjectsMap::AddEntry(Address addr, uint64_t id) {
  HashMap::Entry* entry = entries_map_.Lookup(addr, AddressHash(addr), true);
  ASSERT(entry->value == NULL);
  entry->value = reinterpret_cast<void*>(entries_->length());
  entries_->Add(EntryInfo(id));
}


uint64_t HeapObjectsMap::FindEntry(Address addr) {
  HashMap::Entry* entry = entries_map_.Lookup(addr, AddressHash(addr), false);
  if (entry != NULL) {
    int entry_index =
        static_cast<int>(reinterpret_cast<intptr_t>(entry->value));
    EntryInfo& entry_info = entries_->at(entry_index);
    entry_info.accessed = true;
    return entry_info.id;
  } else {
    return 0;
  }
}


void HeapObjectsMap::RemoveDeadEntries() {
  List<EntryInfo>* new_entries = new List<EntryInfo>();
  List<void*> dead_entries;
  for (HashMap::Entry* entry = entries_map_.Start();
       entry != NULL;
       entry = entries_map_.Next(entry)) {
    int entry_index =
        static_cast<int>(reinterpret_cast<intptr_t>(entry->value));
    EntryInfo& entry_info = entries_->at(entry_index);
    if (entry_info.accessed) {
      entry->value = reinterpret_cast<void*>(new_entries->length());
      new_entries->Add(EntryInfo(entry_info.id, false));
    } else {
      dead_entries.Add(entry->key);
    }
  }
  for (int i = 0; i < dead_entries.length(); ++i) {
    void* raw_entry = dead_entries[i];
    entries_map_.Remove(
        raw_entry, AddressHash(reinterpret_cast<Address>(raw_entry)));
  }
  delete entries_;
  entries_ = new_entries;
}


HeapSnapshotsCollection::HeapSnapshotsCollection()
    : is_tracking_objects_(false),
      snapshots_uids_(HeapSnapshotsMatch),
      token_enumerator_(new TokenEnumerator()) {
}


static void DeleteHeapSnapshot(HeapSnapshot** snapshot_ptr) {
  delete *snapshot_ptr;
}


HeapSnapshotsCollection::~HeapSnapshotsCollection() {
  delete token_enumerator_;
  snapshots_.Iterate(DeleteHeapSnapshot);
}


HeapSnapshot* HeapSnapshotsCollection::NewSnapshot(HeapSnapshot::Type type,
                                                   const char* name,
                                                   unsigned uid) {
  is_tracking_objects_ = true;  // Start watching for heap objects moves.
  return new HeapSnapshot(this, type, name, uid);
}


void HeapSnapshotsCollection::SnapshotGenerationFinished(
    HeapSnapshot* snapshot) {
  ids_.SnapshotGenerationFinished();
  if (snapshot != NULL) {
    snapshots_.Add(snapshot);
    HashMap::Entry* entry =
        snapshots_uids_.Lookup(reinterpret_cast<void*>(snapshot->uid()),
                               static_cast<uint32_t>(snapshot->uid()),
                               true);
    ASSERT(entry->value == NULL);
    entry->value = snapshot;
  }
}


HeapSnapshot* HeapSnapshotsCollection::GetSnapshot(unsigned uid) {
  HashMap::Entry* entry = snapshots_uids_.Lookup(reinterpret_cast<void*>(uid),
                                                 static_cast<uint32_t>(uid),
                                                 false);
  return entry != NULL ? reinterpret_cast<HeapSnapshot*>(entry->value) : NULL;
}


HeapSnapshotsDiff* HeapSnapshotsCollection::CompareSnapshots(
    HeapSnapshot* snapshot1,
    HeapSnapshot* snapshot2) {
  return comparator_.Compare(snapshot1, snapshot2);
}


HeapEntry *const HeapEntriesMap::kHeapEntryPlaceholder =
    reinterpret_cast<HeapEntry*>(1);

HeapEntriesMap::HeapEntriesMap()
    : entries_(HeapObjectsMatch),
      entries_count_(0),
      total_children_count_(0),
      total_retainers_count_(0) {
}


HeapEntriesMap::~HeapEntriesMap() {
  for (HashMap::Entry* p = entries_.Start(); p != NULL; p = entries_.Next(p)) {
    delete reinterpret_cast<EntryInfo*>(p->value);
  }
}


HeapEntry* HeapEntriesMap::Map(HeapObject* object) {
  HashMap::Entry* cache_entry = entries_.Lookup(object, Hash(object), false);
  if (cache_entry != NULL) {
    EntryInfo* entry_info = reinterpret_cast<EntryInfo*>(cache_entry->value);
    return entry_info->entry;
  } else {
    return NULL;
  }
}


void HeapEntriesMap::Pair(HeapObject* object, HeapEntry* entry) {
  HashMap::Entry* cache_entry = entries_.Lookup(object, Hash(object), true);
  ASSERT(cache_entry->value == NULL);
  cache_entry->value = new EntryInfo(entry);
  ++entries_count_;
}


void HeapEntriesMap::CountReference(HeapObject* from, HeapObject* to,
                                    int* prev_children_count,
                                    int* prev_retainers_count) {
  HashMap::Entry* from_cache_entry = entries_.Lookup(from, Hash(from), false);
  HashMap::Entry* to_cache_entry = entries_.Lookup(to, Hash(to), false);
  ASSERT(from_cache_entry != NULL);
  ASSERT(to_cache_entry != NULL);
  EntryInfo* from_entry_info =
      reinterpret_cast<EntryInfo*>(from_cache_entry->value);
  EntryInfo* to_entry_info =
      reinterpret_cast<EntryInfo*>(to_cache_entry->value);
  if (prev_children_count)
    *prev_children_count = from_entry_info->children_count;
  if (prev_retainers_count)
    *prev_retainers_count = to_entry_info->retainers_count;
  ++from_entry_info->children_count;
  ++to_entry_info->retainers_count;
  ++total_children_count_;
  ++total_retainers_count_;
}


HeapObjectsSet::HeapObjectsSet()
    : entries_(HeapEntriesMap::HeapObjectsMatch) {
}


void HeapObjectsSet::Clear() {
  entries_.Clear();
}


bool HeapObjectsSet::Contains(Object* obj) {
  if (!obj->IsHeapObject()) return false;
  HeapObject* object = HeapObject::cast(obj);
  HashMap::Entry* cache_entry =
      entries_.Lookup(object, HeapEntriesMap::Hash(object), false);
  return cache_entry != NULL;
}


void HeapObjectsSet::Insert(Object* obj) {
  if (!obj->IsHeapObject()) return;
  HeapObject* object = HeapObject::cast(obj);
  HashMap::Entry* cache_entry =
      entries_.Lookup(object, HeapEntriesMap::Hash(object), true);
  if (cache_entry->value == NULL) {
    cache_entry->value = HeapEntriesMap::kHeapEntryPlaceholder;
  }
}


HeapSnapshotGenerator::HeapSnapshotGenerator(HeapSnapshot* snapshot,
                                             v8::ActivityControl* control)
    : snapshot_(snapshot),
      control_(control),
      collection_(snapshot->collection()),
      filler_(NULL) {
}

class SnapshotCounter : public HeapSnapshotGenerator::SnapshotFillerInterface {
 public:
  explicit SnapshotCounter(HeapEntriesMap* entries)
      : entries_(entries) { }
  HeapEntry* AddEntry(HeapObject* obj) {
    entries_->Pair(obj, HeapEntriesMap::kHeapEntryPlaceholder);
    return HeapEntriesMap::kHeapEntryPlaceholder;
  }
  void SetIndexedReference(HeapGraphEdge::Type,
                           HeapObject* parent_obj,
                           HeapEntry*,
                           int,
                           Object* child_obj,
                           HeapEntry*) {
    entries_->CountReference(parent_obj, HeapObject::cast(child_obj));
  }
  void SetNamedReference(HeapGraphEdge::Type,
                         HeapObject* parent_obj,
                         HeapEntry*,
                         const char*,
                         Object* child_obj,
                         HeapEntry*) {
    entries_->CountReference(parent_obj, HeapObject::cast(child_obj));
  }
  void SetRootShortcutReference(Object* child_obj, HeapEntry*) {
    entries_->CountReference(
        HeapSnapshot::kInternalRootObject, HeapObject::cast(child_obj));
  }
  void SetRootGcRootsReference() {
    entries_->CountReference(
        HeapSnapshot::kInternalRootObject, HeapSnapshot::kGcRootsObject);
  }
  void SetStrongRootReference(Object* child_obj, HeapEntry*) {
    entries_->CountReference(
        HeapSnapshot::kGcRootsObject, HeapObject::cast(child_obj));
  }
 private:
  HeapEntriesMap* entries_;
};


class SnapshotFiller : public HeapSnapshotGenerator::SnapshotFillerInterface {
 public:
  explicit SnapshotFiller(HeapSnapshot* snapshot, HeapEntriesMap* entries)
      : snapshot_(snapshot),
        collection_(snapshot->collection()),
        entries_(entries) { }
  HeapEntry* AddEntry(HeapObject* obj) {
    UNREACHABLE();
    return NULL;
  }
  void SetIndexedReference(HeapGraphEdge::Type type,
                           HeapObject* parent_obj,
                           HeapEntry* parent_entry,
                           int index,
                           Object* child_obj,
                           HeapEntry* child_entry) {
    int child_index, retainer_index;
    entries_->CountReference(parent_obj,
                             HeapObject::cast(child_obj),
                             &child_index,
                             &retainer_index);
    parent_entry->SetIndexedReference(
        type, child_index, index, child_entry, retainer_index);
  }
  void SetNamedReference(HeapGraphEdge::Type type,
                         HeapObject* parent_obj,
                         HeapEntry* parent_entry,
                         const char* reference_name,
                         Object* child_obj,
                         HeapEntry* child_entry) {
    int child_index, retainer_index;
    entries_->CountReference(parent_obj, HeapObject::cast(child_obj),
                             &child_index, &retainer_index);
    parent_entry->SetNamedReference(type,
                              child_index,
                              reference_name,
                              child_entry,
                              retainer_index);
  }
  void SetRootGcRootsReference() {
    int child_index, retainer_index;
    entries_->CountReference(HeapSnapshot::kInternalRootObject,
                             HeapSnapshot::kGcRootsObject,
                             &child_index,
                             &retainer_index);
    snapshot_->root()->SetIndexedReference(HeapGraphEdge::kElement,
                                           child_index,
                                           child_index + 1,
                                           snapshot_->gc_roots(),
                                           retainer_index);
  }
  void SetRootShortcutReference(Object* child_obj,
                                HeapEntry* child_entry) {
    int child_index, retainer_index;
    entries_->CountReference(HeapSnapshot::kInternalRootObject,
                             HeapObject::cast(child_obj),
                             &child_index,
                             &retainer_index);
    snapshot_->root()->SetNamedReference(HeapGraphEdge::kShortcut,
                                         child_index,
                                         collection_->GetName(child_index + 1),
                                         child_entry,
                                         retainer_index);
  }
  void SetStrongRootReference(Object* child_obj,
                              HeapEntry* child_entry) {
    int child_index, retainer_index;
    entries_->CountReference(HeapSnapshot::kGcRootsObject,
                             HeapObject::cast(child_obj),
                             &child_index,
                             &retainer_index);
    snapshot_->gc_roots()->SetIndexedReference(HeapGraphEdge::kElement,
                                               child_index,
                                               child_index + 1,
                                               child_entry,
                                               retainer_index);
  }
 private:
  HeapSnapshot* snapshot_;
  HeapSnapshotsCollection* collection_;
  HeapEntriesMap* entries_;
};

class SnapshotAllocator {
 public:
  explicit SnapshotAllocator(HeapSnapshot* snapshot)
      : snapshot_(snapshot) { }
  HeapEntry* GetEntry(
      HeapObject* obj, int children_count, int retainers_count) {
    HeapEntry* entry =
        snapshot_->AddEntry(obj, children_count, retainers_count);
    ASSERT(entry != NULL);
    return entry;
  }
 private:
  HeapSnapshot* snapshot_;
};

class RootsReferencesExtractor : public ObjectVisitor {
 public:
  explicit RootsReferencesExtractor(HeapSnapshotGenerator* generator)
      : generator_(generator) {
  }
  void VisitPointers(Object** start, Object** end) {
    for (Object** p = start; p < end; p++) generator_->SetGcRootsReference(*p);
  }
 private:
  HeapSnapshotGenerator* generator_;
};


bool HeapSnapshotGenerator::GenerateSnapshot() {
  AssertNoAllocation no_alloc;

  SetProgressTotal(4);  // 2 passes + dominators + sizes.

  // Pass 1. Iterate heap contents to count entries and references.
  if (!CountEntriesAndReferences()) return false;

  // Allocate and fill entries in the snapshot, allocate references.
  snapshot_->AllocateEntries(entries_.entries_count(),
                             entries_.total_children_count(),
                             entries_.total_retainers_count());
  SnapshotAllocator allocator(snapshot_);
  entries_.UpdateEntries(&allocator);

  // Pass 2. Fill references.
  if (!FillReferences()) return false;

  if (!SetEntriesDominators()) return false;
  if (!ApproximateRetainedSizes()) return false;

  progress_counter_ = progress_total_;
  if (!ReportProgress(true)) return false;
  return true;
}


HeapEntry* HeapSnapshotGenerator::GetEntry(Object* obj) {
  if (!obj->IsHeapObject()) return NULL;
  HeapObject* object = HeapObject::cast(obj);
  HeapEntry* entry = entries_.Map(object);
  // A new entry.
  if (entry == NULL) entry = filler_->AddEntry(object);
  return entry;
}


class IndexedReferencesExtractor : public ObjectVisitor {
 public:
  IndexedReferencesExtractor(HeapSnapshotGenerator* generator,
                             HeapObject* parent_obj,
                             HeapEntry* parent_entry,
                             HeapObjectsSet* known_references = NULL)
      : generator_(generator),
        parent_obj_(parent_obj),
        parent_(parent_entry),
        known_references_(known_references),
        next_index_(1) {
  }
  void VisitPointers(Object** start, Object** end) {
    for (Object** p = start; p < end; p++) {
      if (!known_references_ || !known_references_->Contains(*p)) {
        generator_->SetHiddenReference(parent_obj_, parent_, next_index_++, *p);
      }
    }
  }
 private:
  HeapSnapshotGenerator* generator_;
  HeapObject* parent_obj_;
  HeapEntry* parent_;
  HeapObjectsSet* known_references_;
  int next_index_;
};


void HeapSnapshotGenerator::ExtractReferences(HeapObject* obj) {
  HeapEntry* entry = GetEntry(obj);
  if (entry == NULL) return;  // No interest in this object.

  known_references_.Clear();
  if (obj->IsJSGlobalProxy()) {
    // We need to reference JS global objects from snapshot's root.
    // We use JSGlobalProxy because this is what embedder (e.g. browser)
    // uses for the global object.
    JSGlobalProxy* proxy = JSGlobalProxy::cast(obj);
    SetRootShortcutReference(proxy->map()->prototype());
    IndexedReferencesExtractor refs_extractor(this, obj, entry);
    obj->Iterate(&refs_extractor);
  } else if (obj->IsJSObject()) {
    JSObject* js_obj = JSObject::cast(obj);
    ExtractClosureReferences(js_obj, entry);
    ExtractPropertyReferences(js_obj, entry);
    ExtractElementReferences(js_obj, entry);
    ExtractInternalReferences(js_obj, entry);
    SetPropertyReference(
        obj, entry, Heap::Proto_symbol(), js_obj->GetPrototype());
    if (obj->IsJSFunction()) {
      JSFunction* js_fun = JSFunction::cast(obj);
      if (js_fun->has_prototype()) {
        SetPropertyReference(
            obj, entry, Heap::prototype_symbol(), js_fun->prototype());
      }
    }
    IndexedReferencesExtractor refs_extractor(
        this, obj, entry, &known_references_);
    obj->Iterate(&refs_extractor);
  } else if (obj->IsString()) {
    if (obj->IsConsString()) {
      ConsString* cs = ConsString::cast(obj);
      SetInternalReference(obj, entry, 1, cs->first());
      SetInternalReference(obj, entry, 2, cs->second());
    }
  } else {
    IndexedReferencesExtractor refs_extractor(this, obj, entry);
    obj->Iterate(&refs_extractor);
  }
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
        SetClosureReference(js_obj, entry, local_name, context->get(idx));
      }
    }
    SetInternalReference(js_obj, entry, "code", func->shared());
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
          SetPropertyReference(
              js_obj, entry, descs->GetKey(i), js_obj->FastPropertyAt(index));
          break;
        }
        case CONSTANT_FUNCTION:
          SetPropertyReference(
              js_obj, entry, descs->GetKey(i), descs->GetConstantFunction(i));
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
        Object* target = dictionary->ValueAt(i);
        SetPropertyReference(
            js_obj, entry, String::cast(k), target);
        // We assume that global objects can only have slow properties.
        if (target->IsJSGlobalPropertyCell()) {
          SetPropertyShortcutReference(js_obj,
                                       entry,
                                       String::cast(k),
                                       JSGlobalPropertyCell::cast(
                                           target)->value());
        }
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
        SetElementReference(js_obj, entry, i, elements->get(i));
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
        SetElementReference(js_obj, entry, index, dictionary->ValueAt(i));
      }
    }
  }
}


void HeapSnapshotGenerator::ExtractInternalReferences(JSObject* js_obj,
                                                      HeapEntry* entry) {
  int length = js_obj->GetInternalFieldCount();
  for (int i = 0; i < length; ++i) {
    Object* o = js_obj->GetInternalField(i);
    SetInternalReference(js_obj, entry, i, o);
  }
}


void HeapSnapshotGenerator::SetClosureReference(HeapObject* parent_obj,
                                                HeapEntry* parent_entry,
                                                String* reference_name,
                                                Object* child_obj) {
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry != NULL) {
    filler_->SetNamedReference(HeapGraphEdge::kContextVariable,
                               parent_obj,
                               parent_entry,
                               collection_->GetName(reference_name),
                               child_obj,
                               child_entry);
    known_references_.Insert(child_obj);
  }
}


void HeapSnapshotGenerator::SetElementReference(HeapObject* parent_obj,
                                                HeapEntry* parent_entry,
                                                int index,
                                                Object* child_obj) {
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry != NULL) {
    filler_->SetIndexedReference(HeapGraphEdge::kElement,
                                 parent_obj,
                                 parent_entry,
                                 index,
                                 child_obj,
                                 child_entry);
    known_references_.Insert(child_obj);
  }
}


void HeapSnapshotGenerator::SetInternalReference(HeapObject* parent_obj,
                                                 HeapEntry* parent_entry,
                                                 const char* reference_name,
                                                 Object* child_obj) {
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry != NULL) {
    filler_->SetNamedReference(HeapGraphEdge::kInternal,
                               parent_obj,
                               parent_entry,
                               reference_name,
                               child_obj,
                               child_entry);
    known_references_.Insert(child_obj);
  }
}


void HeapSnapshotGenerator::SetInternalReference(HeapObject* parent_obj,
                                                 HeapEntry* parent_entry,
                                                 int index,
                                                 Object* child_obj) {
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry != NULL) {
    filler_->SetNamedReference(HeapGraphEdge::kInternal,
                               parent_obj,
                               parent_entry,
                               collection_->GetName(index),
                               child_obj,
                               child_entry);
    known_references_.Insert(child_obj);
  }
}


void HeapSnapshotGenerator::SetHiddenReference(HeapObject* parent_obj,
                                               HeapEntry* parent_entry,
                                               int index,
                                               Object* child_obj) {
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry != NULL) {
    filler_->SetIndexedReference(HeapGraphEdge::kHidden,
                                 parent_obj,
                                 parent_entry,
                                 index,
                                 child_obj,
                                 child_entry);
  }
}


void HeapSnapshotGenerator::SetPropertyReference(HeapObject* parent_obj,
                                                 HeapEntry* parent_entry,
                                                 String* reference_name,
                                                 Object* child_obj) {
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry != NULL) {
    HeapGraphEdge::Type type = reference_name->length() > 0 ?
        HeapGraphEdge::kProperty : HeapGraphEdge::kInternal;
    filler_->SetNamedReference(type,
                               parent_obj,
                               parent_entry,
                               collection_->GetName(reference_name),
                               child_obj,
                               child_entry);
    known_references_.Insert(child_obj);
  }
}


void HeapSnapshotGenerator::SetPropertyShortcutReference(
    HeapObject* parent_obj,
    HeapEntry* parent_entry,
    String* reference_name,
    Object* child_obj) {
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry != NULL) {
    filler_->SetNamedReference(HeapGraphEdge::kShortcut,
                               parent_obj,
                               parent_entry,
                               collection_->GetName(reference_name),
                               child_obj,
                               child_entry);
  }
}


void HeapSnapshotGenerator::SetRootGcRootsReference() {
  filler_->SetRootGcRootsReference();
}


void HeapSnapshotGenerator::SetRootShortcutReference(Object* child_obj) {
  HeapEntry* child_entry = GetEntry(child_obj);
  ASSERT(child_entry != NULL);
  filler_->SetRootShortcutReference(child_obj, child_entry);
}


void HeapSnapshotGenerator::SetGcRootsReference(Object* child_obj) {
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry != NULL) {
    filler_->SetStrongRootReference(child_obj, child_entry);
  }
}


void HeapSnapshotGenerator::SetProgressTotal(int iterations_count) {
  if (control_ == NULL) return;

  HeapIterator iterator(HeapIterator::kFilterUnreachable);
  int objects_count = 0;
  for (HeapObject* obj = iterator.next();
       obj != NULL;
       obj = iterator.next(), ++objects_count) {}
  progress_total_ = objects_count * iterations_count;
  progress_counter_ = 0;
}


bool HeapSnapshotGenerator::CountEntriesAndReferences() {
  SnapshotCounter counter(&entries_);
  filler_ = &counter;
  filler_->AddEntry(HeapSnapshot::kInternalRootObject);
  filler_->AddEntry(HeapSnapshot::kGcRootsObject);
  return IterateAndExtractReferences();
}


bool HeapSnapshotGenerator::FillReferences() {
  SnapshotFiller filler(snapshot_, &entries_);
  filler_ = &filler;
  return IterateAndExtractReferences();
}


void HeapSnapshotGenerator::FillReversePostorderIndexes(
    Vector<HeapEntry*>* entries) {
  snapshot_->ClearPaint();
  int current_entry = 0;
  List<HeapEntry*> nodes_to_visit;
  nodes_to_visit.Add(snapshot_->root());
  snapshot_->root()->paint_reachable();
  while (!nodes_to_visit.is_empty()) {
    HeapEntry* entry = nodes_to_visit.last();
    Vector<HeapGraphEdge> children = entry->children();
    bool has_new_edges = false;
    for (int i = 0; i < children.length(); ++i) {
      if (children[i].type() == HeapGraphEdge::kShortcut) continue;
      HeapEntry* child = children[i].to();
      if (!child->painted_reachable()) {
        nodes_to_visit.Add(child);
        child->paint_reachable();
        has_new_edges = true;
      }
    }
    if (!has_new_edges) {
      entry->set_ordered_index(current_entry);
      (*entries)[current_entry++] = entry;
      nodes_to_visit.RemoveLast();
    }
  }
  entries->Truncate(current_entry);
}


static int Intersect(int i1, int i2, const Vector<HeapEntry*>& dominators) {
  int finger1 = i1, finger2 = i2;
  while (finger1 != finger2) {
    while (finger1 < finger2) finger1 = dominators[finger1]->ordered_index();
    while (finger2 < finger1) finger2 = dominators[finger2]->ordered_index();
  }
  return finger1;
}

// The algorithm is based on the article:
// K. Cooper, T. Harvey and K. Kennedy "A Simple, Fast Dominance Algorithm"
// Softw. Pract. Exper. 4 (2001), pp. 1–10.
bool HeapSnapshotGenerator::BuildDominatorTree(
    const Vector<HeapEntry*>& entries,
    Vector<HeapEntry*>* dominators) {
  if (entries.length() == 0) return true;
  const int entries_length = entries.length(), root_index = entries_length - 1;
  for (int i = 0; i < root_index; ++i) (*dominators)[i] = NULL;
  (*dominators)[root_index] = entries[root_index];
  int changed = 1;
  const int base_progress_counter = progress_counter_;
  while (changed != 0) {
    changed = 0;
    for (int i = root_index - 1; i >= 0; --i) {
      HeapEntry* new_idom = NULL;
      Vector<HeapGraphEdge*> rets = entries[i]->retainers();
      int j = 0;
      for (; j < rets.length(); ++j) {
        if (rets[j]->type() == HeapGraphEdge::kShortcut) continue;
        HeapEntry* ret = rets[j]->From();
        if (dominators->at(ret->ordered_index()) != NULL) {
          new_idom = ret;
          break;
        }
      }
      for (++j; j < rets.length(); ++j) {
        if (rets[j]->type() == HeapGraphEdge::kShortcut) continue;
        HeapEntry* ret = rets[j]->From();
        if (dominators->at(ret->ordered_index()) != NULL) {
          new_idom = entries[Intersect(ret->ordered_index(),
                                       new_idom->ordered_index(),
                                       *dominators)];
        }
      }
      if (new_idom != NULL && dominators->at(i) != new_idom) {
        (*dominators)[i] = new_idom;
        ++changed;
      }
    }
    int remaining = entries_length - changed;
    if (remaining < 0) remaining = 0;
    progress_counter_ = base_progress_counter + remaining;
    if (!ReportProgress(true)) return false;
  }
  return true;
}


bool HeapSnapshotGenerator::SetEntriesDominators() {
  // This array is used for maintaining reverse postorder of nodes.
  ScopedVector<HeapEntry*> ordered_entries(snapshot_->entries()->length());
  FillReversePostorderIndexes(&ordered_entries);
  ScopedVector<HeapEntry*> dominators(ordered_entries.length());
  if (!BuildDominatorTree(ordered_entries, &dominators)) return false;
  for (int i = 0; i < ordered_entries.length(); ++i) {
    ASSERT(dominators[i] != NULL);
    ordered_entries[i]->set_dominator(dominators[i]);
  }
  return true;
}


bool HeapSnapshotGenerator::ApproximateRetainedSizes() {
  // As for the dominators tree we only know parent nodes, not
  // children, to sum up total sizes we "bubble" node's self size
  // adding it to all of its parents.
  for (int i = 0; i < snapshot_->entries()->length(); ++i) {
    HeapEntry* entry = snapshot_->entries()->at(i);
    entry->set_retained_size(entry->self_size());
  }
  for (int i = 0;
       i < snapshot_->entries()->length();
       ++i, IncProgressCounter()) {
    HeapEntry* entry = snapshot_->entries()->at(i);
    int entry_size = entry->self_size();
    for (HeapEntry* dominator = entry->dominator();
         dominator != entry;
         entry = dominator, dominator = entry->dominator()) {
      dominator->add_retained_size(entry_size);
    }
    if (!ReportProgress()) return false;
  }
  return true;
}


bool HeapSnapshotGenerator::IterateAndExtractReferences() {
  HeapIterator iterator(HeapIterator::kFilterUnreachable);
  bool interrupted = false;
  // Heap iteration with filtering must be finished in any case.
  for (HeapObject* obj = iterator.next();
       obj != NULL;
       obj = iterator.next(), IncProgressCounter()) {
    if (!interrupted) {
      ExtractReferences(obj);
      if (!ReportProgress()) interrupted = true;
    }
  }
  if (interrupted) return false;
  SetRootGcRootsReference();
  RootsReferencesExtractor extractor(this);
  Heap::IterateRoots(&extractor, VISIT_ALL);
  return ReportProgress();
}


void HeapSnapshotsDiff::CreateRoots(int additions_count, int deletions_count) {
  raw_additions_root_ =
      NewArray<char>(HeapEntry::EntriesSize(1, additions_count, 0));
  additions_root()->Init(
      snapshot2_, HeapEntry::kHidden, "", 0, 0, additions_count, 0);
  raw_deletions_root_ =
      NewArray<char>(HeapEntry::EntriesSize(1, deletions_count, 0));
  deletions_root()->Init(
      snapshot1_, HeapEntry::kHidden, "", 0, 0, deletions_count, 0);
}


static void DeleteHeapSnapshotsDiff(HeapSnapshotsDiff** diff_ptr) {
  delete *diff_ptr;
}

HeapSnapshotsComparator::~HeapSnapshotsComparator() {
  diffs_.Iterate(DeleteHeapSnapshotsDiff);
}


HeapSnapshotsDiff* HeapSnapshotsComparator::Compare(HeapSnapshot* snapshot1,
                                                    HeapSnapshot* snapshot2) {
  snapshot1->ClearPaint();
  snapshot1->root()->PaintAllReachable();
  snapshot2->ClearPaint();
  snapshot2->root()->PaintAllReachable();

  List<HeapEntry*>* entries1 = snapshot1->GetSortedEntriesList();
  List<HeapEntry*>* entries2 = snapshot2->GetSortedEntriesList();
  int i = 0, j = 0;
  List<HeapEntry*> added_entries, deleted_entries;
  while (i < entries1->length() && j < entries2->length()) {
    uint64_t id1 = entries1->at(i)->id();
    uint64_t id2 = entries2->at(j)->id();
    if (id1 == id2) {
      HeapEntry* entry1 = entries1->at(i++);
      HeapEntry* entry2 = entries2->at(j++);
      if (entry1->painted_reachable() != entry2->painted_reachable()) {
        if (entry1->painted_reachable())
          deleted_entries.Add(entry1);
        else
          added_entries.Add(entry2);
      }
    } else if (id1 < id2) {
      HeapEntry* entry = entries1->at(i++);
      deleted_entries.Add(entry);
    } else {
      HeapEntry* entry = entries2->at(j++);
      added_entries.Add(entry);
    }
  }
  while (i < entries1->length()) {
    HeapEntry* entry = entries1->at(i++);
    deleted_entries.Add(entry);
  }
  while (j < entries2->length()) {
    HeapEntry* entry = entries2->at(j++);
    added_entries.Add(entry);
  }

  HeapSnapshotsDiff* diff = new HeapSnapshotsDiff(snapshot1, snapshot2);
  diffs_.Add(diff);
  diff->CreateRoots(added_entries.length(), deleted_entries.length());

  for (int i = 0; i < deleted_entries.length(); ++i) {
    HeapEntry* entry = deleted_entries[i];
    diff->AddDeletedEntry(i, i + 1, entry);
  }
  for (int i = 0; i < added_entries.length(); ++i) {
    HeapEntry* entry = added_entries[i];
    diff->AddAddedEntry(i, i + 1, entry);
  }
  return diff;
}


class OutputStreamWriter {
 public:
  explicit OutputStreamWriter(v8::OutputStream* stream)
      : stream_(stream),
        chunk_size_(stream->GetChunkSize()),
        chunk_(chunk_size_),
        chunk_pos_(0),
        aborted_(false) {
    ASSERT(chunk_size_ > 0);
  }
  bool aborted() { return aborted_; }
  void AddCharacter(char c) {
    ASSERT(c != '\0');
    ASSERT(chunk_pos_ < chunk_size_);
    chunk_[chunk_pos_++] = c;
    MaybeWriteChunk();
  }
  void AddString(const char* s) {
    AddSubstring(s, StrLength(s));
  }
  void AddSubstring(const char* s, int n) {
    if (n <= 0) return;
    ASSERT(static_cast<size_t>(n) <= strlen(s));
    const char* s_end = s + n;
    while (s < s_end) {
      int s_chunk_size = Min(
          chunk_size_ - chunk_pos_, static_cast<int>(s_end - s));
      ASSERT(s_chunk_size > 0);
      memcpy(chunk_.start() + chunk_pos_, s, s_chunk_size);
      s += s_chunk_size;
      chunk_pos_ += s_chunk_size;
      MaybeWriteChunk();
    }
  }
  void AddNumber(int n) { AddNumberImpl<int>(n, "%d"); }
  void AddNumber(unsigned n) { AddNumberImpl<unsigned>(n, "%u"); }
  void AddNumber(uint64_t n) { AddNumberImpl<uint64_t>(n, "%llu"); }
  void Finalize() {
    if (aborted_) return;
    ASSERT(chunk_pos_ < chunk_size_);
    if (chunk_pos_ != 0) {
      WriteChunk();
    }
    stream_->EndOfStream();
  }

 private:
  template<typename T>
  void AddNumberImpl(T n, const char* format) {
    ScopedVector<char> buffer(32);
    int result = OS::SNPrintF(buffer, format, n);
    USE(result);
    ASSERT(result != -1);
    AddString(buffer.start());
  }
  void MaybeWriteChunk() {
    ASSERT(chunk_pos_ <= chunk_size_);
    if (chunk_pos_ == chunk_size_) {
      WriteChunk();
      chunk_pos_ = 0;
    }
  }
  void WriteChunk() {
    if (aborted_) return;
    if (stream_->WriteAsciiChunk(chunk_.start(), chunk_pos_) ==
        v8::OutputStream::kAbort) aborted_ = true;
  }

  v8::OutputStream* stream_;
  int chunk_size_;
  ScopedVector<char> chunk_;
  int chunk_pos_;
  bool aborted_;
};

void HeapSnapshotJSONSerializer::Serialize(v8::OutputStream* stream) {
  ASSERT(writer_ == NULL);
  writer_ = new OutputStreamWriter(stream);

  // Since nodes graph is cyclic, we need the first pass to enumerate
  // them. Strings can be serialized in one pass.
  EnumerateNodes();
  SerializeImpl();

  delete writer_;
  writer_ = NULL;
}


void HeapSnapshotJSONSerializer::SerializeImpl() {
  writer_->AddCharacter('{');
  writer_->AddString("\"snapshot\":{");
  SerializeSnapshot();
  if (writer_->aborted()) return;
  writer_->AddString("},\n");
  writer_->AddString("\"nodes\":[");
  SerializeNodes();
  if (writer_->aborted()) return;
  writer_->AddString("],\n");
  writer_->AddString("\"strings\":[");
  SerializeStrings();
  if (writer_->aborted()) return;
  writer_->AddCharacter(']');
  writer_->AddCharacter('}');
  writer_->Finalize();
}


class HeapSnapshotJSONSerializerEnumerator {
 public:
  explicit HeapSnapshotJSONSerializerEnumerator(HeapSnapshotJSONSerializer* s)
      : s_(s) {
  }
  void Apply(HeapEntry** entry) {
    s_->GetNodeId(*entry);
  }
 private:
  HeapSnapshotJSONSerializer* s_;
};

void HeapSnapshotJSONSerializer::EnumerateNodes() {
  GetNodeId(snapshot_->root());  // Make sure root gets the first id.
  HeapSnapshotJSONSerializerEnumerator iter(this);
  snapshot_->IterateEntries(&iter);
}


int HeapSnapshotJSONSerializer::GetNodeId(HeapEntry* entry) {
  HashMap::Entry* cache_entry = nodes_.Lookup(entry, ObjectHash(entry), true);
  if (cache_entry->value == NULL) {
    cache_entry->value = reinterpret_cast<void*>(next_node_id_++);
  }
  return static_cast<int>(reinterpret_cast<intptr_t>(cache_entry->value));
}


int HeapSnapshotJSONSerializer::GetStringId(const char* s) {
  HashMap::Entry* cache_entry = strings_.Lookup(
      const_cast<char*>(s), ObjectHash(s), true);
  if (cache_entry->value == NULL) {
    cache_entry->value = reinterpret_cast<void*>(next_string_id_++);
  }
  return static_cast<int>(reinterpret_cast<intptr_t>(cache_entry->value));
}


void HeapSnapshotJSONSerializer::SerializeEdge(HeapGraphEdge* edge) {
  writer_->AddCharacter(',');
  writer_->AddNumber(edge->type());
  writer_->AddCharacter(',');
  if (edge->type() == HeapGraphEdge::kElement
      || edge->type() == HeapGraphEdge::kHidden) {
    writer_->AddNumber(edge->index());
  } else {
    writer_->AddNumber(GetStringId(edge->name()));
  }
  writer_->AddCharacter(',');
  writer_->AddNumber(GetNodeId(edge->to()));
}


void HeapSnapshotJSONSerializer::SerializeNode(HeapEntry* entry) {
  writer_->AddCharacter('\n');
  writer_->AddCharacter(',');
  writer_->AddNumber(entry->type());
  writer_->AddCharacter(',');
  writer_->AddNumber(GetStringId(entry->name()));
  writer_->AddCharacter(',');
  writer_->AddNumber(entry->id());
  writer_->AddCharacter(',');
  writer_->AddNumber(entry->self_size());
  writer_->AddCharacter(',');
  writer_->AddNumber(entry->RetainedSize(false));
  writer_->AddCharacter(',');
  writer_->AddNumber(GetNodeId(entry->dominator()));
  Vector<HeapGraphEdge> children = entry->children();
  writer_->AddCharacter(',');
  writer_->AddNumber(children.length());
  for (int i = 0; i < children.length(); ++i) {
    SerializeEdge(&children[i]);
    if (writer_->aborted()) return;
  }
}


void HeapSnapshotJSONSerializer::SerializeNodes() {
  // The first (zero) item of nodes array is an object describing node
  // serialization layout.  We use a set of macros to improve
  // readability.
#define JSON_A(s) "["s"]"
#define JSON_O(s) "{"s"}"
#define JSON_S(s) "\""s"\""
  writer_->AddString(JSON_O(
    JSON_S("fields") ":" JSON_A(
        JSON_S("type")
        "," JSON_S("name")
        "," JSON_S("id")
        "," JSON_S("self_size")
        "," JSON_S("retained_size")
        "," JSON_S("dominator")
        "," JSON_S("children_count")
        "," JSON_S("children"))
    "," JSON_S("types") ":" JSON_A(
        JSON_A(
            JSON_S("hidden")
            "," JSON_S("array")
            "," JSON_S("string")
            "," JSON_S("object")
            "," JSON_S("code")
            "," JSON_S("closure")
            "," JSON_S("regexp")
            "," JSON_S("number"))
        "," JSON_S("string")
        "," JSON_S("number")
        "," JSON_S("number")
        "," JSON_S("number")
        "," JSON_S("number")
        "," JSON_S("number")
        "," JSON_O(
            JSON_S("fields") ":" JSON_A(
                JSON_S("type")
                "," JSON_S("name_or_index")
                "," JSON_S("to_node"))
            "," JSON_S("types") ":" JSON_A(
                JSON_A(
                    JSON_S("context")
                    "," JSON_S("element")
                    "," JSON_S("property")
                    "," JSON_S("internal")
                    "," JSON_S("hidden")
                    "," JSON_S("shortcut"))
                "," JSON_S("string_or_number")
                "," JSON_S("node"))))));
#undef JSON_S
#undef JSON_O
#undef JSON_A

  const int node_fields_count = 7;
  // type,name,id,self_size,retained_size,dominator,children_count.
  const int edge_fields_count = 3;  // type,name|index,to_node.
  List<HashMap::Entry*> sorted_nodes;
  SortHashMap(&nodes_, &sorted_nodes);
  // Rewrite node ids, so they refer to actual array positions.
  if (sorted_nodes.length() > 1) {
    // Nodes start from array index 1.
    int prev_value = 1;
    sorted_nodes[0]->value = reinterpret_cast<void*>(prev_value);
    for (int i = 1; i < sorted_nodes.length(); ++i) {
      HeapEntry* prev_heap_entry =
          reinterpret_cast<HeapEntry*>(sorted_nodes[i-1]->key);
      prev_value += node_fields_count +
          prev_heap_entry->children().length() * edge_fields_count;
      sorted_nodes[i]->value = reinterpret_cast<void*>(prev_value);
    }
  }
  for (int i = 0; i < sorted_nodes.length(); ++i) {
    SerializeNode(reinterpret_cast<HeapEntry*>(sorted_nodes[i]->key));
    if (writer_->aborted()) return;
  }
}


void HeapSnapshotJSONSerializer::SerializeSnapshot() {
  writer_->AddString("\"title\":\"");
  writer_->AddString(snapshot_->title());
  writer_->AddString("\"");
  writer_->AddString(",\"uid\":");
  writer_->AddNumber(snapshot_->uid());
}


static void WriteUChar(OutputStreamWriter* w, unibrow::uchar u) {
  static const char hex_chars[] = "0123456789ABCDEF";
  w->AddString("\\u");
  w->AddCharacter(hex_chars[(u >> 12) & 0xf]);
  w->AddCharacter(hex_chars[(u >> 8) & 0xf]);
  w->AddCharacter(hex_chars[(u >> 4) & 0xf]);
  w->AddCharacter(hex_chars[u & 0xf]);
}

void HeapSnapshotJSONSerializer::SerializeString(const unsigned char* s) {
  writer_->AddCharacter('\n');
  writer_->AddCharacter('\"');
  for ( ; *s != '\0'; ++s) {
    switch (*s) {
      case '\b':
        writer_->AddString("\\b");
        continue;
      case '\f':
        writer_->AddString("\\f");
        continue;
      case '\n':
        writer_->AddString("\\n");
        continue;
      case '\r':
        writer_->AddString("\\r");
        continue;
      case '\t':
        writer_->AddString("\\t");
        continue;
      case '\"':
      case '\\':
        writer_->AddCharacter('\\');
        writer_->AddCharacter(*s);
        continue;
      default:
        if (*s > 31 && *s < 128) {
          writer_->AddCharacter(*s);
        } else if (*s <= 31) {
          // Special character with no dedicated literal.
          WriteUChar(writer_, *s);
        } else {
          // Convert UTF-8 into \u UTF-16 literal.
          unsigned length = 1, cursor = 0;
          for ( ; length <= 4 && *(s + length) != '\0'; ++length) { }
          unibrow::uchar c = unibrow::Utf8::CalculateValue(s, length, &cursor);
          if (c != unibrow::Utf8::kBadChar) {
            WriteUChar(writer_, c);
            ASSERT(cursor != 0);
            s += cursor - 1;
          } else {
            writer_->AddCharacter('?');
          }
        }
    }
  }
  writer_->AddCharacter('\"');
}


void HeapSnapshotJSONSerializer::SerializeStrings() {
  List<HashMap::Entry*> sorted_strings;
  SortHashMap(&strings_, &sorted_strings);
  writer_->AddString("\"<dummy>\"");
  for (int i = 0; i < sorted_strings.length(); ++i) {
    writer_->AddCharacter(',');
    SerializeString(
        reinterpret_cast<const unsigned char*>(sorted_strings[i]->key));
    if (writer_->aborted()) return;
  }
}


template<typename T>
inline static int SortUsingEntryValue(const T* x, const T* y) {
  uintptr_t x_uint = reinterpret_cast<uintptr_t>((*x)->value);
  uintptr_t y_uint = reinterpret_cast<uintptr_t>((*y)->value);
  if (x_uint > y_uint) {
    return 1;
  } else if (x_uint == y_uint) {
    return 0;
  } else {
    return -1;
  }
}


void HeapSnapshotJSONSerializer::SortHashMap(
    HashMap* map, List<HashMap::Entry*>* sorted_entries) {
  for (HashMap::Entry* p = map->Start(); p != NULL; p = map->Next(p))
    sorted_entries->Add(p);
  sorted_entries->Sort(SortUsingEntryValue);
}


String* GetConstructorNameForHeapProfile(JSObject* object) {
  if (object->IsJSFunction()) return Heap::closure_symbol();
  return object->constructor_name();
}

} }  // namespace v8::internal

#endif  // ENABLE_LOGGING_AND_PROFILING
