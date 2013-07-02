// Copyright 2011 the V8 project authors. All rights reserved.
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

#include "allocation.h"
#include "hashmap.h"
#include "../include/v8-profiler.h"

namespace v8 {
namespace internal {

struct OffsetRange;

class TokenEnumerator {
 public:
  TokenEnumerator();
  ~TokenEnumerator();
  int GetTokenId(Object* token);

  static const int kNoSecurityToken = -1;
  static const int kInheritsSecurityToken = -2;

 private:
  static void TokenRemovedCallback(v8::Isolate* isolate,
                                   v8::Persistent<v8::Value>* handle,
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

  const char* GetCopy(const char* src);
  const char* GetFormatted(const char* format, ...);
  const char* GetVFormatted(const char* format, va_list args);
  const char* GetName(Name* name);
  const char* GetName(int index);
  inline const char* GetFunctionName(Name* name);
  inline const char* GetFunctionName(const char* name);
  size_t GetUsedMemorySize() const;

 private:
  static const int kMaxNameSize = 1024;

  INLINE(static bool StringsMatch(void* key1, void* key2)) {
    return strcmp(reinterpret_cast<char*>(key1),
                  reinterpret_cast<char*>(key2)) == 0;
  }
  const char* AddOrDisposeString(char* str, uint32_t hash);

  // Mapping of strings by String::Hash to const char* strings.
  HashMap names_;

  DISALLOW_COPY_AND_ASSIGN(StringsStorage);
};


class CodeEntry {
 public:
  // CodeEntry doesn't own name strings, just references them.
  INLINE(CodeEntry(Logger::LogEventsAndTags tag,
                   const char* name,
                   int security_token_id = TokenEnumerator::kNoSecurityToken,
                   const char* name_prefix = CodeEntry::kEmptyNamePrefix,
                   const char* resource_name = CodeEntry::kEmptyResourceName,
                   int line_number = v8::CpuProfileNode::kNoLineNumberInfo));
  ~CodeEntry();

  INLINE(bool is_js_function() const) { return is_js_function_tag(tag_); }
  INLINE(const char* name_prefix() const) { return name_prefix_; }
  INLINE(bool has_name_prefix() const) { return name_prefix_[0] != '\0'; }
  INLINE(const char* name() const) { return name_; }
  INLINE(const char* resource_name() const) { return resource_name_; }
  INLINE(int line_number() const) { return line_number_; }
  INLINE(void set_shared_id(int shared_id)) { shared_id_ = shared_id; }
  INLINE(int script_id() const) { return script_id_; }
  INLINE(void set_script_id(int script_id)) { script_id_ = script_id; }
  INLINE(int security_token_id() const) { return security_token_id_; }

  INLINE(static bool is_js_function_tag(Logger::LogEventsAndTags tag));

  List<OffsetRange>* no_frame_ranges() const { return no_frame_ranges_; }
  void set_no_frame_ranges(List<OffsetRange>* ranges) {
    no_frame_ranges_ = ranges;
  }

  void SetBuiltinId(Builtins::Name id);
  Builtins::Name builtin_id() const { return builtin_id_; }

  void CopyData(const CodeEntry& source);
  uint32_t GetCallUid() const;
  bool IsSameAs(CodeEntry* entry) const;

  static const char* const kEmptyNamePrefix;
  static const char* const kEmptyResourceName;

 private:
  Logger::LogEventsAndTags tag_ : 8;
  Builtins::Name builtin_id_ : 8;
  const char* name_prefix_;
  const char* name_;
  const char* resource_name_;
  int line_number_;
  int shared_id_;
  int script_id_;
  int security_token_id_;
  List<OffsetRange>* no_frame_ranges_;

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
  unsigned id() const { return id_; }

  void Print(int indent);

 private:
  INLINE(static bool CodeEntriesMatch(void* entry1, void* entry2)) {
    return reinterpret_cast<CodeEntry*>(entry1)->IsSameAs(
        reinterpret_cast<CodeEntry*>(entry2));
  }

  INLINE(static uint32_t CodeEntryHash(CodeEntry* entry)) {
    return entry->GetCallUid();
  }

  ProfileTree* tree_;
  CodeEntry* entry_;
  unsigned total_ticks_;
  unsigned self_ticks_;
  // Mapping from CodeEntry* to ProfileNode*
  HashMap children_;
  List<ProfileNode*> children_list_;
  unsigned id_;

  DISALLOW_COPY_AND_ASSIGN(ProfileNode);
};


class ProfileTree {
 public:
  ProfileTree();
  ~ProfileTree();

  ProfileNode* AddPathFromEnd(const Vector<CodeEntry*>& path);
  void AddPathFromStart(const Vector<CodeEntry*>& path);
  void CalculateTotalTicks();
  void FilteredClone(ProfileTree* src, int security_token_id);

  double TicksToMillis(unsigned ticks) const {
    return ticks * ms_to_ticks_scale_;
  }
  ProfileNode* root() const { return root_; }
  void SetTickRatePerMs(double ticks_per_ms);

  unsigned next_node_id() { return next_node_id_++; }

  void ShortPrint();
  void Print() {
    root_->Print(0);
  }

 private:
  template <typename Callback>
  void TraverseDepthFirst(Callback* callback);

  CodeEntry root_entry_;
  unsigned next_node_id_;
  ProfileNode* root_;
  double ms_to_ticks_scale_;

