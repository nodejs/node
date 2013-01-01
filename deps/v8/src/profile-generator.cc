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
#include "debug.h"

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


size_t StringsStorage::GetUsedMemorySize() const {
  size_t size = sizeof(*this);
  size += sizeof(HashMap::Entry) * names_.capacity();
  for (HashMap::Entry* p = names_.Start(); p != NULL; p = names_.Next(p)) {
    size += strlen(reinterpret_cast<const char*>(p->value)) + 1;
  }
  return size;
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


HeapGraphEdge::HeapGraphEdge(Type type, const char* name, int from, int to)
    : type_(type),
      from_index_(from),
      to_index_(to),
      name_(name) {
  ASSERT(type == kContextVariable
      || type == kProperty
      || type == kInternal
      || type == kShortcut);
}


HeapGraphEdge::HeapGraphEdge(Type type, int index, int from, int to)
    : type_(type),
      from_index_(from),
      to_index_(to),
      index_(index) {
  ASSERT(type == kElement || type == kHidden || type == kWeak);
}


void HeapGraphEdge::ReplaceToIndexWithEntry(HeapSnapshot* snapshot) {
  to_entry_ = &snapshot->entries()[to_index_];
}


const int HeapEntry::kNoEntry = -1;

HeapEntry::HeapEntry(HeapSnapshot* snapshot,
                     Type type,
                     const char* name,
                     SnapshotObjectId id,
                     int self_size)
    : type_(type),
      children_count_(0),
      children_index_(-1),
      self_size_(self_size),
      id_(id),
      snapshot_(snapshot),
      name_(name) { }


void HeapEntry::SetNamedReference(HeapGraphEdge::Type type,
                                  const char* name,
                                  HeapEntry* entry) {
  HeapGraphEdge edge(type, name, this->index(), entry->index());
  snapshot_->edges().Add(edge);
  ++children_count_;
}


void HeapEntry::SetIndexedReference(HeapGraphEdge::Type type,
                                    int index,
                                    HeapEntry* entry) {
  HeapGraphEdge edge(type, index, this->index(), entry->index());
  snapshot_->edges().Add(edge);
  ++children_count_;
}


Handle<HeapObject> HeapEntry::GetHeapObject() {
  return snapshot_->collection()->FindHeapObjectById(id());
}


void HeapEntry::Print(
    const char* prefix, const char* edge_name, int max_depth, int indent) {
  STATIC_CHECK(sizeof(unsigned) == sizeof(id()));
  OS::Print("%6d @%6u %*c %s%s: ",
            self_size(), id(), indent, ' ', prefix, edge_name);
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
  Vector<HeapGraphEdge*> ch = children();
  for (int i = 0; i < ch.length(); ++i) {
    HeapGraphEdge& edge = *ch[i];
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


// It is very important to keep objects that form a heap snapshot
// as small as possible.
namespace {  // Avoid littering the global namespace.

template <size_t ptr_size> struct SnapshotSizeConstants;

template <> struct SnapshotSizeConstants<4> {
  static const int kExpectedHeapGraphEdgeSize = 12;
  static const int kExpectedHeapEntrySize = 24;
  static const int kExpectedHeapSnapshotsCollectionSize = 96;
  static const int kExpectedHeapSnapshotSize = 136;
  static const size_t kMaxSerializableSnapshotRawSize = 256 * MB;
};

template <> struct SnapshotSizeConstants<8> {
  static const int kExpectedHeapGraphEdgeSize = 24;
  static const int kExpectedHeapEntrySize = 32;
  static const int kExpectedHeapSnapshotsCollectionSize = 144;
  static const int kExpectedHeapSnapshotSize = 168;
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
      root_index_(HeapEntry::kNoEntry),
      gc_roots_index_(HeapEntry::kNoEntry),
      natives_root_index_(HeapEntry::kNoEntry),
      max_snapshot_js_object_id_(0) {
  STATIC_CHECK(
      sizeof(HeapGraphEdge) ==
      SnapshotSizeConstants<kPointerSize>::kExpectedHeapGraphEdgeSize);
  STATIC_CHECK(
      sizeof(HeapEntry) ==
      SnapshotSizeConstants<kPointerSize>::kExpectedHeapEntrySize);
  for (int i = 0; i < VisitorSynchronization::kNumberOfSyncTags; ++i) {
    gc_subroot_indexes_[i] = HeapEntry::kNoEntry;
  }
}


void HeapSnapshot::Delete() {
  collection_->RemoveSnapshot(this);
  delete this;
}


void HeapSnapshot::RememberLastJSObjectId() {
  max_snapshot_js_object_id_ = collection_->last_assigned_id();
}


HeapEntry* HeapSnapshot::AddRootEntry() {
  ASSERT(root_index_ == HeapEntry::kNoEntry);
  ASSERT(entries_.is_empty());  // Root entry must be the first one.
  HeapEntry* entry = AddEntry(HeapEntry::kObject,
                              "",
                              HeapObjectsMap::kInternalRootObjectId,
                              0);
  root_index_ = entry->index();
  ASSERT(root_index_ == 0);
  return entry;
}


HeapEntry* HeapSnapshot::AddGcRootsEntry() {
  ASSERT(gc_roots_index_ == HeapEntry::kNoEntry);
  HeapEntry* entry = AddEntry(HeapEntry::kObject,
                              "(GC roots)",
                              HeapObjectsMap::kGcRootsObjectId,
                              0);
  gc_roots_index_ = entry->index();
  return entry;
}


HeapEntry* HeapSnapshot::AddGcSubrootEntry(int tag) {
  ASSERT(gc_subroot_indexes_[tag] == HeapEntry::kNoEntry);
  ASSERT(0 <= tag && tag < VisitorSynchronization::kNumberOfSyncTags);
  HeapEntry* entry = AddEntry(
      HeapEntry::kObject,
      VisitorSynchronization::kTagNames[tag],
      HeapObjectsMap::GetNthGcSubrootId(tag),
      0);
  gc_subroot_indexes_[tag] = entry->index();
  return entry;
}


HeapEntry* HeapSnapshot::AddEntry(HeapEntry::Type type,
                                  const char* name,
                                  SnapshotObjectId id,
                                  int size) {
  HeapEntry entry(this, type, name, id, size);
  entries_.Add(entry);
  return &entries_.last();
}


void HeapSnapshot::FillChildren() {
  ASSERT(children().is_empty());
  children().Allocate(edges().length());
  int children_index = 0;
  for (int i = 0; i < entries().length(); ++i) {
    HeapEntry* entry = &entries()[i];
    children_index = entry->set_children_index(children_index);
  }
  ASSERT(edges().length() == children_index);
  for (int i = 0; i < edges().length(); ++i) {
    HeapGraphEdge* edge = &edges()[i];
    edge->ReplaceToIndexWithEntry(this);
    edge->from()->add_child(edge);
  }
}


class FindEntryById {
 public:
  explicit FindEntryById(SnapshotObjectId id) : id_(id) { }
  int operator()(HeapEntry* const* entry) {
    if ((*entry)->id() == id_) return 0;
    return (*entry)->id() < id_ ? -1 : 1;
  }
 private:
  SnapshotObjectId id_;
};


HeapEntry* HeapSnapshot::GetEntryById(SnapshotObjectId id) {
  List<HeapEntry*>* entries_by_id = GetSortedEntriesList();
  // Perform a binary search by id.
  int index = SortedListBSearch(*entries_by_id, FindEntryById(id));
  if (index == -1)
    return NULL;
  return entries_by_id->at(index);
}


template<class T>
static int SortByIds(const T* entry1_ptr,
                     const T* entry2_ptr) {
  if ((*entry1_ptr)->id() == (*entry2_ptr)->id()) return 0;
  return (*entry1_ptr)->id() < (*entry2_ptr)->id() ? -1 : 1;
}


List<HeapEntry*>* HeapSnapshot::GetSortedEntriesList() {
  if (sorted_entries_.is_empty()) {
    sorted_entries_.Allocate(entries_.length());
    for (int i = 0; i < entries_.length(); ++i) {
      sorted_entries_[i] = &entries_[i];
    }
    sorted_entries_.Sort(SortByIds);
  }
  return &sorted_entries_;
}


void HeapSnapshot::Print(int max_depth) {
  root()->Print("", "", max_depth, 0);
}


template<typename T, class P>
static size_t GetMemoryUsedByList(const List<T, P>& list) {
  return list.length() * sizeof(T) + sizeof(list);
}


size_t HeapSnapshot::RawSnapshotSize() const {
  STATIC_CHECK(SnapshotSizeConstants<kPointerSize>::kExpectedHeapSnapshotSize ==
      sizeof(HeapSnapshot));  // NOLINT
  return
      sizeof(*this) +
      GetMemoryUsedByList(entries_) +
      GetMemoryUsedByList(edges_) +
      GetMemoryUsedByList(children_) +
      GetMemoryUsedByList(sorted_entries_);
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
    : next_id_(kFirstAvailableObjectId),
      entries_map_(AddressesMatch) {
  // This dummy element solves a problem with entries_map_.
  // When we do lookup in HashMap we see no difference between two cases:
  // it has an entry with NULL as the value or it has created
  // a new entry on the fly with NULL as the default value.
  // With such dummy element we have a guaranty that all entries_map_ entries
  // will have the value field grater than 0.
  // This fact is using in MoveObject method.
  entries_.Add(EntryInfo(0, NULL, 0));
}


void HeapObjectsMap::SnapshotGenerationFinished() {
  RemoveDeadEntries();
}


void HeapObjectsMap::MoveObject(Address from, Address to) {
  ASSERT(to != NULL);
  ASSERT(from != NULL);
  if (from == to) return;
  void* from_value = entries_map_.Remove(from, AddressHash(from));
  if (from_value == NULL) return;
  int from_entry_info_index =
      static_cast<int>(reinterpret_cast<intptr_t>(from_value));
  entries_.at(from_entry_info_index).addr = to;
  HashMap::Entry* to_entry = entries_map_.Lookup(to, AddressHash(to), true);
  if (to_entry->value != NULL) {
    int to_entry_info_index =
        static_cast<int>(reinterpret_cast<intptr_t>(to_entry->value));
    // Without this operation we will have two EntryInfo's with the same
    // value in addr field. It is bad because later at RemoveDeadEntries
    // one of this entry will be removed with the corresponding entries_map_
    // entry.
    entries_.at(to_entry_info_index).addr = NULL;
  }
  to_entry->value = reinterpret_cast<void*>(from_entry_info_index);
}


SnapshotObjectId HeapObjectsMap::FindEntry(Address addr) {
  HashMap::Entry* entry = entries_map_.Lookup(addr, AddressHash(addr), false);
  if (entry == NULL) return 0;
  int entry_index = static_cast<int>(reinterpret_cast<intptr_t>(entry->value));
  EntryInfo& entry_info = entries_.at(entry_index);
  ASSERT(static_cast<uint32_t>(entries_.length()) > entries_map_.occupancy());
  return entry_info.id;
}


SnapshotObjectId HeapObjectsMap::FindOrAddEntry(Address addr,
                                                unsigned int size) {
  ASSERT(static_cast<uint32_t>(entries_.length()) > entries_map_.occupancy());
  HashMap::Entry* entry = entries_map_.Lookup(addr, AddressHash(addr), true);
  if (entry->value != NULL) {
    int entry_index =
        static_cast<int>(reinterpret_cast<intptr_t>(entry->value));
    EntryInfo& entry_info = entries_.at(entry_index);
    entry_info.accessed = true;
    entry_info.size = size;
    return entry_info.id;
  }
  entry->value = reinterpret_cast<void*>(entries_.length());
  SnapshotObjectId id = next_id_;
  next_id_ += kObjectIdStep;
  entries_.Add(EntryInfo(id, addr, size));
  ASSERT(static_cast<uint32_t>(entries_.length()) > entries_map_.occupancy());
  return id;
}


void HeapObjectsMap::StopHeapObjectsTracking() {
  time_intervals_.Clear();
}

void HeapObjectsMap::UpdateHeapObjectsMap() {
  HEAP->CollectAllGarbage(Heap::kMakeHeapIterableMask,
                          "HeapSnapshotsCollection::UpdateHeapObjectsMap");
  HeapIterator iterator;
  for (HeapObject* obj = iterator.next();
       obj != NULL;
       obj = iterator.next()) {
    FindOrAddEntry(obj->address(), obj->Size());
  }
  RemoveDeadEntries();
}


SnapshotObjectId HeapObjectsMap::PushHeapObjectsStats(OutputStream* stream) {
  UpdateHeapObjectsMap();
  time_intervals_.Add(TimeInterval(next_id_));
  int prefered_chunk_size = stream->GetChunkSize();
  List<v8::HeapStatsUpdate> stats_buffer;
  ASSERT(!entries_.is_empty());
  EntryInfo* entry_info = &entries_.first();
  EntryInfo* end_entry_info = &entries_.last() + 1;
  for (int time_interval_index = 0;
       time_interval_index < time_intervals_.length();
       ++time_interval_index) {
    TimeInterval& time_interval = time_intervals_[time_interval_index];
    SnapshotObjectId time_interval_id = time_interval.id;
    uint32_t entries_size = 0;
    EntryInfo* start_entry_info = entry_info;
    while (entry_info < end_entry_info && entry_info->id < time_interval_id) {
      entries_size += entry_info->size;
      ++entry_info;
    }
    uint32_t entries_count =
        static_cast<uint32_t>(entry_info - start_entry_info);
    if (time_interval.count != entries_count ||
        time_interval.size != entries_size) {
      stats_buffer.Add(v8::HeapStatsUpdate(
          time_interval_index,
          time_interval.count = entries_count,
          time_interval.size = entries_size));
      if (stats_buffer.length() >= prefered_chunk_size) {
        OutputStream::WriteResult result = stream->WriteHeapStatsChunk(
            &stats_buffer.first(), stats_buffer.length());
        if (result == OutputStream::kAbort) return last_assigned_id();
        stats_buffer.Clear();
      }
    }
  }
  ASSERT(entry_info == end_entry_info);
  if (!stats_buffer.is_empty()) {
    OutputStream::WriteResult result = stream->WriteHeapStatsChunk(
        &stats_buffer.first(), stats_buffer.length());
    if (result == OutputStream::kAbort) return last_assigned_id();
  }
  stream->EndOfStream();
  return last_assigned_id();
}


void HeapObjectsMap::RemoveDeadEntries() {
  ASSERT(entries_.length() > 0 &&
         entries_.at(0).id == 0 &&
         entries_.at(0).addr == NULL);
  int first_free_entry = 1;
  for (int i = 1; i < entries_.length(); ++i) {
    EntryInfo& entry_info = entries_.at(i);
    if (entry_info.accessed) {
      if (first_free_entry != i) {
        entries_.at(first_free_entry) = entry_info;
      }
      entries_.at(first_free_entry).accessed = false;
      HashMap::Entry* entry = entries_map_.Lookup(
          entry_info.addr, AddressHash(entry_info.addr), false);
      ASSERT(entry);
      entry->value = reinterpret_cast<void*>(first_free_entry);
      ++first_free_entry;
    } else {
      if (entry_info.addr) {
        entries_map_.Remove(entry_info.addr, AddressHash(entry_info.addr));
      }
    }
  }
  entries_.Rewind(first_free_entry);
  ASSERT(static_cast<uint32_t>(entries_.length()) - 1 ==
         entries_map_.occupancy());
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


size_t HeapObjectsMap::GetUsedMemorySize() const {
  return
      sizeof(*this) +
      sizeof(HashMap::Entry) * entries_map_.capacity() +
      GetMemoryUsedByList(entries_) +
      GetMemoryUsedByList(time_intervals_);
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
    if (ids_.FindEntry(obj->address()) == id) {
      ASSERT(object == NULL);
      object = obj;
      // Can't break -- kFilterUnreachable requires full heap traversal.
    }
  }
  return object != NULL ? Handle<HeapObject>(object) : Handle<HeapObject>();
}


size_t HeapSnapshotsCollection::GetUsedMemorySize() const {
  STATIC_CHECK(SnapshotSizeConstants<kPointerSize>::
      kExpectedHeapSnapshotsCollectionSize ==
      sizeof(HeapSnapshotsCollection));  // NOLINT
  size_t size = sizeof(*this);
  size += names_.GetUsedMemorySize();
  size += ids_.GetUsedMemorySize();
  size += sizeof(HashMap::Entry) * snapshots_uids_.capacity();
  size += GetMemoryUsedByList(snapshots_);
  for (int i = 0; i < snapshots_.length(); ++i) {
    size += snapshots_[i]->RawSnapshotSize();
  }
  return size;
}


HeapEntriesMap::HeapEntriesMap()
    : entries_(HeapThingsMatch) {
}


int HeapEntriesMap::Map(HeapThing thing) {
  HashMap::Entry* cache_entry = entries_.Lookup(thing, Hash(thing), false);
  if (cache_entry == NULL) return HeapEntry::kNoEntry;
  return static_cast<int>(reinterpret_cast<intptr_t>(cache_entry->value));
}


void HeapEntriesMap::Pair(HeapThing thing, int entry) {
  HashMap::Entry* cache_entry = entries_.Lookup(thing, Hash(thing), true);
  ASSERT(cache_entry->value == NULL);
  cache_entry->value = reinterpret_cast<void*>(static_cast<intptr_t>(entry));
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
  return entries_.Lookup(object, HeapEntriesMap::Hash(object), false) != NULL;
}


void HeapObjectsSet::Insert(Object* obj) {
  if (!obj->IsHeapObject()) return;
  HeapObject* object = HeapObject::cast(obj);
  entries_.Lookup(object, HeapEntriesMap::Hash(object), true);
}


const char* HeapObjectsSet::GetTag(Object* obj) {
  HeapObject* object = HeapObject::cast(obj);
  HashMap::Entry* cache_entry =
      entries_.Lookup(object, HeapEntriesMap::Hash(object), false);
  return cache_entry != NULL
      ? reinterpret_cast<const char*>(cache_entry->value)
      : NULL;
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
    SnapshottingProgressReportingInterface* progress,
    v8::HeapProfiler::ObjectNameResolver* resolver)
    : heap_(Isolate::Current()->heap()),
      snapshot_(snapshot),
      collection_(snapshot_->collection()),
      progress_(progress),
      filler_(NULL),
      global_object_name_resolver_(resolver) {
}


V8HeapExplorer::~V8HeapExplorer() {
}


HeapEntry* V8HeapExplorer::AllocateEntry(HeapThing ptr) {
  return AddEntry(reinterpret_cast<HeapObject*>(ptr));
}


HeapEntry* V8HeapExplorer::AddEntry(HeapObject* object) {
  if (object == kInternalRootObject) {
    snapshot_->AddRootEntry();
    return snapshot_->root();
  } else if (object == kGcRootsObject) {
    HeapEntry* entry = snapshot_->AddGcRootsEntry();
    return entry;
  } else if (object >= kFirstGcSubrootObject && object < kLastGcSubrootObject) {
    HeapEntry* entry = snapshot_->AddGcSubrootEntry(GetGcSubrootOrder(object));
    return entry;
  } else if (object->IsJSFunction()) {
    JSFunction* func = JSFunction::cast(object);
    SharedFunctionInfo* shared = func->shared();
    const char* name = shared->bound() ? "native_bind" :
        collection_->names()->GetName(String::cast(shared->name()));
    return AddEntry(object, HeapEntry::kClosure, name);
  } else if (object->IsJSRegExp()) {
    JSRegExp* re = JSRegExp::cast(object);
    return AddEntry(object,
                    HeapEntry::kRegExp,
                    collection_->names()->GetName(re->Pattern()));
  } else if (object->IsJSObject()) {
    const char* name = collection_->names()->GetName(
        GetConstructorName(JSObject::cast(object)));
    if (object->IsJSGlobalObject()) {
      const char* tag = objects_tags_.GetTag(object);
      if (tag != NULL) {
        name = collection_->names()->GetFormatted("%s / %s", name, tag);
      }
    }
    return AddEntry(object, HeapEntry::kObject, name);
  } else if (object->IsString()) {
    return AddEntry(object,
                    HeapEntry::kString,
                    collection_->names()->GetName(String::cast(object)));
  } else if (object->IsCode()) {
    return AddEntry(object, HeapEntry::kCode, "");
  } else if (object->IsSharedFunctionInfo()) {
    String* name = String::cast(SharedFunctionInfo::cast(object)->name());
    return AddEntry(object,
                    HeapEntry::kCode,
                    collection_->names()->GetName(name));
  } else if (object->IsScript()) {
    Object* name = Script::cast(object)->name();
    return AddEntry(object,
                    HeapEntry::kCode,
                    name->IsString()
                        ? collection_->names()->GetName(String::cast(name))
                        : "");
  } else if (object->IsNativeContext()) {
    return AddEntry(object, HeapEntry::kHidden, "system / NativeContext");
  } else if (object->IsContext()) {
    return AddEntry(object, HeapEntry::kHidden, "system / Context");
  } else if (object->IsFixedArray() ||
             object->IsFixedDoubleArray() ||
             object->IsByteArray() ||
             object->IsExternalArray()) {
    return AddEntry(object, HeapEntry::kArray, "");
  } else if (object->IsHeapNumber()) {
    return AddEntry(object, HeapEntry::kHeapNumber, "number");
  }
  return AddEntry(object, HeapEntry::kHidden, GetSystemEntryName(object));
}


HeapEntry* V8HeapExplorer::AddEntry(HeapObject* object,
                                    HeapEntry::Type type,
                                    const char* name) {
  int object_size = object->Size();
  SnapshotObjectId object_id =
    collection_->GetObjectId(object->address(), object_size);
  return snapshot_->AddEntry(type, name, object_id, object_size);
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
    case MAP_TYPE:
      switch (Map::cast(object)->instance_type()) {
#define MAKE_STRING_MAP_CASE(instance_type, size, name, Name) \
        case instance_type: return "system / Map (" #Name ")";
      STRING_TYPE_LIST(MAKE_STRING_MAP_CASE)
#undef MAKE_STRING_MAP_CASE
        default: return "system / Map";
      }
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
                             int parent)
      : generator_(generator),
        parent_obj_(parent_obj),
        parent_(parent),
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
  int parent_;
  int next_index_;
};


void V8HeapExplorer::ExtractReferences(HeapObject* obj) {
  HeapEntry* heap_entry = GetEntry(obj);
  if (heap_entry == NULL) return;  // No interest in this object.
  int entry = heap_entry->index();

  bool extract_indexed_refs = true;
  if (obj->IsJSGlobalProxy()) {
    ExtractJSGlobalProxyReferences(JSGlobalProxy::cast(obj));
  } else if (obj->IsJSObject()) {
    ExtractJSObjectReferences(entry, JSObject::cast(obj));
  } else if (obj->IsString()) {
    ExtractStringReferences(entry, String::cast(obj));
  } else if (obj->IsContext()) {
    ExtractContextReferences(entry, Context::cast(obj));
  } else if (obj->IsMap()) {
    ExtractMapReferences(entry, Map::cast(obj));
  } else if (obj->IsSharedFunctionInfo()) {
    ExtractSharedFunctionInfoReferences(entry, SharedFunctionInfo::cast(obj));
  } else if (obj->IsScript()) {
    ExtractScriptReferences(entry, Script::cast(obj));
  } else if (obj->IsCodeCache()) {
    ExtractCodeCacheReferences(entry, CodeCache::cast(obj));
  } else if (obj->IsCode()) {
    ExtractCodeReferences(entry, Code::cast(obj));
  } else if (obj->IsJSGlobalPropertyCell()) {
    ExtractJSGlobalPropertyCellReferences(
        entry, JSGlobalPropertyCell::cast(obj));
    extract_indexed_refs = false;
  }
  if (extract_indexed_refs) {
    SetInternalReference(obj, entry, "map", obj->map(), HeapObject::kMapOffset);
    IndexedReferencesExtractor refs_extractor(this, obj, entry);
    obj->Iterate(&refs_extractor);
  }
}


void V8HeapExplorer::ExtractJSGlobalProxyReferences(JSGlobalProxy* proxy) {
  // We need to reference JS global objects from snapshot's root.
  // We use JSGlobalProxy because this is what embedder (e.g. browser)
  // uses for the global object.
  Object* object = proxy->map()->prototype();
  bool is_debug_object = false;
#ifdef ENABLE_DEBUGGER_SUPPORT
  is_debug_object = object->IsGlobalObject() &&
      Isolate::Current()->debug()->IsDebugGlobal(GlobalObject::cast(object));
#endif
  if (!is_debug_object) {
    SetUserGlobalReference(object);
  }
}


void V8HeapExplorer::ExtractJSObjectReferences(
    int entry, JSObject* js_obj) {
  HeapObject* obj = js_obj;
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
    TagObject(shared_info, "(shared function info)");
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
  } else if (obj->IsGlobalObject()) {
    GlobalObject* global_obj = GlobalObject::cast(obj);
    SetInternalReference(global_obj, entry,
                         "builtins", global_obj->builtins(),
                         GlobalObject::kBuiltinsOffset);
    SetInternalReference(global_obj, entry,
                         "native_context", global_obj->native_context(),
                         GlobalObject::kNativeContextOffset);
    SetInternalReference(global_obj, entry,
                         "global_receiver", global_obj->global_receiver(),
                         GlobalObject::kGlobalReceiverOffset);
  }
  TagObject(js_obj->properties(), "(object properties)");
  SetInternalReference(obj, entry,
                       "properties", js_obj->properties(),
                       JSObject::kPropertiesOffset);
  TagObject(js_obj->elements(), "(object elements)");
  SetInternalReference(obj, entry,
                       "elements", js_obj->elements(),
                       JSObject::kElementsOffset);
}


void V8HeapExplorer::ExtractStringReferences(int entry, String* string) {
  if (string->IsConsString()) {
    ConsString* cs = ConsString::cast(string);
    SetInternalReference(cs, entry, "first", cs->first(),
                         ConsString::kFirstOffset);
    SetInternalReference(cs, entry, "second", cs->second(),
                         ConsString::kSecondOffset);
  } else if (string->IsSlicedString()) {
    SlicedString* ss = SlicedString::cast(string);
    SetInternalReference(ss, entry, "parent", ss->parent(),
                         SlicedString::kParentOffset);
  }
}


void V8HeapExplorer::ExtractContextReferences(int entry, Context* context) {
#define EXTRACT_CONTEXT_FIELD(index, type, name) \
  SetInternalReference(context, entry, #name, context->get(Context::index), \
      FixedArray::OffsetOfElementAt(Context::index));
  EXTRACT_CONTEXT_FIELD(CLOSURE_INDEX, JSFunction, closure);
  EXTRACT_CONTEXT_FIELD(PREVIOUS_INDEX, Context, previous);
  EXTRACT_CONTEXT_FIELD(EXTENSION_INDEX, Object, extension);
  EXTRACT_CONTEXT_FIELD(GLOBAL_OBJECT_INDEX, GlobalObject, global);
  if (context->IsNativeContext()) {
    TagObject(context->jsfunction_result_caches(),
              "(context func. result caches)");
    TagObject(context->normalized_map_cache(), "(context norm. map cache)");
    TagObject(context->runtime_context(), "(runtime context)");
    TagObject(context->embedder_data(), "(context data)");
    NATIVE_CONTEXT_FIELDS(EXTRACT_CONTEXT_FIELD);
#undef EXTRACT_CONTEXT_FIELD
    for (int i = Context::FIRST_WEAK_SLOT;
         i < Context::NATIVE_CONTEXT_SLOTS;
         ++i) {
      SetWeakReference(context, entry, i, context->get(i),
          FixedArray::OffsetOfElementAt(i));
    }
  }
}


void V8HeapExplorer::ExtractMapReferences(int entry, Map* map) {
  SetInternalReference(map, entry,
                       "prototype", map->prototype(), Map::kPrototypeOffset);
  SetInternalReference(map, entry,
                       "constructor", map->constructor(),
                       Map::kConstructorOffset);
  if (map->HasTransitionArray()) {
    TransitionArray* transitions = map->transitions();

    Object* back_pointer = transitions->back_pointer_storage();
    TagObject(transitions->back_pointer_storage(), "(back pointer)");
    SetInternalReference(transitions, entry,
                         "backpointer", back_pointer,
                         TransitionArray::kBackPointerStorageOffset);
    IndexedReferencesExtractor transitions_refs(this, transitions, entry);
    transitions->Iterate(&transitions_refs);

    TagObject(transitions, "(transition array)");
    SetInternalReference(map, entry,
                         "transitions", transitions,
                         Map::kTransitionsOrBackPointerOffset);
  } else {
    Object* back_pointer = map->GetBackPointer();
    TagObject(back_pointer, "(back pointer)");
    SetInternalReference(map, entry,
                         "backpointer", back_pointer,
                         Map::kTransitionsOrBackPointerOffset);
  }
  DescriptorArray* descriptors = map->instance_descriptors();
  TagObject(descriptors, "(map descriptors)");
  SetInternalReference(map, entry,
                       "descriptors", descriptors,
                       Map::kDescriptorsOffset);

  SetInternalReference(map, entry,
                       "code_cache", map->code_cache(),
                       Map::kCodeCacheOffset);
}


void V8HeapExplorer::ExtractSharedFunctionInfoReferences(
    int entry, SharedFunctionInfo* shared) {
  HeapObject* obj = shared;
  SetInternalReference(obj, entry,
                       "name", shared->name(),
                       SharedFunctionInfo::kNameOffset);
  TagObject(shared->code(), "(code)");
  SetInternalReference(obj, entry,
                       "code", shared->code(),
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
  TagObject(shared->construct_stub(), "(code)");
  SetInternalReference(obj, entry,
                       "construct_stub", shared->construct_stub(),
                       SharedFunctionInfo::kConstructStubOffset);
  SetInternalReference(obj, entry,
                       "function_data", shared->function_data(),
                       SharedFunctionInfo::kFunctionDataOffset);
  SetInternalReference(obj, entry,
                       "debug_info", shared->debug_info(),
                       SharedFunctionInfo::kDebugInfoOffset);
  SetInternalReference(obj, entry,
                       "inferred_name", shared->inferred_name(),
                       SharedFunctionInfo::kInferredNameOffset);
  SetInternalReference(obj, entry,
                       "this_property_assignments",
                       shared->this_property_assignments(),
                       SharedFunctionInfo::kThisPropertyAssignmentsOffset);
  SetWeakReference(obj, entry,
                   1, shared->initial_map(),
                   SharedFunctionInfo::kInitialMapOffset);
}


void V8HeapExplorer::ExtractScriptReferences(int entry, Script* script) {
  HeapObject* obj = script;
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
}


void V8HeapExplorer::ExtractCodeCacheReferences(
    int entry, CodeCache* code_cache) {
  TagObject(code_cache->default_cache(), "(default code cache)");
  SetInternalReference(code_cache, entry,
                       "default_cache", code_cache->default_cache(),
                       CodeCache::kDefaultCacheOffset);
  TagObject(code_cache->normal_type_cache(), "(code type cache)");
  SetInternalReference(code_cache, entry,
                       "type_cache", code_cache->normal_type_cache(),
                       CodeCache::kNormalTypeCacheOffset);
}


void V8HeapExplorer::ExtractCodeReferences(int entry, Code* code) {
  TagObject(code->relocation_info(), "(code relocation info)");
  SetInternalReference(code, entry,
                       "relocation_info", code->relocation_info(),
                       Code::kRelocationInfoOffset);
  SetInternalReference(code, entry,
                       "handler_table", code->handler_table(),
                       Code::kHandlerTableOffset);
  TagObject(code->deoptimization_data(), "(code deopt data)");
  SetInternalReference(code, entry,
                       "deoptimization_data", code->deoptimization_data(),
                       Code::kDeoptimizationDataOffset);
  if (code->kind() == Code::FUNCTION) {
    SetInternalReference(code, entry,
                         "type_feedback_info", code->type_feedback_info(),
                         Code::kTypeFeedbackInfoOffset);
  }
  SetInternalReference(code, entry,
                       "gc_metadata", code->gc_metadata(),
                       Code::kGCMetadataOffset);
}


void V8HeapExplorer::ExtractJSGlobalPropertyCellReferences(
    int entry, JSGlobalPropertyCell* cell) {
  SetInternalReference(cell, entry, "value", cell->value());
}


void V8HeapExplorer::ExtractClosureReferences(JSObject* js_obj, int entry) {
  if (!js_obj->IsJSFunction()) return;

  JSFunction* func = JSFunction::cast(js_obj);
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
    Context* context = func->context()->declaration_context();
    ScopeInfo* scope_info = context->closure()->shared()->scope_info();
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
      VariableMode mode;
      int idx = scope_info->FunctionContextSlotIndex(name, &mode);
      if (idx >= 0) {
        SetClosureReference(js_obj, entry, name, context->get(idx));
      }
    }
  }
}


void V8HeapExplorer::ExtractPropertyReferences(JSObject* js_obj, int entry) {
  if (js_obj->HasFastProperties()) {
    DescriptorArray* descs = js_obj->map()->instance_descriptors();
    int real_size = js_obj->map()->NumberOfOwnDescriptors();
    for (int i = 0; i < descs->number_of_descriptors(); i++) {
      if (descs->GetDetails(i).descriptor_index() > real_size) continue;
      switch (descs->GetType(i)) {
        case FIELD: {
          int index = descs->GetFieldIndex(i);

          String* k = descs->GetKey(i);
          if (index < js_obj->map()->inobject_properties()) {
            Object* value = js_obj->InObjectPropertyAt(index);
            if (k != heap_->hidden_symbol()) {
              SetPropertyReference(
                  js_obj, entry,
                  k, value,
                  NULL,
                  js_obj->GetInObjectPropertyOffset(index));
            } else {
              TagObject(value, "(hidden properties)");
              SetInternalReference(
                  js_obj, entry,
                  "hidden_properties", value,
                  js_obj->GetInObjectPropertyOffset(index));
            }
          } else {
            Object* value = js_obj->FastPropertyAt(index);
            if (k != heap_->hidden_symbol()) {
              SetPropertyReference(js_obj, entry, k, value);
            } else {
              TagObject(value, "(hidden properties)");
              SetInternalReference(js_obj, entry, "hidden_properties", value);
            }
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
          break;
        case TRANSITION:
        case NONEXISTENT:
          UNREACHABLE();
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
        // We assume that global objects can only have slow properties.
        Object* value = target->IsJSGlobalPropertyCell()
            ? JSGlobalPropertyCell::cast(target)->value()
            : target;
        if (k != heap_->hidden_symbol()) {
          SetPropertyReference(js_obj, entry, String::cast(k), value);
        } else {
          TagObject(value, "(hidden properties)");
          SetInternalReference(js_obj, entry, "hidden_properties", value);
        }
      }
    }
  }
}


void V8HeapExplorer::ExtractElementReferences(JSObject* js_obj, int entry) {
  if (js_obj->HasFastObjectElements()) {
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


void V8HeapExplorer::ExtractInternalReferences(JSObject* js_obj, int entry) {
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
    if (!result.IsFound()) return object->constructor_name();

    constructor_prop = result.GetLazyValue();
    if (constructor_prop->IsJSFunction()) {
      Object* maybe_name =
          JSFunction::cast(constructor_prop)->shared()->name();
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
  return progress_->ProgressReport(true);
}


bool V8HeapExplorer::IsEssentialObject(Object* object) {
  return object->IsHeapObject()
      && !object->IsOddball()
      && object != heap_->empty_byte_array()
      && object != heap_->empty_fixed_array()
      && object != heap_->empty_descriptor_array()
      && object != heap_->fixed_array_map()
      && object != heap_->global_property_cell_map()
      && object != heap_->shared_function_info_map()
      && object != heap_->free_space_map()
      && object != heap_->one_pointer_filler_map()
      && object != heap_->two_pointer_filler_map();
}


void V8HeapExplorer::SetClosureReference(HeapObject* parent_obj,
                                         int parent_entry,
                                         String* reference_name,
                                         Object* child_obj) {
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry != NULL) {
    filler_->SetNamedReference(HeapGraphEdge::kContextVariable,
                               parent_entry,
                               collection_->names()->GetName(reference_name),
                               child_entry);
  }
}


void V8HeapExplorer::SetNativeBindReference(HeapObject* parent_obj,
                                            int parent_entry,
                                            const char* reference_name,
                                            Object* child_obj) {
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry != NULL) {
    filler_->SetNamedReference(HeapGraphEdge::kShortcut,
                               parent_entry,
                               reference_name,
                               child_entry);
  }
}


void V8HeapExplorer::SetElementReference(HeapObject* parent_obj,
                                         int parent_entry,
                                         int index,
                                         Object* child_obj) {
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry != NULL) {
    filler_->SetIndexedReference(HeapGraphEdge::kElement,
                                 parent_entry,
                                 index,
                                 child_entry);
  }
}


void V8HeapExplorer::SetInternalReference(HeapObject* parent_obj,
                                          int parent_entry,
                                          const char* reference_name,
                                          Object* child_obj,
                                          int field_offset) {
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry == NULL) return;
  if (IsEssentialObject(child_obj)) {
    filler_->SetNamedReference(HeapGraphEdge::kInternal,
                               parent_entry,
                               reference_name,
                               child_entry);
  }
  IndexedReferencesExtractor::MarkVisitedField(parent_obj, field_offset);
}


void V8HeapExplorer::SetInternalReference(HeapObject* parent_obj,
                                          int parent_entry,
                                          int index,
                                          Object* child_obj,
                                          int field_offset) {
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry == NULL) return;
  if (IsEssentialObject(child_obj)) {
    filler_->SetNamedReference(HeapGraphEdge::kInternal,
                               parent_entry,
                               collection_->names()->GetName(index),
                               child_entry);
  }
  IndexedReferencesExtractor::MarkVisitedField(parent_obj, field_offset);
}


void V8HeapExplorer::SetHiddenReference(HeapObject* parent_obj,
                                        int parent_entry,
                                        int index,
                                        Object* child_obj) {
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry != NULL && IsEssentialObject(child_obj)) {
    filler_->SetIndexedReference(HeapGraphEdge::kHidden,
                                 parent_entry,
                                 index,
                                 child_entry);
  }
}


void V8HeapExplorer::SetWeakReference(HeapObject* parent_obj,
                                      int parent_entry,
                                      int index,
                                      Object* child_obj,
                                      int field_offset) {
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry != NULL) {
    filler_->SetIndexedReference(HeapGraphEdge::kWeak,
                                 parent_entry,
                                 index,
                                 child_entry);
    IndexedReferencesExtractor::MarkVisitedField(parent_obj, field_offset);
  }
}


void V8HeapExplorer::SetPropertyReference(HeapObject* parent_obj,
                                          int parent_entry,
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
                               parent_entry,
                               name,
                               child_entry);
    IndexedReferencesExtractor::MarkVisitedField(parent_obj, field_offset);
  }
}


