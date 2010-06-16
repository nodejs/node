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

#ifndef V8_PROFILE_GENERATOR_H_
#define V8_PROFILE_GENERATOR_H_

#ifdef ENABLE_LOGGING_AND_PROFILING

#include "hashmap.h"
#include "../include/v8-profiler.h"

namespace v8 {
namespace internal {

class TokenEnumerator {
 public:
  TokenEnumerator();
  ~TokenEnumerator();
  int GetTokenId(Object* token);

  static const int kNoSecurityToken = -1;
  static const int kInheritsSecurityToken = -2;

 private:
  static void TokenRemovedCallback(v8::Persistent<v8::Value> handle,
                                   void* parameter);
  void TokenRemoved(Object** token_location);

  List<Object**> token_locations_;
  List<bool> token_removed_;

  friend class TokenEnumeratorTester;

  DISALLOW_COPY_AND_ASSIGN(TokenEnumerator);
};


// Provides a storage of strings allocated in C++ heap, to hold them
// forever, even if they disappear from JS heap or external storage.
class StringsStorage {
 public:
  StringsStorage();
  ~StringsStorage();

  const char* GetName(String* name);

 private:
  INLINE(static bool StringsMatch(void* key1, void* key2)) {
    return strcmp(reinterpret_cast<char*>(key1),
                  reinterpret_cast<char*>(key2)) == 0;
  }

  // String::Hash -> const char*
  HashMap names_;

  DISALLOW_COPY_AND_ASSIGN(StringsStorage);
};


class CodeEntry {
 public:
  explicit INLINE(CodeEntry(int security_token_id));
  // CodeEntry doesn't own name strings, just references them.
  INLINE(CodeEntry(Logger::LogEventsAndTags tag,
                   const char* name_prefix,
                   const char* name,
                   const char* resource_name,
                   int line_number,
                   int security_token_id));

  INLINE(bool is_js_function() const) { return is_js_function_tag(tag_); }
  INLINE(const char* name_prefix() const) { return name_prefix_; }
  INLINE(bool has_name_prefix() const) { return name_prefix_[0] != '\0'; }
  INLINE(const char* name() const) { return name_; }
  INLINE(const char* resource_name() const) { return resource_name_; }
  INLINE(int line_number() const) { return line_number_; }
  INLINE(unsigned call_uid() const) { return call_uid_; }
  INLINE(int security_token_id() const) { return security_token_id_; }

  INLINE(static bool is_js_function_tag(Logger::LogEventsAndTags tag));

  void CopyData(const CodeEntry& source);

  static const char* kEmptyNamePrefix;

 private:
  unsigned call_uid_;
  Logger::LogEventsAndTags tag_;
  const char* name_prefix_;
  const char* name_;
  const char* resource_name_;
  int line_number_;
  int security_token_id_;

  static unsigned next_call_uid_;

  DISALLOW_COPY_AND_ASSIGN(CodeEntry);
};


class ProfileTree;

class ProfileNode {
 public:
  INLINE(ProfileNode(ProfileTree* tree, CodeEntry* entry));

  ProfileNode* FindChild(CodeEntry* entry);
  ProfileNode* FindOrAddChild(CodeEntry* entry);
  INLINE(void IncrementSelfTicks()) { ++self_ticks_; }
  INLINE(void IncreaseSelfTicks(unsigned amount)) { self_ticks_ += amount; }
  INLINE(void IncreaseTotalTicks(unsigned amount)) { total_ticks_ += amount; }

  INLINE(CodeEntry* entry() const) { return entry_; }
  INLINE(unsigned self_ticks() const) { return self_ticks_; }
  INLINE(unsigned total_ticks() const) { return total_ticks_; }
  INLINE(const List<ProfileNode*>* children() const) { return &children_list_; }
  double GetSelfMillis() const;
  double GetTotalMillis() const;

  void Print(int indent);

