// Copyright 2012 the V8 project authors. All rights reserved.
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

#include "v8.h"

#include "profile-generator-inl.h"

#include "global-handles.h"
#include "heap-profiler.h"
#include "scopeinfo.h"
#include "unicode.h"
#include "zone-inl.h"

namespace v8 {
namespace internal {


TokenEnumerator::TokenEnumerator()
    : token_locations_(4),
      token_removed_(4) {
}


TokenEnumerator::~TokenEnumerator() {
  Isolate* isolate = Isolate::Current();
  for (int i = 0; i < token_locations_.length(); ++i) {
    if (!token_removed_[i]) {
      isolate->global_handles()->ClearWeakness(token_locations_[i]);
      isolate->global_handles()->Destroy(token_locations_[i]);
    }
  }
}


int TokenEnumerator::GetTokenId(Object* token) {
  Isolate* isolate = Isolate::Current();
  if (token == NULL) return TokenEnumerator::kNoSecurityToken;
  for (int i = 0; i < token_locations_.length(); ++i) {
    if (*token_locations_[i] == token && !token_removed_[i]) return i;
  }
  Handle<Object> handle = isolate->global_handles()->Create(token);
  // handle.location() points to a memory cell holding a pointer
  // to a token object in the V8's heap.
  isolate->global_handles()->MakeWeak(handle.location(), this,
                                      TokenRemovedCallback);
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


StringsStorage::~StringsStorage() {
  for (HashMap::Entry* p = names_.Start();
       p != NULL;
       p = names_.Next(p)) {
    DeleteArray(reinterpret_cast<const char*>(p->value));
  }
}


const char* StringsStorage::GetCopy(const char* src) {
  int len = static_cast<int>(strlen(src));
  Vector<char> dst = Vector<char>::New(len + 1);
  OS::StrNCpy(dst, src, len);
  dst[len] = '\0';
  uint32_t hash =
      HashSequentialString(dst.start(), len, HEAP->HashSeed());
  return AddOrDisposeString(dst.start(), hash);
}


const char* StringsStorage::GetFormatted(const char* format, ...) {
  va_list args;
  va_start(args, format);
  const char* result = GetVFormatted(format, args);
  va_end(args);
  return result;
}


const char* StringsStorage::AddOrDisposeString(char* str, uint32_t hash) {
  HashMap::Entry* cache_entry = names_.Lookup(str, hash, true);
  if (cache_entry->value == NULL) {
    // New entry added.
    cache_entry->value = str;
  } else {
    DeleteArray(str);
  }
  return reinterpret_cast<const char*>(cache_entry->value);
}


const char* StringsStorage::GetVFormatted(const char* format, va_list args) {
  Vector<char> str = Vector<char>::New(1024);
  int len = OS::VSNPrintF(str, format, args);
  if (len == -1) {
    DeleteArray(str.start());
    return format;
  }
  uint32_t hash = HashSequentialString(
      str.start(), len, HEAP->HashSeed());
  return AddOrDisposeString(str.start(), hash);
}


const char* StringsStorage::GetName(String* name) {
  if (name->IsString()) {
    int length = Min(kMaxNameSize, name->length());
    SmartArrayPointer<char> data =
        name->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL, 0, length);
    uint32_t hash =
        HashSequentialString(*data, length, name->GetHeap()->HashSeed());
    return AddOrDisposeString(data.Detach(), hash);
  }
  return "";
}


const char* StringsStorage::GetName(int index) {
  return GetFormatted("%d", index);
}


const char* const CodeEntry::kEmptyNamePrefix = "";


void CodeEntry::CopyData(const CodeEntry& source) {
  tag_ = source.tag_;
  name_prefix_ = source.name_prefix_;
  name_ = source.name_;
  resource_name_ = source.resource_name_;
  line_number_ = source.line_number_;
}


uint32_t CodeEntry::GetCallUid() const {
  uint32_t hash = ComputeIntegerHash(tag_, v8::internal::kZeroHashSeed);
  if (shared_id_ != 0) {
    hash ^= ComputeIntegerHash(static_cast<uint32_t>(shared_id_),
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


bool CodeEntry::IsSameAs(CodeEntry* entry) const {
  return this == entry
      || (tag_ == entry->tag_
          && shared_id_ == entry->shared_id_
          && (shared_id_ != 0
              || (name_prefix_ == entry->name_prefix_
                  && name_ == entry->name_
                  && resource_name_ == entry->resource_name_
                  && line_number_ == entry->line_number_)));
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
  FilteredCloneCallback(ProfileNode* dst_root, int security_token_id)
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


CodeEntry* const CodeMap::kSharedFunctionCodeEntry = NULL;
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


int CodeMap::GetSharedId(Address addr) {
  CodeTree::Locator locator;
  // For shared function entries, 'size' field is used to store their IDs.
  if (tree_.Find(addr, &locator)) {
    const CodeEntryInfo& entry = locator.value();
    ASSERT(entry.entry == kSharedFunctionCodeEntry);
    return entry.size;
  } else {
    tree_.Insert(addr, &locator);
    int id = next_shared_id_++;
    locator.set_value(CodeEntryInfo(kSharedFunctionCodeEntry, id));
    return id;
  }
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
  if (*list_ptr != NULL) {
    (*list_ptr)->Iterate(DeleteCpuProfile);
    delete *list_ptr;
  }
}

CpuProfilesCollection::~CpuProfilesCollection() {
  delete current_profiles_semaphore_;
  current_profiles_.Iterate(DeleteCpuProfile);
  detached_profiles_.Iterate(DeleteCpuProfile);
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
  int index = GetProfileIndex(uid);
  if (index < 0) return NULL;
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


int CpuProfilesCollection::GetProfileIndex(unsigned uid) {
  HashMap::Entry* entry = profiles_uids_.Lookup(reinterpret_cast<void*>(uid),
                                                static_cast<uint32_t>(uid),
                                                false);
  return entry != NULL ?
      static_cast<int>(reinterpret_cast<intptr_t>(entry->value)) : -1;
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
  unsigned uid = profile->uid();
  int index = GetProfileIndex(uid);
  if (index < 0) {
    detached_profiles_.RemoveElement(profile);
    return;
  }
  profiles_uids_.Remove(reinterpret_cast<void*>(uid),
                        static_cast<uint32_t>(uid));
  // Decrement all indexes above the deleted one.
  for (HashMap::Entry* p = profiles_uids_.Start();
       p != NULL;
       p = profiles_uids_.Next(p)) {
    intptr_t p_index = reinterpret_cast<intptr_t>(p->value);
    if (p_index > index) {
      p->value = reinterpret_cast<void*>(p_index - 1);
    }
  }
  for (int i = 0; i < profiles_by_token_.length(); ++i) {
    List<CpuProfile*>* list = profiles_by_token_[i];
    if (list != NULL && index < list->length()) {
      // Move all filtered clones into detached_profiles_,
      // so we can know that they are still in use.
      CpuProfile* cloned_profile = list->Remove(index);
      if (cloned_profile != NULL && cloned_profile != profile) {
        detached_profiles_.Add(cloned_profile);
      }
    }
  }
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


const char* const ProfileGenerator::kAnonymousFunctionName =
    "(anonymous function)";
const char* const ProfileGenerator::kProgramEntryName =
    "(program)";
const char* const ProfileGenerator::kGarbageCollectorEntryName =
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

    if (sample.has_external_callback) {
      // Don't use PC when in external callback code, as it can point
      // inside callback's code, and we will erroneously report
      // that a callback calls itself.
      *(entries.start()) = NULL;
      *entry++ = code_map_.FindEntry(sample.external_callback);
    } else if (sample.tos != NULL) {
      // Find out, if top of stack was pointing inside a JS function
      // meaning that we have encountered a frameless invocation.
      *entry = code_map_.FindEntry(sample.tos);
      if (*entry != NULL && !(*entry)->is_js_function()) {
        *entry = NULL;
      }
      entry++;
    }

    for (const Address* stack_pos = sample.stack,
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
  ASSERT(type == kElement || type == kHidden || type == kWeak);
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
                     SnapshotObjectId id,
                     int self_size,
                     int children_count,
                     int retainers_count) {
  snapshot_ = snapshot;
  type_ = type;
  painted_ = false;
  name_ = name;
  self_size_ = self_size;
  retained_size_ = 0;
  children_count_ = children_count;
  retainers_count_ = retainers_count;
  dominator_ = NULL;
  id_ = id;
}


void HeapEntry::SetNamedReference(HeapGraphEdge::Type type,
                                  int child_index,
                                  const char* name,
                                  HeapEntry* entry,
                                  int retainer_index) {
  children()[child_index].Init(child_index, type, name, entry);
  entry->retainers()[retainer_index] = children_arr() + child_index;
}


void HeapEntry::SetIndexedReference(HeapGraphEdge::Type type,
                                    int child_index,
                                    int index,
                                    HeapEntry* entry,
                                    int retainer_index) {
  children()[child_index].Init(child_index, type, index, entry);
  entry->retainers()[retainer_index] = children_arr() + child_index;
}


void HeapEntry::SetUnidirElementReference(
    int child_index, int index, HeapEntry* entry) {
  children()[child_index].Init(child_index, index, entry);
}


Handle<HeapObject> HeapEntry::GetHeapObject() {
  return snapshot_->collection()->FindHeapObjectById(id());
}


void HeapEntry::Print(
    const char* prefix, const char* edge_name, int max_depth, int indent) {
  OS::Print("%6d %7d @%6llu %*c %s%s: ",
            self_size(), retained_size(), id(),
            indent, ' ', prefix, edge_name);
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
    const char* edge_prefix = "";
    EmbeddedVector<char, 64> index;
    const char* edge_name = index.start();
    switch (edge.type()) {
      case HeapGraphEdge::kContextVariable:
        edge_prefix = "#";
        edge_name = edge.name();
        break;
      case HeapGraphEdge::kElement:
        OS::SNPrintF(index, "%d", edge.index());
        break;
      case HeapGraphEdge::kInternal:
        edge_prefix = "$";
        edge_name = edge.name();
        break;
      case HeapGraphEdge::kProperty:
        edge_name = edge.name();
        break;
      case HeapGraphEdge::kHidden:
        edge_prefix = "$";
        OS::SNPrintF(index, "%d", edge.index());
        break;
      case HeapGraphEdge::kShortcut:
        edge_prefix = "^";
        edge_name = edge.name();
        break;
      case HeapGraphEdge::kWeak:
        edge_prefix = "w";
        OS::SNPrintF(index, "%d", edge.index());
        break;
      default:
        OS::SNPrintF(index, "!!! unknown edge type: %d ", edge.type());
    }
    edge.to()->Print(edge_prefix, edge_name, max_depth, indent + 2);
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
    case kNative: return "/native/";
    case kSynthetic: return "/synthetic/";
    default: return "???";
  }
}


size_t HeapEntry::EntriesSize(int entries_count,
                              int children_count,
                              int retainers_count) {
  return sizeof(HeapEntry) * entries_count         // NOLINT
      + sizeof(HeapGraphEdge) * children_count     // NOLINT
      + sizeof(HeapGraphEdge*) * retainers_count;  // NOLINT
}


// It is very important to keep objects that form a heap snapshot
// as small as possible.
namespace {  // Avoid littering the global namespace.

template <size_t ptr_size> struct SnapshotSizeConstants;

template <> struct SnapshotSizeConstants<4> {
  static const int kExpectedHeapGraphEdgeSize = 12;
  static const int kExpectedHeapEntrySize = 32;
  static const size_t kMaxSerializableSnapshotRawSize = 256 * MB;
};

template <> struct SnapshotSizeConstants<8> {
  static const int kExpectedHeapGraphEdgeSize = 24;
  static const int kExpectedHeapEntrySize = 48;
  static const uint64_t kMaxSerializableSnapshotRawSize =
      static_cast<uint64_t>(6000) * MB;
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
      natives_root_entry_(NULL),
      raw_entries_(NULL),
      entries_sorted_(false) {
  STATIC_CHECK(
      sizeof(HeapGraphEdge) ==
      SnapshotSizeConstants<kPointerSize>::kExpectedHeapGraphEdgeSize);
  STATIC_CHECK(
      sizeof(HeapEntry) ==
      SnapshotSizeConstants<kPointerSize>::kExpectedHeapEntrySize);
  for (int i = 0; i < VisitorSynchronization::kNumberOfSyncTags; ++i) {
    gc_subroot_entries_[i] = NULL;
  }
}


HeapSnapshot::~HeapSnapshot() {
  DeleteArray(raw_entries_);
}


void HeapSnapshot::Delete() {
  collection_->RemoveSnapshot(this);
  delete this;
}


void HeapSnapshot::AllocateEntries(int entries_count,
                                   int children_count,
                                   int retainers_count) {
  ASSERT(raw_entries_ == NULL);
  raw_entries_size_ =
      HeapEntry::EntriesSize(entries_count, children_count, retainers_count);
  raw_entries_ = NewArray<char>(raw_entries_size_);
}


static void HeapEntryClearPaint(HeapEntry** entry_ptr) {
  (*entry_ptr)->clear_paint();
}


void HeapSnapshot::ClearPaint() {
  entries_.Iterate(HeapEntryClearPaint);
}


HeapEntry* HeapSnapshot::AddRootEntry(int children_count) {
  ASSERT(root_entry_ == NULL);
  return (root_entry_ = AddEntry(HeapEntry::kObject,
                                 "",
                                 HeapObjectsMap::kInternalRootObjectId,
                                 0,
                                 children_count,
                                 0));
}


HeapEntry* HeapSnapshot::AddGcRootsEntry(int children_count,
                                         int retainers_count) {
  ASSERT(gc_roots_entry_ == NULL);
  return (gc_roots_entry_ = AddEntry(HeapEntry::kObject,
                                     "(GC roots)",
                                     HeapObjectsMap::kGcRootsObjectId,
                                     0,
                                     children_count,
                                     retainers_count));
}


HeapEntry* HeapSnapshot::AddGcSubrootEntry(int tag,
                                           int children_count,
                                           int retainers_count) {
  ASSERT(gc_subroot_entries_[tag] == NULL);
  ASSERT(0 <= tag && tag < VisitorSynchronization::kNumberOfSyncTags);
  return (gc_subroot_entries_[tag] = AddEntry(
      HeapEntry::kObject,
      VisitorSynchronization::kTagNames[tag],
      HeapObjectsMap::GetNthGcSubrootId(tag),
      0,
      children_count,
      retainers_count));
}


HeapEntry* HeapSnapshot::AddEntry(HeapEntry::Type type,
                                  const char* name,
                                  SnapshotObjectId id,
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


HeapEntry* HeapSnapshot::GetEntryById(SnapshotObjectId id) {
  List<HeapEntry*>* entries_by_id = GetSortedEntriesList();

  // Perform a binary search by id.
  int low = 0;
  int high = entries_by_id->length() - 1;
  while (low <= high) {
    int mid =
        (static_cast<unsigned int>(low) + static_cast<unsigned int>(high)) >> 1;
    SnapshotObjectId mid_id = entries_by_id->at(mid)->id();
    if (mid_id > id)
      high = mid - 1;
    else if (mid_id < id)
      low = mid + 1;
    else
      return entries_by_id->at(mid);
  }
  return NULL;
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
  root()->Print("", "", max_depth, 0);
}


// We split IDs on evens for embedder objects (see
// HeapObjectsMap::GenerateId) and odds for native objects.
const SnapshotObjectId HeapObjectsMap::kInternalRootObjectId = 1;
const SnapshotObjectId HeapObjectsMap::kGcRootsObjectId =
    HeapObjectsMap::kInternalRootObjectId + HeapObjectsMap::kObjectIdStep;
const SnapshotObjectId HeapObjectsMap::kGcRootsFirstSubrootId =
    HeapObjectsMap::kGcRootsObjectId + HeapObjectsMap::kObjectIdStep;
const SnapshotObjectId HeapObjectsMap::kFirstAvailableObjectId =
    HeapObjectsMap::kGcRootsFirstSubrootId +
    VisitorSynchronization::kNumberOfSyncTags * HeapObjectsMap::kObjectIdStep;

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


SnapshotObjectId HeapObjectsMap::FindObject(Address addr) {
  if (!initial_fill_mode_) {
    SnapshotObjectId existing = FindEntry(addr);
    if (existing != 0) return existing;
  }
  SnapshotObjectId id = next_id_;
  next_id_ += kObjectIdStep;
  AddEntry(addr, id);
  return id;
}


void HeapObjectsMap::MoveObject(Address from, Address to) {
  if (from == to) return;
  HashMap::Entry* entry = entries_map_.Lookup(from, AddressHash(from), false);
  if (entry != NULL) {
    void* value = entry->value;
    entries_map_.Remove(from, AddressHash(from));
    if (to != NULL) {
      entry = entries_map_.Lookup(to, AddressHash(to), true);
      // We can have an entry at the new location, it is OK, as GC can overwrite
      // dead objects with alive objects being moved.
      entry->value = value;
    }
  }
}


void HeapObjectsMap::AddEntry(Address addr, SnapshotObjectId id) {
  HashMap::Entry* entry = entries_map_.Lookup(addr, AddressHash(addr), true);
  ASSERT(entry->value == NULL);
  entry->value = reinterpret_cast<void*>(entries_->length());
  entries_->Add(EntryInfo(id));
}


SnapshotObjectId HeapObjectsMap::FindEntry(Address addr) {
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


SnapshotObjectId HeapObjectsMap::GenerateId(v8::RetainedObjectInfo* info) {
  SnapshotObjectId id = static_cast<SnapshotObjectId>(info->GetHash());
  const char* label = info->GetLabel();
  id ^= HashSequentialString(label,
                             static_cast<int>(strlen(label)),
                             HEAP->HashSeed());
  intptr_t element_count = info->GetElementCount();
  if (element_count != -1)
    id ^= ComputeIntegerHash(static_cast<uint32_t>(element_count),
                             v8::internal::kZeroHashSeed);
  return id << 1;
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


void HeapSnapshotsCollection::RemoveSnapshot(HeapSnapshot* snapshot) {
  snapshots_.RemoveElement(snapshot);
  unsigned uid = snapshot->uid();
  snapshots_uids_.Remove(reinterpret_cast<void*>(uid),
                         static_cast<uint32_t>(uid));
}


Handle<HeapObject> HeapSnapshotsCollection::FindHeapObjectById(
    SnapshotObjectId id) {
  // First perform a full GC in order to avoid dead objects.
  HEAP->CollectAllGarbage(Heap::kMakeHeapIterableMask,
                          "HeapSnapshotsCollection::FindHeapObjectById");
  AssertNoAllocation no_allocation;
  HeapObject* object = NULL;
  HeapIterator iterator(HeapIterator::kFilterUnreachable);
  // Make sure that object with the given id is still reachable.
  for (HeapObject* obj = iterator.next();
       obj != NULL;
       obj = iterator.next()) {
    if (ids_.FindObject(obj->address()) == id) {
      ASSERT(object == NULL);
      object = obj;
      // Can't break -- kFilterUnreachable requires full heap traversal.
    }
  }
  return object != NULL ? Handle<HeapObject>(object) : Handle<HeapObject>();
}


HeapEntry* const HeapEntriesMap::kHeapEntryPlaceholder =
    reinterpret_cast<HeapEntry*>(1);

HeapEntriesMap::HeapEntriesMap()
    : entries_(HeapThingsMatch),
      entries_count_(0),
      total_children_count_(0),
      total_retainers_count_(0) {
}


HeapEntriesMap::~HeapEntriesMap() {
  for (HashMap::Entry* p = entries_.Start(); p != NULL; p = entries_.Next(p)) {
    delete reinterpret_cast<EntryInfo*>(p->value);
  }
}


void HeapEntriesMap::AllocateEntries() {
  for (HashMap::Entry* p = entries_.Start();
       p != NULL;
       p = entries_.Next(p)) {
    EntryInfo* entry_info = reinterpret_cast<EntryInfo*>(p->value);
    entry_info->entry = entry_info->allocator->AllocateEntry(
        p->key,
        entry_info->children_count,
        entry_info->retainers_count);
    ASSERT(entry_info->entry != NULL);
    ASSERT(entry_info->entry != kHeapEntryPlaceholder);
    entry_info->children_count = 0;
    entry_info->retainers_count = 0;
  }
}


HeapEntry* HeapEntriesMap::Map(HeapThing thing) {
  HashMap::Entry* cache_entry = entries_.Lookup(thing, Hash(thing), false);
  if (cache_entry != NULL) {
    EntryInfo* entry_info = reinterpret_cast<EntryInfo*>(cache_entry->value);
    return entry_info->entry;
  } else {
    return NULL;
  }
}


void HeapEntriesMap::Pair(
    HeapThing thing, HeapEntriesAllocator* allocator, HeapEntry* entry) {
  HashMap::Entry* cache_entry = entries_.Lookup(thing, Hash(thing), true);
  ASSERT(cache_entry->value == NULL);
  cache_entry->value = new EntryInfo(entry, allocator);
  ++entries_count_;
}


void HeapEntriesMap::CountReference(HeapThing from, HeapThing to,
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
    : entries_(HeapEntriesMap::HeapThingsMatch) {
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


const char* HeapObjectsSet::GetTag(Object* obj) {
  HeapObject* object = HeapObject::cast(obj);
  HashMap::Entry* cache_entry =
      entries_.Lookup(object, HeapEntriesMap::Hash(object), false);
  if (cache_entry != NULL
      && cache_entry->value != HeapEntriesMap::kHeapEntryPlaceholder) {
    return reinterpret_cast<const char*>(cache_entry->value);
  } else {
    return NULL;
  }
}


void HeapObjectsSet::SetTag(Object* obj, const char* tag) {
  if (!obj->IsHeapObject()) return;
  HeapObject* object = HeapObject::cast(obj);
  HashMap::Entry* cache_entry =
      entries_.Lookup(object, HeapEntriesMap::Hash(object), true);
  cache_entry->value = const_cast<char*>(tag);
}


HeapObject* const V8HeapExplorer::kInternalRootObject =
    reinterpret_cast<HeapObject*>(
        static_cast<intptr_t>(HeapObjectsMap::kInternalRootObjectId));
HeapObject* const V8HeapExplorer::kGcRootsObject =
    reinterpret_cast<HeapObject*>(
        static_cast<intptr_t>(HeapObjectsMap::kGcRootsObjectId));
HeapObject* const V8HeapExplorer::kFirstGcSubrootObject =
    reinterpret_cast<HeapObject*>(
        static_cast<intptr_t>(HeapObjectsMap::kGcRootsFirstSubrootId));
HeapObject* const V8HeapExplorer::kLastGcSubrootObject =
    reinterpret_cast<HeapObject*>(
        static_cast<intptr_t>(HeapObjectsMap::kFirstAvailableObjectId));


V8HeapExplorer::V8HeapExplorer(
    HeapSnapshot* snapshot,
    SnapshottingProgressReportingInterface* progress)
    : heap_(Isolate::Current()->heap()),
      snapshot_(snapshot),
      collection_(snapshot_->collection()),
      progress_(progress),
      filler_(NULL) {
}


V8HeapExplorer::~V8HeapExplorer() {
}


HeapEntry* V8HeapExplorer::AllocateEntry(
    HeapThing ptr, int children_count, int retainers_count) {
  return AddEntry(
      reinterpret_cast<HeapObject*>(ptr), children_count, retainers_count);
}


HeapEntry* V8HeapExplorer::AddEntry(HeapObject* object,
                                    int children_count,
                                    int retainers_count) {
  if (object == kInternalRootObject) {
    ASSERT(retainers_count == 0);
    return snapshot_->AddRootEntry(children_count);
  } else if (object == kGcRootsObject) {
    return snapshot_->AddGcRootsEntry(children_count, retainers_count);
  } else if (object >= kFirstGcSubrootObject && object < kLastGcSubrootObject) {
    return snapshot_->AddGcSubrootEntry(
        GetGcSubrootOrder(object),
        children_count,
        retainers_count);
  } else if (object->IsJSFunction()) {
    JSFunction* func = JSFunction::cast(object);
    SharedFunctionInfo* shared = func->shared();
    const char* name = shared->bound() ? "native_bind" :
        collection_->names()->GetName(String::cast(shared->name()));
    return AddEntry(object,
                    HeapEntry::kClosure,
                    name,
                    children_count,
                    retainers_count);
  } else if (object->IsJSRegExp()) {
    JSRegExp* re = JSRegExp::cast(object);
    return AddEntry(object,
                    HeapEntry::kRegExp,
                    collection_->names()->GetName(re->Pattern()),
                    children_count,
                    retainers_count);
  } else if (object->IsJSObject()) {
    return AddEntry(object,
                    HeapEntry::kObject,
                    "",
                    children_count,
                    retainers_count);
  } else if (object->IsString()) {
    return AddEntry(object,
                    HeapEntry::kString,
                    collection_->names()->GetName(String::cast(object)),
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
                    collection_->names()->GetName(String::cast(shared->name())),
                    children_count,
                    retainers_count);
  } else if (object->IsScript()) {
    Script* script = Script::cast(object);
    return AddEntry(object,
                    HeapEntry::kCode,
                    script->name()->IsString() ?
                        collection_->names()->GetName(
                            String::cast(script->name()))
                        : "",
                    children_count,
                    retainers_count);
  } else if (object->IsGlobalContext()) {
    return AddEntry(object,
                    HeapEntry::kHidden,
                    "system / GlobalContext",
                    children_count,
                    retainers_count);
  } else if (object->IsContext()) {
    return AddEntry(object,
                    HeapEntry::kHidden,
                    "system / Context",
                    children_count,
                    retainers_count);
  } else if (object->IsFixedArray() ||
             object->IsFixedDoubleArray() ||
             object->IsByteArray() ||
             object->IsExternalArray()) {
    const char* tag = objects_tags_.GetTag(object);
    return AddEntry(object,
                    HeapEntry::kArray,
                    tag != NULL ? tag : "",
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
                  GetSystemEntryName(object),
                  children_count,
                  retainers_count);
}


HeapEntry* V8HeapExplorer::AddEntry(HeapObject* object,
                                    HeapEntry::Type type,
                                    const char* name,
                                    int children_count,
                                    int retainers_count) {
  return snapshot_->AddEntry(type,
                             name,
                             collection_->GetObjectId(object->address()),
                             object->Size(),
                             children_count,
                             retainers_count);
}


class GcSubrootsEnumerator : public ObjectVisitor {
 public:
  GcSubrootsEnumerator(
      SnapshotFillerInterface* filler, V8HeapExplorer* explorer)
      : filler_(filler),
        explorer_(explorer),
        previous_object_count_(0),
        object_count_(0) {
  }
  void VisitPointers(Object** start, Object** end) {
    object_count_ += end - start;
  }
  void Synchronize(VisitorSynchronization::SyncTag tag) {
    // Skip empty subroots.
    if (previous_object_count_ != object_count_) {
      previous_object_count_ = object_count_;
      filler_->AddEntry(V8HeapExplorer::GetNthGcSubrootObject(tag), explorer_);
    }
  }
 private:
  SnapshotFillerInterface* filler_;
  V8HeapExplorer* explorer_;
  intptr_t previous_object_count_;
  intptr_t object_count_;
};


void V8HeapExplorer::AddRootEntries(SnapshotFillerInterface* filler) {
  filler->AddEntry(kInternalRootObject, this);
  filler->AddEntry(kGcRootsObject, this);
  GcSubrootsEnumerator enumerator(filler, this);
  heap_->IterateRoots(&enumerator, VISIT_ALL);
}


const char* V8HeapExplorer::GetSystemEntryName(HeapObject* object) {
  switch (object->map()->instance_type()) {
    case MAP_TYPE: return "system / Map";
    case JS_GLOBAL_PROPERTY_CELL_TYPE: return "system / JSGlobalPropertyCell";
    case FOREIGN_TYPE: return "system / Foreign";
    case ODDBALL_TYPE: return "system / Oddball";
#define MAKE_STRUCT_CASE(NAME, Name, name) \
    case NAME##_TYPE: return "system / "#Name;
  STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE
    default: return "system";
  }
}


int V8HeapExplorer::EstimateObjectsCount(HeapIterator* iterator) {
  int objects_count = 0;
  for (HeapObject* obj = iterator->next();
       obj != NULL;
       obj = iterator->next()) {
    objects_count++;
  }
  return objects_count;
}


class IndexedReferencesExtractor : public ObjectVisitor {
 public:
  IndexedReferencesExtractor(V8HeapExplorer* generator,
                             HeapObject* parent_obj,
                             HeapEntry* parent_entry)
      : generator_(generator),
        parent_obj_(parent_obj),
        parent_(parent_entry),
        next_index_(1) {
  }
  void VisitPointers(Object** start, Object** end) {
    for (Object** p = start; p < end; p++) {
      if (CheckVisitedAndUnmark(p)) continue;
      generator_->SetHiddenReference(parent_obj_, parent_, next_index_++, *p);
    }
  }
  static void MarkVisitedField(HeapObject* obj, int offset) {
    if (offset < 0) return;
    Address field = obj->address() + offset;
    ASSERT(!Memory::Object_at(field)->IsFailure());
    ASSERT(Memory::Object_at(field)->IsHeapObject());
    *field |= kFailureTag;
  }

 private:
  bool CheckVisitedAndUnmark(Object** field) {
    if ((*field)->IsFailure()) {
      intptr_t untagged = reinterpret_cast<intptr_t>(*field) & ~kFailureTagMask;
      *field = reinterpret_cast<Object*>(untagged | kHeapObjectTag);
      ASSERT((*field)->IsHeapObject());
      return true;
    }
    return false;
  }
  V8HeapExplorer* generator_;
  HeapObject* parent_obj_;
  HeapEntry* parent_;
  int next_index_;
};


void V8HeapExplorer::ExtractReferences(HeapObject* obj) {
  HeapEntry* entry = GetEntry(obj);
  if (entry == NULL) return;  // No interest in this object.

  bool extract_indexed_refs = true;
  if (obj->IsJSGlobalProxy()) {
    // We need to reference JS global objects from snapshot's root.
    // We use JSGlobalProxy because this is what embedder (e.g. browser)
    // uses for the global object.
    JSGlobalProxy* proxy = JSGlobalProxy::cast(obj);
    SetRootShortcutReference(proxy->map()->prototype());
  } else if (obj->IsJSObject()) {
    JSObject* js_obj = JSObject::cast(obj);
    ExtractClosureReferences(js_obj, entry);
    ExtractPropertyReferences(js_obj, entry);
    ExtractElementReferences(js_obj, entry);
    ExtractInternalReferences(js_obj, entry);
    SetPropertyReference(
        obj, entry, heap_->Proto_symbol(), js_obj->GetPrototype());
    if (obj->IsJSFunction()) {
      JSFunction* js_fun = JSFunction::cast(js_obj);
      Object* proto_or_map = js_fun->prototype_or_initial_map();
      if (!proto_or_map->IsTheHole()) {
        if (!proto_or_map->IsMap()) {
          SetPropertyReference(
              obj, entry,
              heap_->prototype_symbol(), proto_or_map,
              NULL,
              JSFunction::kPrototypeOrInitialMapOffset);
        } else {
          SetPropertyReference(
              obj, entry,
              heap_->prototype_symbol(), js_fun->prototype());
        }
      }
      SharedFunctionInfo* shared_info = js_fun->shared();
      // JSFunction has either bindings or literals and never both.
      bool bound = shared_info->bound();
      TagObject(js_fun->literals_or_bindings(),
                bound ? "(function bindings)" : "(function literals)");
      SetInternalReference(js_fun, entry,
                           bound ? "bindings" : "literals",
                           js_fun->literals_or_bindings(),
                           JSFunction::kLiteralsOffset);
      SetInternalReference(js_fun, entry,
                           "shared", shared_info,
                           JSFunction::kSharedFunctionInfoOffset);
      TagObject(js_fun->unchecked_context(), "(context)");
      SetInternalReference(js_fun, entry,
                           "context", js_fun->unchecked_context(),
                           JSFunction::kContextOffset);
      for (int i = JSFunction::kNonWeakFieldsEndOffset;
           i < JSFunction::kSize;
           i += kPointerSize) {
        SetWeakReference(js_fun, entry, i, *HeapObject::RawField(js_fun, i), i);
      }
    }
    TagObject(js_obj->properties(), "(object properties)");
    SetInternalReference(obj, entry,
                         "properties", js_obj->properties(),
                         JSObject::kPropertiesOffset);
    TagObject(js_obj->elements(), "(object elements)");
    SetInternalReference(obj, entry,
                         "elements", js_obj->elements(),
                         JSObject::kElementsOffset);
  } else if (obj->IsString()) {
    if (obj->IsConsString()) {
      ConsString* cs = ConsString::cast(obj);
      SetInternalReference(obj, entry, 1, cs->first());
      SetInternalReference(obj, entry, 2, cs->second());
    }
    if (obj->IsSlicedString()) {
      SlicedString* ss = SlicedString::cast(obj);
      SetInternalReference(obj, entry, "parent", ss->parent());
    }
    extract_indexed_refs = false;
  } else if (obj->IsGlobalContext()) {
    Context* context = Context::cast(obj);
    TagObject(context->jsfunction_result_caches(),
              "(context func. result caches)");
    TagObject(context->normalized_map_cache(), "(context norm. map cache)");
    TagObject(context->runtime_context(), "(runtime context)");
    TagObject(context->data(), "(context data)");
    for (int i = Context::FIRST_WEAK_SLOT;
         i < Context::GLOBAL_CONTEXT_SLOTS;
         ++i) {
      SetWeakReference(obj, entry,
                       i, context->get(i),
                       FixedArray::OffsetOfElementAt(i));
    }
  } else if (obj->IsMap()) {
    Map* map = Map::cast(obj);
    SetInternalReference(obj, entry,
                         "prototype", map->prototype(), Map::kPrototypeOffset);
    SetInternalReference(obj, entry,
                         "constructor", map->constructor(),
                         Map::kConstructorOffset);
    if (!map->instance_descriptors()->IsEmpty()) {
      TagObject(map->instance_descriptors(), "(map descriptors)");
      SetInternalReference(obj, entry,
                           "descriptors", map->instance_descriptors(),
                           Map::kInstanceDescriptorsOrBitField3Offset);
    }
    if (map->prototype_transitions() != heap_->empty_fixed_array()) {
      TagObject(map->prototype_transitions(), "(prototype transitions)");
      SetInternalReference(obj,
                           entry,
                           "prototype_transitions",
                           map->prototype_transitions(),
                           Map::kPrototypeTransitionsOffset);
    }
    SetInternalReference(obj, entry,
                         "code_cache", map->code_cache(),
                         Map::kCodeCacheOffset);
  } else if (obj->IsSharedFunctionInfo()) {
    SharedFunctionInfo* shared = SharedFunctionInfo::cast(obj);
    SetInternalReference(obj, entry,
                         "name", shared->name(),
                         SharedFunctionInfo::kNameOffset);
    SetInternalReference(obj, entry,
                         "code", shared->unchecked_code(),
                         SharedFunctionInfo::kCodeOffset);
    TagObject(shared->scope_info(), "(function scope info)");
    SetInternalReference(obj, entry,
                         "scope_info", shared->scope_info(),
                         SharedFunctionInfo::kScopeInfoOffset);
    SetInternalReference(obj, entry,
                         "instance_class_name", shared->instance_class_name(),
                         SharedFunctionInfo::kInstanceClassNameOffset);
    SetInternalReference(obj, entry,
                         "script", shared->script(),
                         SharedFunctionInfo::kScriptOffset);
    SetWeakReference(obj, entry,
                     1, shared->initial_map(),
                     SharedFunctionInfo::kInitialMapOffset);
  } else if (obj->IsScript()) {
    Script* script = Script::cast(obj);
    SetInternalReference(obj, entry,
                         "source", script->source(),
                         Script::kSourceOffset);
    SetInternalReference(obj, entry,
                         "name", script->name(),
                         Script::kNameOffset);
    SetInternalReference(obj, entry,
                         "data", script->data(),
                         Script::kDataOffset);
    SetInternalReference(obj, entry,
                         "context_data", script->context_data(),
                         Script::kContextOffset);
    TagObject(script->line_ends(), "(script line ends)");
    SetInternalReference(obj, entry,
                         "line_ends", script->line_ends(),
                         Script::kLineEndsOffset);
  } else if (obj->IsCodeCache()) {
    CodeCache* code_cache = CodeCache::cast(obj);
    TagObject(code_cache->default_cache(), "(default code cache)");
    SetInternalReference(obj, entry,
                         "default_cache", code_cache->default_cache(),
                         CodeCache::kDefaultCacheOffset);
    TagObject(code_cache->normal_type_cache(), "(code type cache)");
    SetInternalReference(obj, entry,
                         "type_cache", code_cache->normal_type_cache(),
                         CodeCache::kNormalTypeCacheOffset);
  } else if (obj->IsCode()) {
    Code* code = Code::cast(obj);
    TagObject(code->unchecked_relocation_info(), "(code relocation info)");
    TagObject(code->unchecked_deoptimization_data(), "(code deopt data)");
  }
  if (extract_indexed_refs) {
    SetInternalReference(obj, entry, "map", obj->map(), HeapObject::kMapOffset);
    IndexedReferencesExtractor refs_extractor(this, obj, entry);
    obj->Iterate(&refs_extractor);
  }
}


void V8HeapExplorer::ExtractClosureReferences(JSObject* js_obj,
                                              HeapEntry* entry) {
  if (!js_obj->IsJSFunction()) return;

  JSFunction* func = JSFunction::cast(js_obj);
  Context* context = func->context();
  ScopeInfo* scope_info = context->closure()->shared()->scope_info();

  if (func->shared()->bound()) {
    FixedArray* bindings = func->function_bindings();
    SetNativeBindReference(js_obj, entry, "bound_this",
                           bindings->get(JSFunction::kBoundThisIndex));
    SetNativeBindReference(js_obj, entry, "bound_function",
                           bindings->get(JSFunction::kBoundFunctionIndex));
    for (int i = JSFunction::kBoundArgumentsStartIndex;
         i < bindings->length(); i++) {
      const char* reference_name = collection_->names()->GetFormatted(
          "bound_argument_%d",
          i - JSFunction::kBoundArgumentsStartIndex);
      SetNativeBindReference(js_obj, entry, reference_name,
                             bindings->get(i));
    }
  } else {
    // Add context allocated locals.
    int context_locals = scope_info->ContextLocalCount();
    for (int i = 0; i < context_locals; ++i) {
      String* local_name = scope_info->ContextLocalName(i);
      int idx = Context::MIN_CONTEXT_SLOTS + i;
      SetClosureReference(js_obj, entry, local_name, context->get(idx));
    }

    // Add function variable.
    if (scope_info->HasFunctionName()) {
      String* name = scope_info->FunctionName();
      int idx = Context::MIN_CONTEXT_SLOTS + context_locals;
#ifdef DEBUG
      VariableMode mode;
      ASSERT(idx == scope_info->FunctionContextSlotIndex(name, &mode));
#endif
      SetClosureReference(js_obj, entry, name, context->get(idx));
    }
  }
}


void V8HeapExplorer::ExtractPropertyReferences(JSObject* js_obj,
                                               HeapEntry* entry) {
  if (js_obj->HasFastProperties()) {
    DescriptorArray* descs = js_obj->map()->instance_descriptors();
    for (int i = 0; i < descs->number_of_descriptors(); i++) {
      switch (descs->GetType(i)) {
        case FIELD: {
          int index = descs->GetFieldIndex(i);
          if (index < js_obj->map()->inobject_properties()) {
            SetPropertyReference(
                js_obj, entry,
                descs->GetKey(i), js_obj->InObjectPropertyAt(index),
                NULL,
                js_obj->GetInObjectPropertyOffset(index));
          } else {
            SetPropertyReference(
                js_obj, entry,
                descs->GetKey(i), js_obj->FastPropertyAt(index));
          }
          break;
        }
        case CONSTANT_FUNCTION:
          SetPropertyReference(
              js_obj, entry,
              descs->GetKey(i), descs->GetConstantFunction(i));
          break;
        case CALLBACKS: {
          Object* callback_obj = descs->GetValue(i);
          if (callback_obj->IsAccessorPair()) {
            AccessorPair* accessors = AccessorPair::cast(callback_obj);
            if (Object* getter = accessors->getter()) {
              SetPropertyReference(js_obj, entry, descs->GetKey(i),
                                   getter, "get-%s");
            }
            if (Object* setter = accessors->setter()) {
              SetPropertyReference(js_obj, entry, descs->GetKey(i),
                                   setter, "set-%s");
            }
          }
          break;
        }
        case NORMAL:  // only in slow mode
        case HANDLER:  // only in lookup results, not in descriptors
        case INTERCEPTOR:  // only in lookup results, not in descriptors
        case MAP_TRANSITION:  // we do not care about transitions here...
        case ELEMENTS_TRANSITION:
        case CONSTANT_TRANSITION:
        case NULL_DESCRIPTOR:  // ... and not about "holes"
          break;
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


void V8HeapExplorer::ExtractElementReferences(JSObject* js_obj,
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
    SeededNumberDictionary* dictionary = js_obj->element_dictionary();
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


void V8HeapExplorer::ExtractInternalReferences(JSObject* js_obj,
                                               HeapEntry* entry) {
  int length = js_obj->GetInternalFieldCount();
  for (int i = 0; i < length; ++i) {
    Object* o = js_obj->GetInternalField(i);
    SetInternalReference(
        js_obj, entry, i, o, js_obj->GetInternalFieldOffset(i));
  }
}


String* V8HeapExplorer::GetConstructorName(JSObject* object) {
  Heap* heap = object->GetHeap();
  if (object->IsJSFunction()) return heap->closure_symbol();
  String* constructor_name = object->constructor_name();
  if (constructor_name == heap->Object_symbol()) {
    // Look up an immediate "constructor" property, if it is a function,
    // return its name. This is for instances of binding objects, which
    // have prototype constructor type "Object".
    Object* constructor_prop = NULL;
    LookupResult result(heap->isolate());
    object->LocalLookupRealNamedProperty(heap->constructor_symbol(), &result);
    if (result.IsProperty()) {
      constructor_prop = result.GetLazyValue();
    }
    if (constructor_prop->IsJSFunction()) {
      Object* maybe_name = JSFunction::cast(constructor_prop)->shared()->name();
      if (maybe_name->IsString()) {
        String* name = String::cast(maybe_name);
        if (name->length() > 0) return name;
      }
    }
  }
  return object->constructor_name();
}


HeapEntry* V8HeapExplorer::GetEntry(Object* obj) {
  if (!obj->IsHeapObject()) return NULL;
  return filler_->FindOrAddEntry(obj, this);
}


class RootsReferencesExtractor : public ObjectVisitor {
 private:
  struct IndexTag {
    IndexTag(int index, VisitorSynchronization::SyncTag tag)
        : index(index), tag(tag) { }
    int index;
    VisitorSynchronization::SyncTag tag;
  };

 public:
  RootsReferencesExtractor()
      : collecting_all_references_(false),
        previous_reference_count_(0) {
  }

  void VisitPointers(Object** start, Object** end) {
    if (collecting_all_references_) {
      for (Object** p = start; p < end; p++) all_references_.Add(*p);
    } else {
      for (Object** p = start; p < end; p++) strong_references_.Add(*p);
    }
  }

  void SetCollectingAllReferences() { collecting_all_references_ = true; }

  void FillReferences(V8HeapExplorer* explorer) {
    ASSERT(strong_references_.length() <= all_references_.length());
    for (int i = 0; i < reference_tags_.length(); ++i) {
      explorer->SetGcRootsReference(reference_tags_[i].tag);
    }
    int strong_index = 0, all_index = 0, tags_index = 0;
    while (all_index < all_references_.length()) {
      if (strong_index < strong_references_.length() &&
          strong_references_[strong_index] == all_references_[all_index]) {
        explorer->SetGcSubrootReference(reference_tags_[tags_index].tag,
                                        false,
                                        all_references_[all_index++]);
        ++strong_index;
      } else {
        explorer->SetGcSubrootReference(reference_tags_[tags_index].tag,
                                        true,
                                        all_references_[all_index++]);
      }
      if (reference_tags_[tags_index].index == all_index) ++tags_index;
    }
  }

  void Synchronize(VisitorSynchronization::SyncTag tag) {
    if (collecting_all_references_ &&
        previous_reference_count_ != all_references_.length()) {
      previous_reference_count_ = all_references_.length();
      reference_tags_.Add(IndexTag(previous_reference_count_, tag));
    }
  }

 private:
  bool collecting_all_references_;
  List<Object*> strong_references_;
  List<Object*> all_references_;
  int previous_reference_count_;
  List<IndexTag> reference_tags_;
};


bool V8HeapExplorer::IterateAndExtractReferences(
    SnapshotFillerInterface* filler) {
  HeapIterator iterator(HeapIterator::kFilterUnreachable);

  filler_ = filler;
  bool interrupted = false;

  // Heap iteration with filtering must be finished in any case.
  for (HeapObject* obj = iterator.next();
       obj != NULL;
       obj = iterator.next(), progress_->ProgressStep()) {
    if (!interrupted) {
      ExtractReferences(obj);
      if (!progress_->ProgressReport(false)) interrupted = true;
    }
  }
  if (interrupted) {
    filler_ = NULL;
    return false;
  }
  SetRootGcRootsReference();
  RootsReferencesExtractor extractor;
  heap_->IterateRoots(&extractor, VISIT_ONLY_STRONG);
  extractor.SetCollectingAllReferences();
  heap_->IterateRoots(&extractor, VISIT_ALL);
  extractor.FillReferences(this);
  filler_ = NULL;
  return progress_->ProgressReport(false);
}


bool V8HeapExplorer::IterateAndSetObjectNames(SnapshotFillerInterface* filler) {
  HeapIterator iterator(HeapIterator::kFilterUnreachable);
  filler_ = filler;
  for (HeapObject* obj = iterator.next(); obj != NULL; obj = iterator.next()) {
    SetObjectName(obj);
  }
  return true;
}


void V8HeapExplorer::SetObjectName(HeapObject* object) {
  if (!object->IsJSObject() || object->IsJSRegExp() || object->IsJSFunction()) {
    return;
  }
  const char* name = collection_->names()->GetName(
      GetConstructorName(JSObject::cast(object)));
  if (object->IsJSGlobalObject()) {
    const char* tag = objects_tags_.GetTag(object);
    if (tag != NULL) {
      name = collection_->names()->GetFormatted("%s / %s", name, tag);
    }
  }
  GetEntry(object)->set_name(name);
}


void V8HeapExplorer::SetClosureReference(HeapObject* parent_obj,
                                         HeapEntry* parent_entry,
                                         String* reference_name,
                                         Object* child_obj) {
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry != NULL) {
    filler_->SetNamedReference(HeapGraphEdge::kContextVariable,
                               parent_obj,
                               parent_entry,
                               collection_->names()->GetName(reference_name),
                               child_obj,
                               child_entry);
  }
}


void V8HeapExplorer::SetNativeBindReference(HeapObject* parent_obj,
                                            HeapEntry* parent_entry,
                                            const char* reference_name,
                                            Object* child_obj) {
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry != NULL) {
    filler_->SetNamedReference(HeapGraphEdge::kShortcut,
                               parent_obj,
                               parent_entry,
                               reference_name,
                               child_obj,
                               child_entry);
  }
}


void V8HeapExplorer::SetElementReference(HeapObject* parent_obj,
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
  }
}


void V8HeapExplorer::SetInternalReference(HeapObject* parent_obj,
                                          HeapEntry* parent_entry,
                                          const char* reference_name,
                                          Object* child_obj,
                                          int field_offset) {
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry != NULL) {
    filler_->SetNamedReference(HeapGraphEdge::kInternal,
                               parent_obj,
                               parent_entry,
                               reference_name,
                               child_obj,
                               child_entry);
    IndexedReferencesExtractor::MarkVisitedField(parent_obj, field_offset);
  }
}


void V8HeapExplorer::SetInternalReference(HeapObject* parent_obj,
                                          HeapEntry* parent_entry,
                                          int index,
                                          Object* child_obj,
                                          int field_offset) {
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry != NULL) {
    filler_->SetNamedReference(HeapGraphEdge::kInternal,
                               parent_obj,
                               parent_entry,
                               collection_->names()->GetName(index),
                               child_obj,
                               child_entry);
    IndexedReferencesExtractor::MarkVisitedField(parent_obj, field_offset);
  }
}


void V8HeapExplorer::SetHiddenReference(HeapObject* parent_obj,
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


void V8HeapExplorer::SetWeakReference(HeapObject* parent_obj,
                                      HeapEntry* parent_entry,
                                      int index,
                                      Object* child_obj,
                                      int field_offset) {
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry != NULL) {
    filler_->SetIndexedReference(HeapGraphEdge::kWeak,
                                 parent_obj,
                                 parent_entry,
                                 index,
                                 child_obj,
                                 child_entry);
    IndexedReferencesExtractor::MarkVisitedField(parent_obj, field_offset);
  }
}


void V8HeapExplorer::SetPropertyReference(HeapObject* parent_obj,
                                          HeapEntry* parent_entry,
                                          String* reference_name,
                                          Object* child_obj,
                                          const char* name_format_string,
                                          int field_offset) {
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry != NULL) {
    HeapGraphEdge::Type type = reference_name->length() > 0 ?
        HeapGraphEdge::kProperty : HeapGraphEdge::kInternal;
    const char* name = name_format_string  != NULL ?
        collection_->names()->GetFormatted(
            name_format_string,
            *reference_name->ToCString(DISALLOW_NULLS,
                                       ROBUST_STRING_TRAVERSAL)) :
        collection_->names()->GetName(reference_name);

    filler_->SetNamedReference(type,
                               parent_obj,
                               parent_entry,
                               name,
                               child_obj,
                               child_entry);
    IndexedReferencesExtractor::MarkVisitedField(parent_obj, field_offset);
  }
}


void V8HeapExplorer::SetPropertyShortcutReference(HeapObject* parent_obj,
                                                  HeapEntry* parent_entry,
                                                  String* reference_name,
                                                  Object* child_obj) {
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry != NULL) {
    filler_->SetNamedReference(HeapGraphEdge::kShortcut,
                               parent_obj,
                               parent_entry,
                               collection_->names()->GetName(reference_name),
                               child_obj,
                               child_entry);
  }
}


void V8HeapExplorer::SetRootGcRootsReference() {
  filler_->SetIndexedAutoIndexReference(
      HeapGraphEdge::kElement,
      kInternalRootObject, snapshot_->root(),
      kGcRootsObject, snapshot_->gc_roots());
}


void V8HeapExplorer::SetRootShortcutReference(Object* child_obj) {
  HeapEntry* child_entry = GetEntry(child_obj);
  ASSERT(child_entry != NULL);
  filler_->SetNamedAutoIndexReference(
      HeapGraphEdge::kShortcut,
      kInternalRootObject, snapshot_->root(),
      child_obj, child_entry);
}


void V8HeapExplorer::SetGcRootsReference(VisitorSynchronization::SyncTag tag) {
  filler_->SetIndexedAutoIndexReference(
      HeapGraphEdge::kElement,
      kGcRootsObject, snapshot_->gc_roots(),
      GetNthGcSubrootObject(tag), snapshot_->gc_subroot(tag));
}


void V8HeapExplorer::SetGcSubrootReference(
    VisitorSynchronization::SyncTag tag, bool is_weak, Object* child_obj) {
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry != NULL) {
    filler_->SetIndexedAutoIndexReference(
        is_weak ? HeapGraphEdge::kWeak : HeapGraphEdge::kElement,
        GetNthGcSubrootObject(tag), snapshot_->gc_subroot(tag),
        child_obj, child_entry);
  }
}


void V8HeapExplorer::TagObject(Object* obj, const char* tag) {
  if (obj->IsHeapObject() &&
      !obj->IsOddball() &&
      obj != heap_->raw_unchecked_empty_byte_array() &&
      obj != heap_->raw_unchecked_empty_fixed_array() &&
      obj != heap_->raw_unchecked_empty_descriptor_array()) {
    objects_tags_.SetTag(obj, tag);
  }
}


class GlobalObjectsEnumerator : public ObjectVisitor {
 public:
  virtual void VisitPointers(Object** start, Object** end) {
    for (Object** p = start; p < end; p++) {
      if ((*p)->IsGlobalContext()) {
        Context* context = Context::cast(*p);
        JSObject* proxy = context->global_proxy();
        if (proxy->IsJSGlobalProxy()) {
          Object* global = proxy->map()->prototype();
          if (global->IsJSGlobalObject()) {
            objects_.Add(Handle<JSGlobalObject>(JSGlobalObject::cast(global)));
          }
        }
      }
    }
  }
  int count() { return objects_.length(); }
  Handle<JSGlobalObject>& at(int i) { return objects_[i]; }

 private:
  List<Handle<JSGlobalObject> > objects_;
};


// Modifies heap. Must not be run during heap traversal.
void V8HeapExplorer::TagGlobalObjects() {
  HandleScope scope;
  Isolate* isolate = Isolate::Current();
  GlobalObjectsEnumerator enumerator;
  isolate->global_handles()->IterateAllRoots(&enumerator);
  Handle<String> document_string =
      isolate->factory()->NewStringFromAscii(CStrVector("document"));
  Handle<String> url_string =
      isolate->factory()->NewStringFromAscii(CStrVector("URL"));
  const char** urls = NewArray<const char*>(enumerator.count());
  for (int i = 0, l = enumerator.count(); i < l; ++i) {
    urls[i] = NULL;
    HandleScope scope;
    Handle<JSGlobalObject> global_obj = enumerator.at(i);
    Object* obj_document;
    if (global_obj->GetProperty(*document_string)->ToObject(&obj_document) &&
       obj_document->IsJSObject()) {
      JSObject* document = JSObject::cast(obj_document);
      Object* obj_url;
      if (document->GetProperty(*url_string)->ToObject(&obj_url) &&
          obj_url->IsString()) {
        urls[i] = collection_->names()->GetName(String::cast(obj_url));
      }
    }
  }

  AssertNoAllocation no_allocation;
  for (int i = 0, l = enumerator.count(); i < l; ++i) {
    objects_tags_.SetTag(*enumerator.at(i), urls[i]);
  }

  DeleteArray(urls);
}


class GlobalHandlesExtractor : public ObjectVisitor {
 public:
  explicit GlobalHandlesExtractor(NativeObjectsExplorer* explorer)
      : explorer_(explorer) {}
  virtual ~GlobalHandlesExtractor() {}
  virtual void VisitPointers(Object** start, Object** end) {
    UNREACHABLE();
  }
  virtual void VisitEmbedderReference(Object** p, uint16_t class_id) {
    explorer_->VisitSubtreeWrapper(p, class_id);
  }
 private:
  NativeObjectsExplorer* explorer_;
};


class BasicHeapEntriesAllocator : public HeapEntriesAllocator {
 public:
  BasicHeapEntriesAllocator(
      HeapSnapshot* snapshot,
      HeapEntry::Type entries_type)
    : snapshot_(snapshot),
      collection_(snapshot_->collection()),
      entries_type_(entries_type) {
  }
  virtual HeapEntry* AllocateEntry(
      HeapThing ptr, int children_count, int retainers_count);
 private:
  HeapSnapshot* snapshot_;
  HeapSnapshotsCollection* collection_;
  HeapEntry::Type entries_type_;
};


HeapEntry* BasicHeapEntriesAllocator::AllocateEntry(
    HeapThing ptr, int children_count, int retainers_count) {
  v8::RetainedObjectInfo* info = reinterpret_cast<v8::RetainedObjectInfo*>(ptr);
  intptr_t elements = info->GetElementCount();
  intptr_t size = info->GetSizeInBytes();
  return snapshot_->AddEntry(
      entries_type_,
      elements != -1 ?
          collection_->names()->GetFormatted(
              "%s / %" V8_PTR_PREFIX "d entries",
              info->GetLabel(),
              info->GetElementCount()) :
          collection_->names()->GetCopy(info->GetLabel()),
      HeapObjectsMap::GenerateId(info),
      size != -1 ? static_cast<int>(size) : 0,
      children_count,
      retainers_count);
}


NativeObjectsExplorer::NativeObjectsExplorer(
    HeapSnapshot* snapshot, SnapshottingProgressReportingInterface* progress)
    : snapshot_(snapshot),
      collection_(snapshot_->collection()),
      progress_(progress),
      embedder_queried_(false),
      objects_by_info_(RetainedInfosMatch),
      native_groups_(StringsMatch),
      filler_(NULL) {
  synthetic_entries_allocator_ =
      new BasicHeapEntriesAllocator(snapshot, HeapEntry::kSynthetic);
  native_entries_allocator_ =
      new BasicHeapEntriesAllocator(snapshot, HeapEntry::kNative);
}


NativeObjectsExplorer::~NativeObjectsExplorer() {
  for (HashMap::Entry* p = objects_by_info_.Start();
       p != NULL;
       p = objects_by_info_.Next(p)) {
    v8::RetainedObjectInfo* info =
        reinterpret_cast<v8::RetainedObjectInfo*>(p->key);
    info->Dispose();
    List<HeapObject*>* objects =
        reinterpret_cast<List<HeapObject*>* >(p->value);
    delete objects;
  }
  for (HashMap::Entry* p = native_groups_.Start();
       p != NULL;
       p = native_groups_.Next(p)) {
    v8::RetainedObjectInfo* info =
        reinterpret_cast<v8::RetainedObjectInfo*>(p->value);
    info->Dispose();
  }
  delete synthetic_entries_allocator_;
  delete native_entries_allocator_;
}


int NativeObjectsExplorer::EstimateObjectsCount() {
  FillRetainedObjects();
  return objects_by_info_.occupancy();
}


void NativeObjectsExplorer::FillRetainedObjects() {
  if (embedder_queried_) return;
  Isolate* isolate = Isolate::Current();
  // Record objects that are joined into ObjectGroups.
  isolate->heap()->CallGlobalGCPrologueCallback();
  List<ObjectGroup*>* groups = isolate->global_handles()->object_groups();
  for (int i = 0; i < groups->length(); ++i) {
    ObjectGroup* group = groups->at(i);
    if (group->info_ == NULL) continue;
    List<HeapObject*>* list = GetListMaybeDisposeInfo(group->info_);
    for (size_t j = 0; j < group->length_; ++j) {
      HeapObject* obj = HeapObject::cast(*group->objects_[j]);
      list->Add(obj);
      in_groups_.Insert(obj);
    }
    group->info_ = NULL;  // Acquire info object ownership.
  }
  isolate->global_handles()->RemoveObjectGroups();
  isolate->heap()->CallGlobalGCEpilogueCallback();
  // Record objects that are not in ObjectGroups, but have class ID.
  GlobalHandlesExtractor extractor(this);
  isolate->global_handles()->IterateAllRootsWithClassIds(&extractor);
  embedder_queried_ = true;
}

void NativeObjectsExplorer::FillImplicitReferences() {
  Isolate* isolate = Isolate::Current();
  List<ImplicitRefGroup*>* groups =
      isolate->global_handles()->implicit_ref_groups();
  for (int i = 0; i < groups->length(); ++i) {
    ImplicitRefGroup* group = groups->at(i);
    HeapObject* parent = *group->parent_;
    HeapEntry* parent_entry =
        filler_->FindOrAddEntry(parent, native_entries_allocator_);
    ASSERT(parent_entry != NULL);
    Object*** children = group->children_;
    for (size_t j = 0; j < group->length_; ++j) {
      Object* child = *children[j];
      HeapEntry* child_entry =
          filler_->FindOrAddEntry(child, native_entries_allocator_);
      filler_->SetNamedReference(
          HeapGraphEdge::kInternal,
          parent, parent_entry,
          "native",
          child, child_entry);
    }
  }
}

List<HeapObject*>* NativeObjectsExplorer::GetListMaybeDisposeInfo(
    v8::RetainedObjectInfo* info) {
  HashMap::Entry* entry =
      objects_by_info_.Lookup(info, InfoHash(info), true);
  if (entry->value != NULL) {
    info->Dispose();
  } else {
    entry->value = new List<HeapObject*>(4);
  }
  return reinterpret_cast<List<HeapObject*>* >(entry->value);
}


bool NativeObjectsExplorer::IterateAndExtractReferences(
    SnapshotFillerInterface* filler) {
  filler_ = filler;
  FillRetainedObjects();
  FillImplicitReferences();
  if (EstimateObjectsCount() > 0) {
    for (HashMap::Entry* p = objects_by_info_.Start();
         p != NULL;
         p = objects_by_info_.Next(p)) {
      v8::RetainedObjectInfo* info =
          reinterpret_cast<v8::RetainedObjectInfo*>(p->key);
      SetNativeRootReference(info);
      List<HeapObject*>* objects =
          reinterpret_cast<List<HeapObject*>* >(p->value);
      for (int i = 0; i < objects->length(); ++i) {
        SetWrapperNativeReferences(objects->at(i), info);
      }
    }
    SetRootNativeRootsReference();
  }
  filler_ = NULL;
  return true;
}


class NativeGroupRetainedObjectInfo : public v8::RetainedObjectInfo {
 public:
  explicit NativeGroupRetainedObjectInfo(const char* label)
      : disposed_(false),
        hash_(reinterpret_cast<intptr_t>(label)),
        label_(label) {
  }

  virtual ~NativeGroupRetainedObjectInfo() {}
  virtual void Dispose() {
    CHECK(!disposed_);
    disposed_ = true;
    delete this;
  }
  virtual bool IsEquivalent(RetainedObjectInfo* other) {
    return hash_ == other->GetHash() && !strcmp(label_, other->GetLabel());
  }
  virtual intptr_t GetHash() { return hash_; }
  virtual const char* GetLabel() { return label_; }

 private:
  bool disposed_;
  intptr_t hash_;
  const char* label_;
};


NativeGroupRetainedObjectInfo* NativeObjectsExplorer::FindOrAddGroupInfo(
    const char* label) {
  const char* label_copy = collection_->names()->GetCopy(label);
  uint32_t hash = HashSequentialString(label_copy,
                                       static_cast<int>(strlen(label_copy)),
                                       HEAP->HashSeed());
  HashMap::Entry* entry = native_groups_.Lookup(const_cast<char*>(label_copy),
                                                hash, true);
  if (entry->value == NULL)
    entry->value = new NativeGroupRetainedObjectInfo(label);
  return static_cast<NativeGroupRetainedObjectInfo*>(entry->value);
}


void NativeObjectsExplorer::SetNativeRootReference(
    v8::RetainedObjectInfo* info) {
  HeapEntry* child_entry =
      filler_->FindOrAddEntry(info, native_entries_allocator_);
  ASSERT(child_entry != NULL);
  NativeGroupRetainedObjectInfo* group_info =
      FindOrAddGroupInfo(info->GetGroupLabel());
  HeapEntry* group_entry =
      filler_->FindOrAddEntry(group_info, synthetic_entries_allocator_);
  filler_->SetNamedAutoIndexReference(
      HeapGraphEdge::kInternal,
      group_info, group_entry,
      info, child_entry);
}


void NativeObjectsExplorer::SetWrapperNativeReferences(
    HeapObject* wrapper, v8::RetainedObjectInfo* info) {
  HeapEntry* wrapper_entry = filler_->FindEntry(wrapper);
  ASSERT(wrapper_entry != NULL);
  HeapEntry* info_entry =
      filler_->FindOrAddEntry(info, native_entries_allocator_);
  ASSERT(info_entry != NULL);
  filler_->SetNamedReference(HeapGraphEdge::kInternal,
                             wrapper, wrapper_entry,
                             "native",
                             info, info_entry);
  filler_->SetIndexedAutoIndexReference(HeapGraphEdge::kElement,
                                        info, info_entry,
                                        wrapper, wrapper_entry);
}


void NativeObjectsExplorer::SetRootNativeRootsReference() {
  for (HashMap::Entry* entry = native_groups_.Start();
       entry;
       entry = native_groups_.Next(entry)) {
    NativeGroupRetainedObjectInfo* group_info =
        static_cast<NativeGroupRetainedObjectInfo*>(entry->value);
    HeapEntry* group_entry =
        filler_->FindOrAddEntry(group_info, native_entries_allocator_);
    ASSERT(group_entry != NULL);
    filler_->SetIndexedAutoIndexReference(
        HeapGraphEdge::kElement,
        V8HeapExplorer::kInternalRootObject, snapshot_->root(),
        group_info, group_entry);
  }
}


void NativeObjectsExplorer::VisitSubtreeWrapper(Object** p, uint16_t class_id) {
  if (in_groups_.Contains(*p)) return;
  Isolate* isolate = Isolate::Current();
  v8::RetainedObjectInfo* info =
      isolate->heap_profiler()->ExecuteWrapperClassCallback(class_id, p);
  if (info == NULL) return;
  GetListMaybeDisposeInfo(info)->Add(HeapObject::cast(*p));
}


class SnapshotCounter : public SnapshotFillerInterface {
 public:
  explicit SnapshotCounter(HeapEntriesMap* entries) : entries_(entries) { }
  HeapEntry* AddEntry(HeapThing ptr, HeapEntriesAllocator* allocator) {
    entries_->Pair(ptr, allocator, HeapEntriesMap::kHeapEntryPlaceholder);
    return HeapEntriesMap::kHeapEntryPlaceholder;
  }
  HeapEntry* FindEntry(HeapThing ptr) {
    return entries_->Map(ptr);
  }
  HeapEntry* FindOrAddEntry(HeapThing ptr, HeapEntriesAllocator* allocator) {
    HeapEntry* entry = FindEntry(ptr);
    return entry != NULL ? entry : AddEntry(ptr, allocator);
  }
  void SetIndexedReference(HeapGraphEdge::Type,
                           HeapThing parent_ptr,
                           HeapEntry*,
                           int,
                           HeapThing child_ptr,
                           HeapEntry*) {
    entries_->CountReference(parent_ptr, child_ptr);
  }
  void SetIndexedAutoIndexReference(HeapGraphEdge::Type,
                                    HeapThing parent_ptr,
                                    HeapEntry*,
                                    HeapThing child_ptr,
                                    HeapEntry*) {
    entries_->CountReference(parent_ptr, child_ptr);
  }
  void SetNamedReference(HeapGraphEdge::Type,
                         HeapThing parent_ptr,
                         HeapEntry*,
                         const char*,
                         HeapThing child_ptr,
                         HeapEntry*) {
    entries_->CountReference(parent_ptr, child_ptr);
  }
  void SetNamedAutoIndexReference(HeapGraphEdge::Type,
                                  HeapThing parent_ptr,
                                  HeapEntry*,
                                  HeapThing child_ptr,
                                  HeapEntry*) {
    entries_->CountReference(parent_ptr, child_ptr);
  }

 private:
  HeapEntriesMap* entries_;
};


class SnapshotFiller : public SnapshotFillerInterface {
 public:
  explicit SnapshotFiller(HeapSnapshot* snapshot, HeapEntriesMap* entries)
      : snapshot_(snapshot),
        collection_(snapshot->collection()),
        entries_(entries) { }
  HeapEntry* AddEntry(HeapThing ptr, HeapEntriesAllocator* allocator) {
    UNREACHABLE();
    return NULL;
  }
  HeapEntry* FindEntry(HeapThing ptr) {
    return entries_->Map(ptr);
  }
  HeapEntry* FindOrAddEntry(HeapThing ptr, HeapEntriesAllocator* allocator) {
    HeapEntry* entry = FindEntry(ptr);
    return entry != NULL ? entry : AddEntry(ptr, allocator);
  }
  void SetIndexedReference(HeapGraphEdge::Type type,
                           HeapThing parent_ptr,
                           HeapEntry* parent_entry,
                           int index,
                           HeapThing child_ptr,
                           HeapEntry* child_entry) {
    int child_index, retainer_index;
    entries_->CountReference(
        parent_ptr, child_ptr, &child_index, &retainer_index);
    parent_entry->SetIndexedReference(
        type, child_index, index, child_entry, retainer_index);
  }
  void SetIndexedAutoIndexReference(HeapGraphEdge::Type type,
                                    HeapThing parent_ptr,
                                    HeapEntry* parent_entry,
                                    HeapThing child_ptr,
                                    HeapEntry* child_entry) {
    int child_index, retainer_index;
    entries_->CountReference(
        parent_ptr, child_ptr, &child_index, &retainer_index);
    parent_entry->SetIndexedReference(
        type, child_index, child_index + 1, child_entry, retainer_index);
  }
  void SetNamedReference(HeapGraphEdge::Type type,
                         HeapThing parent_ptr,
                         HeapEntry* parent_entry,
                         const char* reference_name,
                         HeapThing child_ptr,
                         HeapEntry* child_entry) {
    int child_index, retainer_index;
    entries_->CountReference(
        parent_ptr, child_ptr, &child_index, &retainer_index);
    parent_entry->SetNamedReference(
        type, child_index, reference_name, child_entry, retainer_index);
  }
  void SetNamedAutoIndexReference(HeapGraphEdge::Type type,
                                  HeapThing parent_ptr,
                                  HeapEntry* parent_entry,
                                  HeapThing child_ptr,
                                  HeapEntry* child_entry) {
    int child_index, retainer_index;
    entries_->CountReference(
        parent_ptr, child_ptr, &child_index, &retainer_index);
    parent_entry->SetNamedReference(type,
                              child_index,
                              collection_->names()->GetName(child_index + 1),
                              child_entry,
                              retainer_index);
  }

 private:
  HeapSnapshot* snapshot_;
  HeapSnapshotsCollection* collection_;
  HeapEntriesMap* entries_;
};


HeapSnapshotGenerator::HeapSnapshotGenerator(HeapSnapshot* snapshot,
                                             v8::ActivityControl* control)
    : snapshot_(snapshot),
      control_(control),
      v8_heap_explorer_(snapshot_, this),
      dom_explorer_(snapshot_, this) {
}


bool HeapSnapshotGenerator::GenerateSnapshot() {
  v8_heap_explorer_.TagGlobalObjects();

  // TODO(1562) Profiler assumes that any object that is in the heap after
  // full GC is reachable from the root when computing dominators.
  // This is not true for weakly reachable objects.
  // As a temporary solution we call GC twice.
  Isolate::Current()->heap()->CollectAllGarbage(
      Heap::kMakeHeapIterableMask,
      "HeapSnapshotGenerator::GenerateSnapshot");
  Isolate::Current()->heap()->CollectAllGarbage(
      Heap::kMakeHeapIterableMask,
      "HeapSnapshotGenerator::GenerateSnapshot");

#ifdef DEBUG
  Heap* debug_heap = Isolate::Current()->heap();
  ASSERT(!debug_heap->old_data_space()->was_swept_conservatively());
  ASSERT(!debug_heap->old_pointer_space()->was_swept_conservatively());
  ASSERT(!debug_heap->code_space()->was_swept_conservatively());
  ASSERT(!debug_heap->cell_space()->was_swept_conservatively());
  ASSERT(!debug_heap->map_space()->was_swept_conservatively());
#endif

  // The following code uses heap iterators, so we want the heap to be
  // stable. It should follow TagGlobalObjects as that can allocate.
  AssertNoAllocation no_alloc;

#ifdef DEBUG
  debug_heap->Verify();
#endif

  SetProgressTotal(2);  // 2 passes.

#ifdef DEBUG
  debug_heap->Verify();
#endif

  // Pass 1. Iterate heap contents to count entries and references.
  if (!CountEntriesAndReferences()) return false;

#ifdef DEBUG
  debug_heap->Verify();
#endif

  // Allocate memory for entries and references.
  snapshot_->AllocateEntries(entries_.entries_count(),
                             entries_.total_children_count(),
                             entries_.total_retainers_count());

  // Allocate heap objects to entries hash map.
  entries_.AllocateEntries();

  // Pass 2. Fill references.
  if (!FillReferences()) return false;

  if (!SetEntriesDominators()) return false;
  if (!CalculateRetainedSizes()) return false;

  progress_counter_ = progress_total_;
  if (!ProgressReport(true)) return false;
  return true;
}


void HeapSnapshotGenerator::ProgressStep() {
  ++progress_counter_;
}


bool HeapSnapshotGenerator::ProgressReport(bool force) {
  const int kProgressReportGranularity = 10000;
  if (control_ != NULL
      && (force || progress_counter_ % kProgressReportGranularity == 0)) {
      return
          control_->ReportProgressValue(progress_counter_, progress_total_) ==
          v8::ActivityControl::kContinue;
  }
  return true;
}


void HeapSnapshotGenerator::SetProgressTotal(int iterations_count) {
  if (control_ == NULL) return;
  HeapIterator iterator(HeapIterator::kFilterUnreachable);
  progress_total_ = (
      v8_heap_explorer_.EstimateObjectsCount(&iterator) +
      dom_explorer_.EstimateObjectsCount()) * iterations_count;
  progress_counter_ = 0;
}


bool HeapSnapshotGenerator::CountEntriesAndReferences() {
  SnapshotCounter counter(&entries_);
  v8_heap_explorer_.AddRootEntries(&counter);
  return v8_heap_explorer_.IterateAndExtractReferences(&counter)
      && dom_explorer_.IterateAndExtractReferences(&counter);
}


bool HeapSnapshotGenerator::FillReferences() {
  SnapshotFiller filler(snapshot_, &entries_);
  // IterateAndExtractReferences cannot set object names because
  // it makes call to JSObject::LocalLookupRealNamedProperty which
  // in turn may relocate objects in property maps thus changing the heap
  // layout and affecting retainer counts. This is not acceptable because
  // number of retainers must not change between count and fill passes.
  // To avoid this there's a separate postpass that set object names.
  return v8_heap_explorer_.IterateAndExtractReferences(&filler)
      && dom_explorer_.IterateAndExtractReferences(&filler)
      && v8_heap_explorer_.IterateAndSetObjectNames(&filler);
}


void HeapSnapshotGenerator::FillReversePostorderIndexes(
    Vector<HeapEntry*>* entries) {
  snapshot_->ClearPaint();
  int current_entry = 0;
  List<HeapEntry*> nodes_to_visit;
  nodes_to_visit.Add(snapshot_->root());
  snapshot_->root()->paint();
  while (!nodes_to_visit.is_empty()) {
    HeapEntry* entry = nodes_to_visit.last();
    Vector<HeapGraphEdge> children = entry->children();
    bool has_new_edges = false;
    for (int i = 0; i < children.length(); ++i) {
      if (children[i].type() == HeapGraphEdge::kShortcut) continue;
      HeapEntry* child = children[i].to();
      if (!child->painted()) {
        nodes_to_visit.Add(child);
        child->paint();
        has_new_edges = true;
      }
    }
    if (!has_new_edges) {
      entry->set_ordered_index(current_entry);
      (*entries)[current_entry++] = entry;
      nodes_to_visit.RemoveLast();
    }
  }
  ASSERT_EQ(current_entry, entries->length());
}


static int Intersect(int i1, int i2, const Vector<int>& dominators) {
  int finger1 = i1, finger2 = i2;
  while (finger1 != finger2) {
    while (finger1 < finger2) finger1 = dominators[finger1];
    while (finger2 < finger1) finger2 = dominators[finger2];
  }
  return finger1;
}


// The algorithm is based on the article:
// K. Cooper, T. Harvey and K. Kennedy "A Simple, Fast Dominance Algorithm"
// Softw. Pract. Exper. 4 (2001), pp. 1-10.
bool HeapSnapshotGenerator::BuildDominatorTree(
    const Vector<HeapEntry*>& entries,
    Vector<int>* dominators) {
  if (entries.length() == 0) return true;
  const int entries_length = entries.length(), root_index = entries_length - 1;
  static const int kNoDominator = -1;
  for (int i = 0; i < root_index; ++i) (*dominators)[i] = kNoDominator;
  (*dominators)[root_index] = root_index;

  // The affected array is used to mark entries which dominators
  // have to be racalculated because of changes in their retainers.
  ScopedVector<bool> affected(entries_length);
  for (int i = 0; i < affected.length(); ++i) affected[i] = false;
  // Mark the root direct children as affected.
  Vector<HeapGraphEdge> children = entries[root_index]->children();
  for (int i = 0; i < children.length(); ++i) {
    affected[children[i].to()->ordered_index()] = true;
  }

  bool changed = true;
  while (changed) {
    changed = false;
    if (!ProgressReport(true)) return false;
    for (int i = root_index - 1; i >= 0; --i) {
      if (!affected[i]) continue;
      affected[i] = false;
      // If dominator of the entry has already been set to root,
      // then it can't propagate any further.
      if ((*dominators)[i] == root_index) continue;
      int new_idom_index = kNoDominator;
      Vector<HeapGraphEdge*> rets = entries[i]->retainers();
      for (int j = 0; j < rets.length(); ++j) {
        if (rets[j]->type() == HeapGraphEdge::kShortcut) continue;
        int ret_index = rets[j]->From()->ordered_index();
        if (dominators->at(ret_index) != kNoDominator) {
          new_idom_index = new_idom_index == kNoDominator
              ? ret_index
              : Intersect(ret_index, new_idom_index, *dominators);
          // If idom has already reached the root, it doesn't make sense
          // to check other retainers.
          if (new_idom_index == root_index) break;
        }
      }
      if (new_idom_index != kNoDominator
          && dominators->at(i) != new_idom_index) {
        (*dominators)[i] = new_idom_index;
        changed = true;
        Vector<HeapGraphEdge> children = entries[i]->children();
        for (int j = 0; j < children.length(); ++j) {
          affected[children[j].to()->ordered_index()] = true;
        }
      }
    }
  }
  return true;
}


bool HeapSnapshotGenerator::SetEntriesDominators() {
  // This array is used for maintaining reverse postorder of nodes.
  ScopedVector<HeapEntry*> ordered_entries(snapshot_->entries()->length());
  FillReversePostorderIndexes(&ordered_entries);
  ScopedVector<int> dominators(ordered_entries.length());
  if (!BuildDominatorTree(ordered_entries, &dominators)) return false;
  for (int i = 0; i < ordered_entries.length(); ++i) {
    ASSERT(dominators[i] >= 0);
    ordered_entries[i]->set_dominator(ordered_entries[dominators[i]]);
  }
  return true;
}


bool HeapSnapshotGenerator::CalculateRetainedSizes() {
  // As for the dominators tree we only know parent nodes, not
  // children, to sum up total sizes we "bubble" node's self size
  // adding it to all of its parents.
  List<HeapEntry*>& entries = *snapshot_->entries();
  for (int i = 0; i < entries.length(); ++i) {
    HeapEntry* entry = entries[i];
    entry->set_retained_size(entry->self_size());
  }
  for (int i = 0; i < entries.length(); ++i) {
    HeapEntry* entry = entries[i];
    int entry_size = entry->self_size();
    for (HeapEntry* dominator = entry->dominator();
         dominator != entry;
         entry = dominator, dominator = entry->dominator()) {
      dominator->add_retained_size(entry_size);
    }
  }
  return true;
}


template<int bytes> struct MaxDecimalDigitsIn;
template<> struct MaxDecimalDigitsIn<4> {
  static const int kSigned = 11;
  static const int kUnsigned = 10;
};
template<> struct MaxDecimalDigitsIn<8> {
  static const int kSigned = 20;
  static const int kUnsigned = 20;
};


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
    // Buffer for the longest value plus trailing \0
    static const int kMaxNumberSize =
        MaxDecimalDigitsIn<sizeof(T)>::kUnsigned + 1;
    if (chunk_size_ - chunk_pos_ >= kMaxNumberSize) {
      int result = OS::SNPrintF(
          chunk_.SubVector(chunk_pos_, chunk_size_), format, n);
      ASSERT(result != -1);
      chunk_pos_ += result;
      MaybeWriteChunk();
    } else {
      EmbeddedVector<char, kMaxNumberSize> buffer;
      int result = OS::SNPrintF(buffer, format, n);
      USE(result);
      ASSERT(result != -1);
      AddString(buffer.start());
    }
  }
  void MaybeWriteChunk() {
    ASSERT(chunk_pos_ <= chunk_size_);
    if (chunk_pos_ == chunk_size_) {
      WriteChunk();
    }
  }
  void WriteChunk() {
    if (aborted_) return;
    if (stream_->WriteAsciiChunk(chunk_.start(), chunk_pos_) ==
        v8::OutputStream::kAbort) aborted_ = true;
    chunk_pos_ = 0;
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

  HeapSnapshot* original_snapshot = NULL;
  if (snapshot_->raw_entries_size() >=
      SnapshotSizeConstants<kPointerSize>::kMaxSerializableSnapshotRawSize) {
    // The snapshot is too big. Serialize a fake snapshot.
    original_snapshot = snapshot_;
    snapshot_ = CreateFakeSnapshot();
  }
  // Since nodes graph is cyclic, we need the first pass to enumerate
  // them. Strings can be serialized in one pass.
  EnumerateNodes();
  SerializeImpl();

  delete writer_;
  writer_ = NULL;

  if (original_snapshot != NULL) {
    delete snapshot_;
    snapshot_ = original_snapshot;
  }
}


HeapSnapshot* HeapSnapshotJSONSerializer::CreateFakeSnapshot() {
  HeapSnapshot* result = new HeapSnapshot(snapshot_->collection(),
                                          HeapSnapshot::kFull,
                                          snapshot_->title(),
                                          snapshot_->uid());
  result->AllocateEntries(2, 1, 0);
  HeapEntry* root = result->AddRootEntry(1);
  const char* text = snapshot_->collection()->names()->GetFormatted(
      "The snapshot is too big. "
      "Maximum snapshot size is %"  V8_PTR_PREFIX "u MB. "
      "Actual snapshot size is %"  V8_PTR_PREFIX "u MB.",
      SnapshotSizeConstants<kPointerSize>::kMaxSerializableSnapshotRawSize / MB,
      (snapshot_->raw_entries_size() + MB - 1) / MB);
  HeapEntry* message = result->AddEntry(
      HeapEntry::kString, text, 0, 4, 0, 0);
  root->SetUnidirElementReference(0, 1, message);
  result->SetDominatorsToSelf();
  return result;
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
  // The buffer needs space for 3 ints, 3 commas and \0
  static const int kBufferSize =
      MaxDecimalDigitsIn<sizeof(int)>::kSigned * 3 + 3 + 1;  // NOLINT
  EmbeddedVector<char, kBufferSize> buffer;
  int edge_name_or_index = edge->type() == HeapGraphEdge::kElement
      || edge->type() == HeapGraphEdge::kHidden
      || edge->type() == HeapGraphEdge::kWeak
      ? edge->index() : GetStringId(edge->name());
  STATIC_CHECK(sizeof(int) == sizeof(edge->type()));  // NOLINT
  STATIC_CHECK(sizeof(int) == sizeof(edge_name_or_index));  // NOLINT
  STATIC_CHECK(sizeof(int) == sizeof(GetNodeId(edge->to())));  // NOLINT
  int result = OS::SNPrintF(buffer, ",%d,%d,%d",
      edge->type(), edge_name_or_index, GetNodeId(edge->to()));
  USE(result);
  ASSERT(result != -1);
  writer_->AddString(buffer.start());
}


void HeapSnapshotJSONSerializer::SerializeNode(HeapEntry* entry) {
  // The buffer needs space for 6 ints, 1 uint32_t, 7 commas, \n and \0
  static const int kBufferSize =
      6 * MaxDecimalDigitsIn<sizeof(int)>::kSigned  // NOLINT
      + MaxDecimalDigitsIn<sizeof(uint32_t)>::kUnsigned  // NOLINT
      + 7 + 1 + 1;
  EmbeddedVector<char, kBufferSize> buffer;
  Vector<HeapGraphEdge> children = entry->children();
  STATIC_CHECK(sizeof(int) == sizeof(entry->type()));  // NOLINT
  STATIC_CHECK(sizeof(int) == sizeof(GetStringId(entry->name())));  // NOLINT
  STATIC_CHECK(sizeof(unsigned) == sizeof(entry->id()));  // NOLINT
  STATIC_CHECK(sizeof(int) == sizeof(entry->self_size()));  // NOLINT
  STATIC_CHECK(sizeof(int) == sizeof(entry->retained_size()));  // NOLINT
  STATIC_CHECK(sizeof(int) == sizeof(GetNodeId(entry->dominator())));  // NOLINT
  STATIC_CHECK(sizeof(int) == sizeof(children.length()));  // NOLINT
  int result = OS::SNPrintF(buffer, "\n,%d,%d,%u,%d,%d,%d,%d",
      entry->type(),
      GetStringId(entry->name()),
      entry->id(),
      entry->self_size(),
      entry->retained_size(),
      GetNodeId(entry->dominator()),
      children.length());
  USE(result);
  ASSERT(result != -1);
  writer_->AddString(buffer.start());
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
            "," JSON_S("number")
            "," JSON_S("native")
            "," JSON_S("synthetic"))
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
                    "," JSON_S("shortcut")
                    "," JSON_S("weak"))
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

} }  // namespace v8::internal