  DISALLOW_COPY_AND_ASSIGN(ProfileTree);
};


class CpuProfile {
 public:
  CpuProfile(const char* title, unsigned uid, bool record_samples)
      : title_(title), uid_(uid), record_samples_(record_samples) { }

  // Add pc -> ... -> main() call path to the profile.
  void AddPath(const Vector<CodeEntry*>& path);
  void CalculateTotalTicks();
  void SetActualSamplingRate(double actual_sampling_rate);
  CpuProfile* FilteredClone(int security_token_id);

  INLINE(const char* title() const) { return title_; }
  INLINE(unsigned uid() const) { return uid_; }
  INLINE(const ProfileTree* top_down() const) { return &top_down_; }

  INLINE(int samples_count() const) { return samples_.length(); }
  INLINE(ProfileNode* sample(int index) const) { return samples_.at(index); }

  void UpdateTicksScale();

  void ShortPrint();
  void Print();

 private:
  const char* title_;
  unsigned uid_;
  bool record_samples_;
  List<ProfileNode*> samples_;
  ProfileTree top_down_;

  DISALLOW_COPY_AND_ASSIGN(CpuProfile);
};


class CodeMap {
 public:
  CodeMap() : next_shared_id_(1) { }
  void AddCode(Address addr, CodeEntry* entry, unsigned size);
  void MoveCode(Address from, Address to);
  CodeEntry* FindEntry(Address addr, Address* start = NULL);
  int GetSharedId(Address addr);

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
    static const Value NoValue() { return CodeEntryInfo(NULL, 0); }
    static int Compare(const Key& a, const Key& b) {
      return a < b ? -1 : (a > b ? 1 : 0);
    }
  };
  typedef SplayTree<CodeTreeConfig> CodeTree;

  class CodeTreePrinter {
   public:
    void Call(const Address& key, const CodeEntryInfo& value);
  };

  void DeleteAllCoveredCode(Address start, Address end);

  // Fake CodeEntry pointer to distinguish shared function entries.
  static CodeEntry* const kSharedFunctionCodeEntry;

  CodeTree tree_;
  int next_shared_id_;

  DISALLOW_COPY_AND_ASSIGN(CodeMap);
};


class CpuProfilesCollection {
 public:
  CpuProfilesCollection();
  ~CpuProfilesCollection();

  bool StartProfiling(const char* title, unsigned uid, bool record_samples);
  CpuProfile* StopProfiling(int security_token_id,
                            const char* title,
                            double actual_sampling_rate);
  List<CpuProfile*>* Profiles(int security_token_id);
  const char* GetName(Name* name) {
    return function_and_resource_names_.GetName(name);
  }
  const char* GetName(int args_count) {
    return function_and_resource_names_.GetName(args_count);
  }
  const char* GetFunctionName(Name* name) {
    return function_and_resource_names_.GetFunctionName(name);
  }
  const char* GetFunctionName(const char* name) {
    return function_and_resource_names_.GetFunctionName(name);
  }
  CpuProfile* GetProfile(int security_token_id, unsigned uid);
  bool IsLastProfile(const char* title);
  void RemoveProfile(CpuProfile* profile);
  bool HasDetachedProfiles() { return detached_profiles_.length() > 0; }

  CodeEntry* NewCodeEntry(
      Logger::LogEventsAndTags tag,
      const char* name,
      int security_token_id = TokenEnumerator::kNoSecurityToken,
      const char* name_prefix = CodeEntry::kEmptyNamePrefix,
      const char* resource_name = CodeEntry::kEmptyResourceName,
      int line_number = v8::CpuProfileNode::kNoLineNumberInfo);

  // Called from profile generator thread.
  void AddPathToCurrentProfiles(const Vector<CodeEntry*>& path);

  // Limits the number of profiles that can be simultaneously collected.
  static const int kMaxSimultaneousProfiles = 100;

 private:
  int GetProfileIndex(unsigned uid);
  List<CpuProfile*>* GetProfilesList(int security_token_id);
  int TokenToIndex(int security_token_id);

  INLINE(static bool UidsMatch(void* key1, void* key2)) {
    return key1 == key2;
  }

  StringsStorage function_and_resource_names_;
  List<CodeEntry*> code_entries_;
  List<List<CpuProfile*>* > profiles_by_token_;
  // Mapping from profiles' uids to indexes in the second nested list
  // of profiles_by_token_.
  HashMap profiles_uids_;
  List<CpuProfile*> detached_profiles_;

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

  void RecordTickSample(const TickSample& sample);

  INLINE(CodeMap* code_map()) { return &code_map_; }

  INLINE(void Tick()) { sample_rate_calc_.Tick(); }
  INLINE(double actual_sampling_rate()) {
    return sample_rate_calc_.ticks_per_ms();
  }

  static const char* const kAnonymousFunctionName;
  static const char* const kProgramEntryName;
  static const char* const kGarbageCollectorEntryName;
  // Used to represent frames for which we have no reliable way to
  // detect function.
  static const char* const kUnresolvedFunctionName;

 private:
  INLINE(CodeEntry* EntryForVMState(StateTag tag));

  CpuProfilesCollection* profiles_;
  CodeMap code_map_;
  CodeEntry* program_entry_;
  CodeEntry* gc_entry_;
  CodeEntry* unresolved_entry_;
  SampleRateCalculator sample_rate_calc_;

  DISALLOW_COPY_AND_ASSIGN(ProfileGenerator);
};


} }  // namespace v8::internal

#endif  // V8_PROFILE_GENERATOR_H_