 private:
  INLINE(static bool CodeEntriesMatch(void* entry1, void* entry2)) {
    return entry1 == entry2;
  }

  INLINE(static uint32_t CodeEntryHash(CodeEntry* entry)) {
    return static_cast<int32_t>(reinterpret_cast<intptr_t>(entry));
  }

  ProfileTree* tree_;
  CodeEntry* entry_;
  unsigned total_ticks_;
  unsigned self_ticks_;
  // CodeEntry* -> ProfileNode*
  HashMap children_;
  List<ProfileNode*> children_list_;

  DISALLOW_COPY_AND_ASSIGN(ProfileNode);
};


class ProfileTree {
 public:
  ProfileTree();
  ~ProfileTree();

  void AddPathFromEnd(const Vector<CodeEntry*>& path);
  void AddPathFromStart(const Vector<CodeEntry*>& path);
  void CalculateTotalTicks();
  void FilteredClone(ProfileTree* src, int security_token_id);

  double TicksToMillis(unsigned ticks) const {
    return ticks * ms_to_ticks_scale_;
  }
  ProfileNode* root() const { return root_; }
  void SetTickRatePerMs(double ticks_per_ms);

  void ShortPrint();
  void Print() {
    root_->Print(0);
  }

 private:
  template <typename Callback>
  void TraverseDepthFirst(Callback* callback);

  CodeEntry root_entry_;
  ProfileNode* root_;
  double ms_to_ticks_scale_;

  DISALLOW_COPY_AND_ASSIGN(ProfileTree);
};


class CpuProfile {
 public:
  CpuProfile(const char* title, unsigned uid)
      : title_(title), uid_(uid) { }

  // Add pc -> ... -> main() call path to the profile.
  void AddPath(const Vector<CodeEntry*>& path);
  void CalculateTotalTicks();
  void SetActualSamplingRate(double actual_sampling_rate);
  CpuProfile* FilteredClone(int security_token_id);

  INLINE(const char* title() const) { return title_; }
  INLINE(unsigned uid() const) { return uid_; }
  INLINE(const ProfileTree* top_down() const) { return &top_down_; }
  INLINE(const ProfileTree* bottom_up() const) { return &bottom_up_; }

  void UpdateTicksScale();

  void ShortPrint();
  void Print();

 private:
  const char* title_;
  unsigned uid_;
  ProfileTree top_down_;
  ProfileTree bottom_up_;

  DISALLOW_COPY_AND_ASSIGN(CpuProfile);
};


class CodeMap {
 public:
  CodeMap() { }
  INLINE(void AddCode(Address addr, CodeEntry* entry, unsigned size));
  INLINE(void MoveCode(Address from, Address to));
  INLINE(void DeleteCode(Address addr));
  void AddAlias(Address start, CodeEntry* entry, Address code_start);
  CodeEntry* FindEntry(Address addr);

  void Print();

 private:
  struct CodeEntryInfo {
    CodeEntryInfo(CodeEntry* an_entry, unsigned a_size)
        : entry(an_entry), size(a_size) { }
    CodeEntry* entry;
    unsigned size;
  };

  struct CodeTreeConfig {
    typedef Address Key;
    typedef CodeEntryInfo Value;
    static const Key kNoKey;
    static const Value kNoValue;
    static int Compare(const Key& a, const Key& b) {
      return a < b ? -1 : (a > b ? 1 : 0);
    }
  };
  typedef SplayTree<CodeTreeConfig> CodeTree;

  class CodeTreePrinter {
   public:
    void Call(const Address& key, const CodeEntryInfo& value);
  };

  CodeTree tree_;

  DISALLOW_COPY_AND_ASSIGN(CodeMap);
};


class CpuProfilesCollection {
 public:
  CpuProfilesCollection();
  ~CpuProfilesCollection();