void V8HeapExplorer::SetRootGcRootsReference() {
  filler_->SetIndexedAutoIndexReference(
      HeapGraphEdge::kElement,
      snapshot_->root()->index(),
      snapshot_->gc_roots());
}


void V8HeapExplorer::SetUserGlobalReference(Object* child_obj) {
  HeapEntry* child_entry = GetEntry(child_obj);
  ASSERT(child_entry != NULL);
  filler_->SetNamedAutoIndexReference(
      HeapGraphEdge::kShortcut,
      snapshot_->root()->index(),
      child_entry);
}


void V8HeapExplorer::SetGcRootsReference(VisitorSynchronization::SyncTag tag) {
  filler_->SetIndexedAutoIndexReference(
      HeapGraphEdge::kElement,
      snapshot_->gc_roots()->index(),
      snapshot_->gc_subroot(tag));
}


void V8HeapExplorer::SetGcSubrootReference(
    VisitorSynchronization::SyncTag tag, bool is_weak, Object* child_obj) {
  HeapEntry* child_entry = GetEntry(child_obj);
  if (child_entry != NULL) {
    const char* name = GetStrongGcSubrootName(child_obj);
    if (name != NULL) {
      filler_->SetNamedReference(
          HeapGraphEdge::kInternal,
          snapshot_->gc_subroot(tag)->index(),
          name,
          child_entry);
    } else {
      filler_->SetIndexedAutoIndexReference(
          is_weak ? HeapGraphEdge::kWeak : HeapGraphEdge::kElement,
          snapshot_->gc_subroot(tag)->index(),
          child_entry);
    }
  }
}


