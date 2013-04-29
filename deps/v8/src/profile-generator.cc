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
  isolate->global_handles()->MakeWeak(handle.location(),
                                      this,
                                      NULL,
                                      TokenRemovedCallback);
  token_locations_.Add(handle.location());
  token_removed_.Add(false);
  return token_locations_.length() - 1;
}


void TokenEnumerator::TokenRemovedCallback(v8::Isolate* isolate,
                                           v8::Persistent<v8::Value> handle,
                                           void* parameter) {
  reinterpret_cast<TokenEnumerator*>(parameter)->TokenRemoved(
      Utils::OpenHandle(*handle).location());
  handle.Dispose(isolate);
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
      StringHasher::HashSequentialString(dst.start(), len, HEAP->HashSeed());
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
  uint32_t hash = StringHasher::HashSequentialString(
      str.start(), len, HEAP->HashSeed());
  return AddOrDisposeString(str.start(), hash);
}


const char* StringsStorage::GetName(Name* name) {
  if (name->IsString()) {
    String* str = String::cast(name);
    int length = Min(kMaxNameSize, str->length());
    SmartArrayPointer<char> data =
        str->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL, 0, length);
    uint32_t hash = StringHasher::HashSequentialString(
        *data, length, name->GetHeap()->HashSeed());
    return AddOrDisposeString(data.Detach(), hash);
  } else if (name->IsSymbol()) {
    return "<symbol>";
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
  OS::Print("%5u %5u %*c %s%s [%d] #%d",
            total_ticks_, self_ticks_,
            indent, ' ',
            entry_->name_prefix(),
            entry_->name(),
            entry_->security_token_id(),
            id());
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
      next_node_id_(1),
      root_(new ProfileNode(this, &root_entry_)) {
}


ProfileTree::~ProfileTree() {
  DeleteNodesCallback cb;
  TraverseDepthFirst(&cb);
}


ProfileNode* ProfileTree::AddPathFromEnd(const Vector<CodeEntry*>& path) {
  ProfileNode* node = root_;
  for (CodeEntry** entry = path.start() + path.length() - 1;
       entry != path.start() - 1;
       --entry) {
    if (*entry != NULL) {
      node = node->FindOrAddChild(*entry);
    }
  }
  node->IncrementSelfTicks();
  return node;
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
  ProfileNode* top_frame_node = top_down_.AddPathFromEnd(path);
  if (record_samples_) samples_.Add(top_frame_node);
}


void CpuProfile::CalculateTotalTicks() {
  top_down_.CalculateTotalTicks();
}


void CpuProfile::SetActualSamplingRate(double actual_sampling_rate) {
  top_down_.SetTickRatePerMs(actual_sampling_rate);
}


CpuProfile* CpuProfile::FilteredClone(int security_token_id) {
  ASSERT(security_token_id != TokenEnumerator::kNoSecurityToken);
  CpuProfile* clone = new CpuProfile(title_, uid_, false);
  clone->top_down_.FilteredClone(&top_down_, security_token_id);
  return clone;
}


void CpuProfile::ShortPrint() {
  OS::Print("top down ");
  top_down_.ShortPrint();
}


void CpuProfile::Print() {
  OS::Print("[Top down]:\n");
  top_down_.Print();
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
  // For shared function entries, 'size' field is used to store their IDs.
  if (value.entry == kSharedFunctionCodeEntry) {
    OS::Print("%p SharedFunctionInfo %d\n", key, value.size);
  } else {
    OS::Print("%p %5d %s\n", key, value.size, value.entry->name());
  }
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


bool CpuProfilesCollection::StartProfiling(const char* title, unsigned uid,
                                           bool record_samples) {
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
  current_profiles_.Add(new CpuProfile(title, uid, record_samples));
  current_profiles_semaphore_->Signal();
  return true;
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
                                               Name* name,
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
                                               Name* name) {
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


} }  // namespace v8::internal