  bool StartProfiling(const char* title, unsigned uid);
  bool StartProfiling(String* title, unsigned uid);
  CpuProfile* StopProfiling(int security_token_id,
                            const char* title,
                            double actual_sampling_rate);
  CpuProfile* StopProfiling(int security_token_id,
                            String* title,
                            double actual_sampling_rate);
  List<CpuProfile*>* Profiles(int security_token_id);
  const char* GetName(String* name) {
    return function_and_resource_names_.GetName(name);
  }
  CpuProfile* GetProfile(int security_token_id, unsigned uid);
  inline bool is_last_profile();

  CodeEntry* NewCodeEntry(Logger::LogEventsAndTags tag,
                          String* name, String* resource_name, int line_number);
  CodeEntry* NewCodeEntry(Logger::LogEventsAndTags tag, const char* name);
  CodeEntry* NewCodeEntry(Logger::LogEventsAndTags tag,
                          const char* name_prefix, String* name);
  CodeEntry* NewCodeEntry(Logger::LogEventsAndTags tag, int args_count);
  CodeEntry* NewCodeEntry(int security_token_id);

  // Called from profile generator thread.
  void AddPathToCurrentProfiles(const Vector<CodeEntry*>& path);

 private:
  INLINE(const char* GetFunctionName(String* name));
  INLINE(const char* GetFunctionName(const char* name));
  const char* GetName(int args_count);
  List<CpuProfile*>* GetProfilesList(int security_token_id);
  int TokenToIndex(int security_token_id);

  INLINE(static bool UidsMatch(void* key1, void* key2)) {
    return key1 == key2;
  }

  StringsStorage function_and_resource_names_;
  // args_count -> char*
  List<char*> args_count_names_;
  List<CodeEntry*> code_entries_;
  List<List<CpuProfile*>* > profiles_by_token_;
  // uid -> index
  HashMap profiles_uids_;

  // Accessed by VM thread and profile generator thread.
  List<CpuProfile*> current_profiles_;
  Semaphore* current_profiles_semaphore_;

  DISALLOW_COPY_AND_ASSIGN(CpuProfilesCollection);
};


class SampleRateCalculator {
 public:
  SampleRateCalculator()
      : result_(Logger::kSamplingIntervalMs * kResultScale),
        ticks_per_ms_(Logger::kSamplingIntervalMs),
        measurements_count_(0),
        wall_time_query_countdown_(1) {
  }

  double ticks_per_ms() {
    return result_ / static_cast<double>(kResultScale);
  }
  void Tick();
  void UpdateMeasurements(double current_time);

  // Instead of querying current wall time each tick,
  // we use this constant to control query intervals.
  static const unsigned kWallTimeQueryIntervalMs = 100;

 private:
  // As the result needs to be accessed from a different thread, we
  // use type that guarantees atomic writes to memory.  There should
  // be <= 1000 ticks per second, thus storing a value of a 10 ** 5
  // order should provide enough precision while keeping away from a
  // potential overflow.
  static const int kResultScale = 100000;

  AtomicWord result_;
  // All other fields are accessed only from the sampler thread.
  double ticks_per_ms_;
  unsigned measurements_count_;
  unsigned wall_time_query_countdown_;
  double last_wall_time_;

  DISALLOW_COPY_AND_ASSIGN(SampleRateCalculator);
};


class ProfileGenerator {
 public:
  explicit ProfileGenerator(CpuProfilesCollection* profiles);

  INLINE(CodeEntry* NewCodeEntry(Logger::LogEventsAndTags tag,
                                 String* name,
                                 String* resource_name,
                                 int line_number)) {
    return profiles_->NewCodeEntry(tag, name, resource_name, line_number);
  }

  INLINE(CodeEntry* NewCodeEntry(Logger::LogEventsAndTags tag,
                                 const char* name)) {
    return profiles_->NewCodeEntry(tag, name);
  }

