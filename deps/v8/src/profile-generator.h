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

namespace v8 {
namespace internal {

class TokenEnumerator {
 public:
  TokenEnumerator();
  ~TokenEnumerator();
  int GetTokenId(Object* token);

 private:
  static void TokenRemovedCallback(v8::Persistent<v8::Value> handle,
                                   void* parameter);
  void TokenRemoved(Object** token_location);

  List<Object**> token_locations_;
  List<bool> token_removed_;

  friend class TokenEnumeratorTester;
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
  static const int kNoSecurityToken = -1;
  static const int kInheritsSecurityToken = -2;

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
  CpuProfile* GetProfile(int security_token_id, unsigned uid);
  inline bool is_last_profile();

  const char* GetName(String* name);
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

  INLINE(static bool StringsMatch(void* key1, void* key2)) {
    return strcmp(reinterpret_cast<char*>(key1),
                  reinterpret_cast<char*>(key2)) == 0;
  }

  INLINE(static bool UidsMatch(void* key1, void* key2)) {
    return key1 == key2;
  }

  // String::Hash -> const char*
  HashMap function_and_resource_names_;
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

} }  // namespace v8::internal

#endif  // ENABLE_LOGGING_AND_PROFILING

#endif  // V8_PROFILE_GENERATOR_H_