const char* V8HeapExplorer::GetStrongGcSubrootName(Object* object) {
  if (strong_gc_subroot_names_.is_empty()) {
#define NAME_ENTRY(name) strong_gc_subroot_names_.SetTag(heap_->name(), #name);
#define ROOT_NAME(type, name, camel_name) NAME_ENTRY(name)
    STRONG_ROOT_LIST(ROOT_NAME)
#undef ROOT_NAME
#define STRUCT_MAP_NAME(NAME, Name, name) NAME_ENTRY(name##_map)
    STRUCT_LIST(STRUCT_MAP_NAME)
#undef STRUCT_MAP_NAME
#define SYMBOL_NAME(name, str) NAME_ENTRY(name)
    SYMBOL_LIST(SYMBOL_NAME)
#undef SYMBOL_NAME
#undef NAME_ENTRY
    CHECK(!strong_gc_subroot_names_.is_empty());
  }
  return strong_gc_subroot_names_.GetTag(object);
}


void V8HeapExplorer::TagObject(Object* obj, const char* tag) {
  if (IsEssentialObject(obj)) {
    HeapEntry* entry = GetEntry(obj);
    if (entry->name()[0] == '\0') {
      entry->set_name(tag);
    }
  }
}


class GlobalObjectsEnumerator : public ObjectVisitor {
 public:
  virtual void VisitPointers(Object** start, Object** end) {
    for (Object** p = start; p < end; p++) {
      if ((*p)->IsNativeContext()) {
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
    if (global_object_name_resolver_) {
      HandleScope scope;
      Handle<JSGlobalObject> global_obj = enumerator.at(i);
      urls[i] = global_object_name_resolver_->GetName(
          Utils::ToLocal(Handle<JSObject>::cast(global_obj)));
    } else {
      // TODO(yurys): This branch is going to be removed once Chromium migrates
      // to the new name resolver.
      urls[i] = NULL;
      HandleScope scope;
      Handle<JSGlobalObject> global_obj = enumerator.at(i);
      Object* obj_document;
      if (global_obj->GetProperty(*document_string)->ToObject(&obj_document) &&
          obj_document->IsJSObject()) {
        // FixMe: Workaround: SharedWorker's current Isolate has NULL context.
        // As result GetProperty(*url_string) will crash.
        if (!Isolate::Current()->context() && obj_document->IsJSGlobalProxy())
          continue;
        JSObject* document = JSObject::cast(obj_document);
        Object* obj_url;
        if (document->GetProperty(*url_string)->ToObject(&obj_url) &&
            obj_url->IsString()) {
          urls[i] = collection_->names()->GetName(String::cast(obj_url));
        }
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
  virtual HeapEntry* AllocateEntry(HeapThing ptr);
 private:
  HeapSnapshot* snapshot_;
  HeapSnapshotsCollection* collection_;
  HeapEntry::Type entries_type_;
};


HeapEntry* BasicHeapEntriesAllocator::AllocateEntry(HeapThing ptr) {
  v8::RetainedObjectInfo* info = reinterpret_cast<v8::RetainedObjectInfo*>(ptr);
  intptr_t elements = info->GetElementCount();
  intptr_t size = info->GetSizeInBytes();
  const char* name = elements != -1
      ? collection_->names()->GetFormatted(
            "%s / %" V8_PTR_PREFIX "d entries", info->GetLabel(), elements)
      : collection_->names()->GetCopy(info->GetLabel());
  return snapshot_->AddEntry(
      entries_type_,
      name,
      HeapObjectsMap::GenerateId(info),
      size != -1 ? static_cast<int>(size) : 0);
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
    int parent_entry =
        filler_->FindOrAddEntry(parent, native_entries_allocator_)->index();
    ASSERT(parent_entry != HeapEntry::kNoEntry);
    Object*** children = group->children_;
    for (size_t j = 0; j < group->length_; ++j) {
      Object* child = *children[j];
      HeapEntry* child_entry =
          filler_->FindOrAddEntry(child, native_entries_allocator_);
      filler_->SetNamedReference(
          HeapGraphEdge::kInternal,
          parent_entry,
          "native",
          child_entry);
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
  if (entry->value == NULL) {
    entry->value = new NativeGroupRetainedObjectInfo(label);
  }
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
      group_entry->index(),
      child_entry);
}


void NativeObjectsExplorer::SetWrapperNativeReferences(
    HeapObject* wrapper, v8::RetainedObjectInfo* info) {
  HeapEntry* wrapper_entry = filler_->FindEntry(wrapper);
  ASSERT(wrapper_entry != NULL);
  HeapEntry* info_entry =
      filler_->FindOrAddEntry(info, native_entries_allocator_);
  ASSERT(info_entry != NULL);
  filler_->SetNamedReference(HeapGraphEdge::kInternal,
                             wrapper_entry->index(),
                             "native",
                             info_entry);
  filler_->SetIndexedAutoIndexReference(HeapGraphEdge::kElement,
                                        info_entry->index(),
                                        wrapper_entry);
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
        snapshot_->root()->index(),
        group_entry);
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


class SnapshotFiller : public SnapshotFillerInterface {
 public:
  explicit SnapshotFiller(HeapSnapshot* snapshot, HeapEntriesMap* entries)
      : snapshot_(snapshot),
        collection_(snapshot->collection()),
        entries_(entries) { }
  HeapEntry* AddEntry(HeapThing ptr, HeapEntriesAllocator* allocator) {
    HeapEntry* entry = allocator->AllocateEntry(ptr);
    entries_->Pair(ptr, entry->index());
    return entry;
  }
  HeapEntry* FindEntry(HeapThing ptr) {
    int index = entries_->Map(ptr);
    return index != HeapEntry::kNoEntry ? &snapshot_->entries()[index] : NULL;
  }
  HeapEntry* FindOrAddEntry(HeapThing ptr, HeapEntriesAllocator* allocator) {
    HeapEntry* entry = FindEntry(ptr);
    return entry != NULL ? entry : AddEntry(ptr, allocator);
  }
  void SetIndexedReference(HeapGraphEdge::Type type,
                           int parent,
                           int index,
                           HeapEntry* child_entry) {
    HeapEntry* parent_entry = &snapshot_->entries()[parent];
    parent_entry->SetIndexedReference(type, index, child_entry);
  }
  void SetIndexedAutoIndexReference(HeapGraphEdge::Type type,
                                    int parent,
                                    HeapEntry* child_entry) {
    HeapEntry* parent_entry = &snapshot_->entries()[parent];
    int index = parent_entry->children_count() + 1;
    parent_entry->SetIndexedReference(type, index, child_entry);
  }
  void SetNamedReference(HeapGraphEdge::Type type,
                         int parent,
                         const char* reference_name,
                         HeapEntry* child_entry) {
    HeapEntry* parent_entry = &snapshot_->entries()[parent];
    parent_entry->SetNamedReference(type, reference_name, child_entry);
  }
  void SetNamedAutoIndexReference(HeapGraphEdge::Type type,
                                  int parent,
                                  HeapEntry* child_entry) {
    HeapEntry* parent_entry = &snapshot_->entries()[parent];
    int index = parent_entry->children_count() + 1;
    parent_entry->SetNamedReference(
        type,
        collection_->names()->GetName(index),
        child_entry);
  }

 private:
  HeapSnapshot* snapshot_;
  HeapSnapshotsCollection* collection_;
  HeapEntriesMap* entries_;
};


HeapSnapshotGenerator::HeapSnapshotGenerator(
    HeapSnapshot* snapshot,
    v8::ActivityControl* control,
    v8::HeapProfiler::ObjectNameResolver* resolver)
    : snapshot_(snapshot),
      control_(control),
      v8_heap_explorer_(snapshot_, this, resolver),
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

#ifdef VERIFY_HEAP
  Heap* debug_heap = Isolate::Current()->heap();
  CHECK(!debug_heap->old_data_space()->was_swept_conservatively());
  CHECK(!debug_heap->old_pointer_space()->was_swept_conservatively());
  CHECK(!debug_heap->code_space()->was_swept_conservatively());
  CHECK(!debug_heap->cell_space()->was_swept_conservatively());
  CHECK(!debug_heap->map_space()->was_swept_conservatively());
#endif

  // The following code uses heap iterators, so we want the heap to be
  // stable. It should follow TagGlobalObjects as that can allocate.
  AssertNoAllocation no_alloc;

#ifdef VERIFY_HEAP
  debug_heap->Verify();
#endif

  SetProgressTotal(1);  // 1 pass.

#ifdef VERIFY_HEAP
  debug_heap->Verify();
#endif

  if (!FillReferences()) return false;

  snapshot_->FillChildren();
  snapshot_->RememberLastJSObjectId();

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
  progress_total_ = iterations_count * (
      v8_heap_explorer_.EstimateObjectsCount(&iterator) +
      dom_explorer_.EstimateObjectsCount());
  progress_counter_ = 0;
}


bool HeapSnapshotGenerator::FillReferences() {
  SnapshotFiller filler(snapshot_, &entries_);
  v8_heap_explorer_.AddRootEntries(&filler);
  return v8_heap_explorer_.IterateAndExtractReferences(&filler)
      && dom_explorer_.IterateAndExtractReferences(&filler);
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
  void AddNumber(unsigned n) { AddNumberImpl<unsigned>(n, "%u"); }
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


// type, name|index, to_node.
const int HeapSnapshotJSONSerializer::kEdgeFieldsCount = 3;
// type, name, id, self_size, children_index.
const int HeapSnapshotJSONSerializer::kNodeFieldsCount = 5;

void HeapSnapshotJSONSerializer::Serialize(v8::OutputStream* stream) {
  ASSERT(writer_ == NULL);
  writer_ = new OutputStreamWriter(stream);

  HeapSnapshot* original_snapshot = NULL;
  if (snapshot_->RawSnapshotSize() >=
      SnapshotSizeConstants<kPointerSize>::kMaxSerializableSnapshotRawSize) {
    // The snapshot is too big. Serialize a fake snapshot.
    original_snapshot = snapshot_;
    snapshot_ = CreateFakeSnapshot();
  }

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
  result->AddRootEntry();
  const char* text = snapshot_->collection()->names()->GetFormatted(
      "The snapshot is too big. "
      "Maximum snapshot size is %"  V8_PTR_PREFIX "u MB. "
      "Actual snapshot size is %"  V8_PTR_PREFIX "u MB.",
      SnapshotSizeConstants<kPointerSize>::kMaxSerializableSnapshotRawSize / MB,
      (snapshot_->RawSnapshotSize() + MB - 1) / MB);
  HeapEntry* message = result->AddEntry(HeapEntry::kString, text, 0, 4);
  result->root()->SetIndexedReference(HeapGraphEdge::kElement, 1, message);
  result->FillChildren();
  return result;
}


void HeapSnapshotJSONSerializer::SerializeImpl() {
  ASSERT(0 == snapshot_->root()->index());
  writer_->AddCharacter('{');
  writer_->AddString("\"snapshot\":{");
  SerializeSnapshot();
  if (writer_->aborted()) return;
  writer_->AddString("},\n");
  writer_->AddString("\"nodes\":[");
  SerializeNodes();
  if (writer_->aborted()) return;
  writer_->AddString("],\n");
  writer_->AddString("\"edges\":[");
  SerializeEdges();
  if (writer_->aborted()) return;
  writer_->AddString("],\n");
  writer_->AddString("\"strings\":[");
  SerializeStrings();
  if (writer_->aborted()) return;
  writer_->AddCharacter(']');
  writer_->AddCharacter('}');
  writer_->Finalize();
}


int HeapSnapshotJSONSerializer::GetStringId(const char* s) {
  HashMap::Entry* cache_entry = strings_.Lookup(
      const_cast<char*>(s), ObjectHash(s), true);
  if (cache_entry->value == NULL) {
    cache_entry->value = reinterpret_cast<void*>(next_string_id_++);
  }
  return static_cast<int>(reinterpret_cast<intptr_t>(cache_entry->value));
}


static int utoa(unsigned value, const Vector<char>& buffer, int buffer_pos) {
  int number_of_digits = 0;
  unsigned t = value;
  do {
    ++number_of_digits;
  } while (t /= 10);

  buffer_pos += number_of_digits;
  int result = buffer_pos;
  do {
    int last_digit = value % 10;
    buffer[--buffer_pos] = '0' + last_digit;
    value /= 10;
  } while (value);
  return result;
}


void HeapSnapshotJSONSerializer::SerializeEdge(HeapGraphEdge* edge,
                                               bool first_edge) {
  // The buffer needs space for 3 unsigned ints, 3 commas, \n and \0
  static const int kBufferSize =
      MaxDecimalDigitsIn<sizeof(unsigned)>::kUnsigned * 3 + 3 + 2;  // NOLINT
  EmbeddedVector<char, kBufferSize> buffer;
  int edge_name_or_index = edge->type() == HeapGraphEdge::kElement
      || edge->type() == HeapGraphEdge::kHidden
      || edge->type() == HeapGraphEdge::kWeak
      ? edge->index() : GetStringId(edge->name());
  int buffer_pos = 0;
  if (!first_edge) {
    buffer[buffer_pos++] = ',';
  }
  buffer_pos = utoa(edge->type(), buffer, buffer_pos);
  buffer[buffer_pos++] = ',';
  buffer_pos = utoa(edge_name_or_index, buffer, buffer_pos);
  buffer[buffer_pos++] = ',';
  buffer_pos = utoa(entry_index(edge->to()), buffer, buffer_pos);
  buffer[buffer_pos++] = '\n';
  buffer[buffer_pos++] = '\0';
  writer_->AddString(buffer.start());
}


void HeapSnapshotJSONSerializer::SerializeEdges() {
  List<HeapGraphEdge*>& edges = snapshot_->children();
  for (int i = 0; i < edges.length(); ++i) {
    ASSERT(i == 0 ||
           edges[i - 1]->from()->index() <= edges[i]->from()->index());
    SerializeEdge(edges[i], i == 0);
    if (writer_->aborted()) return;
  }
}


void HeapSnapshotJSONSerializer::SerializeNode(HeapEntry* entry) {
  // The buffer needs space for 5 unsigned ints, 5 commas, \n and \0
  static const int kBufferSize =
      5 * MaxDecimalDigitsIn<sizeof(unsigned)>::kUnsigned  // NOLINT
      + 5 + 1 + 1;
  EmbeddedVector<char, kBufferSize> buffer;
  int buffer_pos = 0;
  if (entry_index(entry) != 0) {
    buffer[buffer_pos++] = ',';
  }
  buffer_pos = utoa(entry->type(), buffer, buffer_pos);
  buffer[buffer_pos++] = ',';
  buffer_pos = utoa(GetStringId(entry->name()), buffer, buffer_pos);
  buffer[buffer_pos++] = ',';
  buffer_pos = utoa(entry->id(), buffer, buffer_pos);
  buffer[buffer_pos++] = ',';
  buffer_pos = utoa(entry->self_size(), buffer, buffer_pos);
  buffer[buffer_pos++] = ',';
  buffer_pos = utoa(entry->children_count(), buffer, buffer_pos);
  buffer[buffer_pos++] = '\n';
  buffer[buffer_pos++] = '\0';
  writer_->AddString(buffer.start());
}


void HeapSnapshotJSONSerializer::SerializeNodes() {
  List<HeapEntry>& entries = snapshot_->entries();
  for (int i = 0; i < entries.length(); ++i) {
    SerializeNode(&entries[i]);
    if (writer_->aborted()) return;
  }
}


void HeapSnapshotJSONSerializer::SerializeSnapshot() {
  writer_->AddString("\"title\":\"");
  writer_->AddString(snapshot_->title());
  writer_->AddString("\"");
  writer_->AddString(",\"uid\":");
  writer_->AddNumber(snapshot_->uid());
  writer_->AddString(",\"meta\":");
  // The object describing node serialization layout.
  // We use a set of macros to improve readability.
#define JSON_A(s) "[" s "]"
#define JSON_O(s) "{" s "}"
#define JSON_S(s) "\"" s "\""
  writer_->AddString(JSON_O(
    JSON_S("node_fields") ":" JSON_A(
        JSON_S("type") ","
        JSON_S("name") ","
        JSON_S("id") ","
        JSON_S("self_size") ","
        JSON_S("edge_count")) ","
    JSON_S("node_types") ":" JSON_A(
        JSON_A(
            JSON_S("hidden") ","
            JSON_S("array") ","
            JSON_S("string") ","
            JSON_S("object") ","
            JSON_S("code") ","
            JSON_S("closure") ","
            JSON_S("regexp") ","
            JSON_S("number") ","
            JSON_S("native") ","
            JSON_S("synthetic")) ","
        JSON_S("string") ","
        JSON_S("number") ","
        JSON_S("number") ","
        JSON_S("number") ","
        JSON_S("number") ","
        JSON_S("number")) ","
    JSON_S("edge_fields") ":" JSON_A(
        JSON_S("type") ","
        JSON_S("name_or_index") ","
        JSON_S("to_node")) ","
    JSON_S("edge_types") ":" JSON_A(
        JSON_A(
            JSON_S("context") ","
            JSON_S("element") ","
            JSON_S("property") ","
            JSON_S("internal") ","
            JSON_S("hidden") ","
            JSON_S("shortcut") ","
            JSON_S("weak")) ","
        JSON_S("string_or_number") ","
        JSON_S("node"))));
#undef JSON_S
#undef JSON_O
#undef JSON_A
  writer_->AddString(",\"node_count\":");
  writer_->AddNumber(snapshot_->entries().length());
  writer_->AddString(",\"edge_count\":");
  writer_->AddNumber(snapshot_->edges().length());
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