  INLINE(CodeEntry* NewCodeEntry(Logger::LogEventsAndTags tag,
                                 const char* name_prefix,
                                 String* name)) {
    return profiles_->NewCodeEntry(tag, name_prefix, name);
  }

  INLINE(CodeEntry* NewCodeEntry(Logger::LogEventsAndTags tag,
                                 int args_count)) {
    return profiles_->NewCodeEntry(tag, args_count);
  }

  INLINE(CodeEntry* NewCodeEntry(int security_token_id)) {
    return profiles_->NewCodeEntry(security_token_id);
  }

  void RecordTickSample(const TickSample& sample);

  INLINE(CodeMap* code_map()) { return &code_map_; }

  INLINE(void Tick()) { sample_rate_calc_.Tick(); }
  INLINE(double actual_sampling_rate()) {
    return sample_rate_calc_.ticks_per_ms();
  }

  static const char* kAnonymousFunctionName;
  static const char* kProgramEntryName;
  static const char* kGarbageCollectorEntryName;

 private:
  INLINE(CodeEntry* EntryForVMState(StateTag tag));

  CpuProfilesCollection* profiles_;
  CodeMap code_map_;
  CodeEntry* program_entry_;
  CodeEntry* gc_entry_;
  SampleRateCalculator sample_rate_calc_;

  DISALLOW_COPY_AND_ASSIGN(ProfileGenerator);
};


class HeapSnapshot;
class HeapEntry;


class HeapGraphEdge {
 public:
  enum Type {
    CONTEXT_VARIABLE = v8::HeapGraphEdge::CONTEXT_VARIABLE,
    ELEMENT = v8::HeapGraphEdge::ELEMENT,
    PROPERTY = v8::HeapGraphEdge::PROPERTY
  };

  HeapGraphEdge(Type type, const char* name, HeapEntry* from, HeapEntry* to);
  HeapGraphEdge(int index, HeapEntry* from, HeapEntry* to);

  Type type() const { return type_; }
  int index() const {
    ASSERT(type_ == ELEMENT);
    return index_;
  }
  const char* name() const {
    ASSERT(type_ == CONTEXT_VARIABLE || type_ == PROPERTY);
    return name_;
  }
  HeapEntry* from() const { return from_; }
  HeapEntry* to() const { return to_; }

 private:
  Type type_;
  union {
    int index_;
    const char* name_;
  };
  HeapEntry* from_;
  HeapEntry* to_;

  DISALLOW_COPY_AND_ASSIGN(HeapGraphEdge);
};


class HeapGraphPath;
class CachedHeapGraphPath;

class HeapEntry {
 public:
  enum Type {
    INTERNAL = v8::HeapGraphNode::INTERNAL,
    ARRAY = v8::HeapGraphNode::ARRAY,
    STRING = v8::HeapGraphNode::STRING,
    OBJECT = v8::HeapGraphNode::OBJECT,
    CODE = v8::HeapGraphNode::CODE,
    CLOSURE = v8::HeapGraphNode::CLOSURE
  };

  explicit HeapEntry(HeapSnapshot* snapshot)
      : snapshot_(snapshot),
        visited_(false),
        type_(INTERNAL),
        name_(""),
        next_auto_index_(0),
        self_size_(0),
        security_token_id_(TokenEnumerator::kNoSecurityToken),
        children_(1),
        retainers_(0),
        retaining_paths_(0),
        total_size_(kUnknownSize),
        non_shared_total_size_(kUnknownSize),
        painted_(kUnpainted) { }
  HeapEntry(HeapSnapshot* snapshot,
            Type type,
            const char* name,
            int self_size,
            int security_token_id)
      : snapshot_(snapshot),
        visited_(false),
        type_(type),
        name_(name),
        next_auto_index_(1),
        self_size_(self_size),
        security_token_id_(security_token_id),
        children_(4),
        retainers_(4),
        retaining_paths_(4),
        total_size_(kUnknownSize),
        non_shared_total_size_(kUnknownSize),
        painted_(kUnpainted) { }
  ~HeapEntry();

  bool visited() const { return visited_; }
  Type type() const { return type_; }
  const char* name() const { return name_; }
  int self_size() const { return self_size_; }
  int security_token_id() const { return security_token_id_; }
  bool painted_reachable() { return painted_ == kPaintReachable; }
  bool not_painted_reachable_from_others() {
    return painted_ != kPaintReachableFromOthers;
  }
  const List<HeapGraphEdge*>* children() const { return &children_; }
  const List<HeapGraphEdge*>* retainers() const { return &retainers_; }
  const List<HeapGraphPath*>* GetRetainingPaths();

  void ClearPaint() { painted_ = kUnpainted; }
  void CutEdges();
  void MarkAsVisited() { visited_ = true; }
  void PaintReachable() {
    ASSERT(painted_ == kUnpainted);
    painted_ = kPaintReachable;
  }
  void PaintReachableFromOthers() { painted_ = kPaintReachableFromOthers; }
  void SetClosureReference(const char* name, HeapEntry* entry);
  void SetElementReference(int index, HeapEntry* entry);
  void SetPropertyReference(const char* name, HeapEntry* entry);
  void SetAutoIndexReference(HeapEntry* entry);

  int TotalSize();
  int NonSharedTotalSize();

  void Print(int max_depth, int indent);

 private:
  int CalculateTotalSize();
  int CalculateNonSharedTotalSize();
  void FindRetainingPaths(HeapEntry* node, CachedHeapGraphPath* prev_path);
  void RemoveChild(HeapGraphEdge* edge);
  void RemoveRetainer(HeapGraphEdge* edge);

  const char* TypeAsString();

  HeapSnapshot* snapshot_;
  bool visited_;
  Type type_;
  const char* name_;
  int next_auto_index_;
  int self_size_;
  int security_token_id_;
  List<HeapGraphEdge*> children_;
  List<HeapGraphEdge*> retainers_;
  List<HeapGraphPath*> retaining_paths_;
  int total_size_;
  int non_shared_total_size_;
  int painted_;

  static const int kUnknownSize = -1;
  static const int kUnpainted = 0;
  static const int kPaintReachable = 1;
  static const int kPaintReachableFromOthers = 2;

  DISALLOW_IMPLICIT_CONSTRUCTORS(HeapEntry);
};


class HeapGraphPath {
 public:
  HeapGraphPath()
      : path_(8) { }
  explicit HeapGraphPath(const List<HeapGraphEdge*>& path);

  void Add(HeapGraphEdge* edge) { path_.Add(edge); }
  void Set(int index, HeapGraphEdge* edge) { path_[index] = edge; }
  const List<HeapGraphEdge*>* path() const { return &path_; }

  void Print();

 private:
  List<HeapGraphEdge*> path_;

  DISALLOW_COPY_AND_ASSIGN(HeapGraphPath);
};


class HeapEntriesMap {
 public:
  HeapEntriesMap();
  ~HeapEntriesMap();

  void Alias(HeapObject* object, HeapEntry* entry);
  void Apply(void (HeapEntry::*Func)(void));
  template<class Visitor>
  void Apply(Visitor* visitor);
  HeapEntry* Map(HeapObject* object);
  void Pair(HeapObject* object, HeapEntry* entry);

 private:
  INLINE(uint32_t Hash(HeapObject* object)) {
    return static_cast<uint32_t>(reinterpret_cast<intptr_t>(object));
  }
  INLINE(static bool HeapObjectsMatch(void* key1, void* key2)) {
    return key1 == key2;
  }
  INLINE(bool IsAlias(void* ptr)) {
    return reinterpret_cast<intptr_t>(ptr) & kAliasTag;
  }

  static const intptr_t kAliasTag = 1;

  HashMap entries_;

  DISALLOW_COPY_AND_ASSIGN(HeapEntriesMap);
};


class HeapSnapshotsCollection;

// HeapSnapshot represents a single heap snapshot. It is stored in
// HeapSnapshotsCollection, which is also a factory for
// HeapSnapshots. All HeapSnapshots share strings copied from JS heap
// to be able to return them even if they were collected.
// HeapSnapshotGenerator fills in a HeapSnapshot.
class HeapSnapshot {
 public:
  HeapSnapshot(HeapSnapshotsCollection* collection,
               const char* title,
               unsigned uid);
  void ClearPaint();
  void CutObjectsFromForeignSecurityContexts();
  HeapEntry* GetEntry(Object* object);
  void SetClosureReference(
      HeapEntry* parent, String* reference_name, Object* child);
  void SetElementReference(HeapEntry* parent, int index, Object* child);
  void SetPropertyReference(
      HeapEntry* parent, String* reference_name, Object* child);

  INLINE(const char* title() const) { return title_; }
  INLINE(unsigned uid() const) { return uid_; }
  const HeapEntry* const_root() const { return &root_; }
  HeapEntry* root() { return &root_; }
  template<class Visitor>
  void IterateEntries(Visitor* visitor) { entries_.Apply(visitor); }

  void Print(int max_depth);

 private:
  HeapEntry* AddEntry(HeapObject* object, HeapEntry::Type type) {
    return AddEntry(object, type, "");
  }
  HeapEntry* AddEntry(
      HeapObject* object, HeapEntry::Type type, const char* name);
  void AddEntryAlias(HeapObject* object, HeapEntry* entry) {
    entries_.Alias(object, entry);
  }
  HeapEntry* FindEntry(HeapObject* object) {
    return entries_.Map(object);
  }
  int GetGlobalSecurityToken();
  int GetObjectSecurityToken(HeapObject* obj);
  static int GetObjectSize(HeapObject* obj);
  static int CalculateNetworkSize(JSObject* obj);

  HeapSnapshotsCollection* collection_;
  const char* title_;
  unsigned uid_;
  HeapEntry root_;
  // HeapObject* -> HeapEntry*
  HeapEntriesMap entries_;

  DISALLOW_COPY_AND_ASSIGN(HeapSnapshot);
};


class HeapSnapshotsCollection {
 public:
  HeapSnapshotsCollection();
  ~HeapSnapshotsCollection();

  HeapSnapshot* NewSnapshot(const char* name, unsigned uid);
  List<HeapSnapshot*>* snapshots() { return &snapshots_; }
  HeapSnapshot* GetSnapshot(unsigned uid);

  const char* GetName(String* name) { return names_.GetName(name); }

  TokenEnumerator* token_enumerator() { return token_enumerator_; }

 private:
  INLINE(static bool HeapSnapshotsMatch(void* key1, void* key2)) {
    return key1 == key2;
  }

  List<HeapSnapshot*> snapshots_;
  // uid -> HeapSnapshot*
  HashMap snapshots_uids_;
  StringsStorage names_;
  TokenEnumerator* token_enumerator_;

  DISALLOW_COPY_AND_ASSIGN(HeapSnapshotsCollection);
};


class HeapSnapshotGenerator {
 public:
  explicit HeapSnapshotGenerator(HeapSnapshot* snapshot);
  void GenerateSnapshot();

 private:
  void ExtractReferences(HeapObject* obj);
  void ExtractClosureReferences(JSObject* js_obj, HeapEntry* entry);
  void ExtractPropertyReferences(JSObject* js_obj, HeapEntry* entry);
  void ExtractElementReferences(JSObject* js_obj, HeapEntry* entry);

  HeapSnapshot* snapshot_;

  DISALLOW_COPY_AND_ASSIGN(HeapSnapshotGenerator);
};

} }  // namespace v8::internal

#endif  // ENABLE_LOGGING_AND_PROFILING

#endif  // V8_PROFILE_GENERATOR_H_
