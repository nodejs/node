// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROFILER_PROFILE_GENERATOR_H_
#define V8_PROFILER_PROFILE_GENERATOR_H_

#include <map>
#include <vector>

#include "include/v8-profiler.h"
#include "src/allocation.h"
#include "src/base/hashmap.h"
#include "src/log.h"
#include "src/profiler/strings-storage.h"
#include "src/source-position.h"

namespace v8 {
namespace internal {

struct TickSample;

// Provides a mapping from the offsets within generated code to
// the source line.
class JITLineInfoTable : public Malloced {
 public:
  JITLineInfoTable();
  ~JITLineInfoTable();

  void SetPosition(int pc_offset, int line);
  int GetSourceLineNumber(int pc_offset) const;

  bool empty() const { return pc_offset_map_.empty(); }

 private:
  // pc_offset -> source line
  typedef std::map<int, int> PcOffsetMap;
  PcOffsetMap pc_offset_map_;
  DISALLOW_COPY_AND_ASSIGN(JITLineInfoTable);
};


class CodeEntry {
 public:
  // CodeEntry doesn't own name strings, just references them.
  inline CodeEntry(CodeEventListener::LogEventsAndTags tag, const char* name,
                   const char* name_prefix = CodeEntry::kEmptyNamePrefix,
                   const char* resource_name = CodeEntry::kEmptyResourceName,
                   int line_number = v8::CpuProfileNode::kNoLineNumberInfo,
                   int column_number = v8::CpuProfileNode::kNoColumnNumberInfo,
                   std::unique_ptr<JITLineInfoTable> line_info = nullptr,
                   Address instruction_start = nullptr);

  const char* name_prefix() const { return name_prefix_; }
  bool has_name_prefix() const { return name_prefix_[0] != '\0'; }
  const char* name() const { return name_; }
  const char* resource_name() const { return resource_name_; }
  int line_number() const { return line_number_; }
  int column_number() const { return column_number_; }
  const JITLineInfoTable* line_info() const { return line_info_.get(); }
  int script_id() const { return script_id_; }
  void set_script_id(int script_id) { script_id_ = script_id; }
  int position() const { return position_; }
  void set_position(int position) { position_ = position; }
  void set_bailout_reason(const char* bailout_reason) {
    bailout_reason_ = bailout_reason;
  }
  const char* bailout_reason() const { return bailout_reason_; }

  void set_deopt_info(const char* deopt_reason, int deopt_id) {
    DCHECK(!has_deopt_info());
    deopt_reason_ = deopt_reason;
    deopt_id_ = deopt_id;
  }
  CpuProfileDeoptInfo GetDeoptInfo();
  bool has_deopt_info() const { return deopt_id_ != kNoDeoptimizationId; }
  void clear_deopt_info() {
    deopt_reason_ = kNoDeoptReason;
    deopt_id_ = kNoDeoptimizationId;
  }

  void FillFunctionInfo(SharedFunctionInfo* shared);

  void SetBuiltinId(Builtins::Name id);
  Builtins::Name builtin_id() const {
    return BuiltinIdField::decode(bit_field_);
  }

  uint32_t GetHash() const;
  bool IsSameFunctionAs(CodeEntry* entry) const;

  int GetSourceLine(int pc_offset) const;

  void AddInlineStack(int pc_offset,
                      std::vector<std::unique_ptr<CodeEntry>> inline_stack);
  const std::vector<std::unique_ptr<CodeEntry>>* GetInlineStack(
      int pc_offset) const;

  void AddDeoptInlinedFrames(int deopt_id, std::vector<CpuProfileDeoptFrame>);
  bool HasDeoptInlinedFramesFor(int deopt_id) const;

  Address instruction_start() const { return instruction_start_; }
  CodeEventListener::LogEventsAndTags tag() const {
    return TagField::decode(bit_field_);
  }

  static const char* const kEmptyNamePrefix;
  static const char* const kEmptyResourceName;
  static const char* const kEmptyBailoutReason;
  static const char* const kNoDeoptReason;

  static const char* const kProgramEntryName;
  static const char* const kIdleEntryName;
  static const char* const kGarbageCollectorEntryName;
  // Used to represent frames for which we have no reliable way to
  // detect function.
  static const char* const kUnresolvedFunctionName;

  V8_INLINE static CodeEntry* program_entry() {
    return kProgramEntry.Pointer();
  }
  V8_INLINE static CodeEntry* idle_entry() { return kIdleEntry.Pointer(); }
  V8_INLINE static CodeEntry* gc_entry() { return kGCEntry.Pointer(); }
  V8_INLINE static CodeEntry* unresolved_entry() {
    return kUnresolvedEntry.Pointer();
  }

 private:
  struct ProgramEntryCreateTrait {
    static CodeEntry* Create();
  };
  struct IdleEntryCreateTrait {
    static CodeEntry* Create();
  };
  struct GCEntryCreateTrait {
    static CodeEntry* Create();
  };
  struct UnresolvedEntryCreateTrait {
    static CodeEntry* Create();
  };

  static base::LazyDynamicInstance<CodeEntry, ProgramEntryCreateTrait>::type
      kProgramEntry;
  static base::LazyDynamicInstance<CodeEntry, IdleEntryCreateTrait>::type
      kIdleEntry;
  static base::LazyDynamicInstance<CodeEntry, GCEntryCreateTrait>::type
      kGCEntry;
  static base::LazyDynamicInstance<CodeEntry, UnresolvedEntryCreateTrait>::type
      kUnresolvedEntry;

  class TagField : public BitField<Logger::LogEventsAndTags, 0, 8> {};
  class BuiltinIdField : public BitField<Builtins::Name, 8, 24> {};

  uint32_t bit_field_;
  const char* name_prefix_;
  const char* name_;
  const char* resource_name_;
  int line_number_;
  int column_number_;
  int script_id_;
  int position_;
  const char* bailout_reason_;
  const char* deopt_reason_;
  int deopt_id_;
  std::unique_ptr<JITLineInfoTable> line_info_;
  Address instruction_start_;
  // Should be an unordered_map, but it doesn't currently work on Win & MacOS.
  std::map<int, std::vector<std::unique_ptr<CodeEntry>>> inline_locations_;
  std::map<int, std::vector<CpuProfileDeoptFrame>> deopt_inlined_frames_;

  DISALLOW_COPY_AND_ASSIGN(CodeEntry);
};


class ProfileTree;

class ProfileNode {
 public:
  inline ProfileNode(ProfileTree* tree, CodeEntry* entry, ProfileNode* parent);

  ProfileNode* FindChild(CodeEntry* entry);
  ProfileNode* FindOrAddChild(CodeEntry* entry);
  void IncrementSelfTicks() { ++self_ticks_; }
  void IncreaseSelfTicks(unsigned amount) { self_ticks_ += amount; }
  void IncrementLineTicks(int src_line);

  CodeEntry* entry() const { return entry_; }
  unsigned self_ticks() const { return self_ticks_; }
  const std::vector<ProfileNode*>* children() const { return &children_list_; }
  unsigned id() const { return id_; }
  unsigned function_id() const;
  ProfileNode* parent() const { return parent_; }
  unsigned int GetHitLineCount() const { return line_ticks_.occupancy(); }
  bool GetLineTicks(v8::CpuProfileNode::LineTick* entries,
                    unsigned int length) const;
  void CollectDeoptInfo(CodeEntry* entry);
  const std::vector<CpuProfileDeoptInfo>& deopt_infos() const {
    return deopt_infos_;
  }
  Isolate* isolate() const;

  void Print(int indent);

  static bool CodeEntriesMatch(void* entry1, void* entry2) {
    return reinterpret_cast<CodeEntry*>(entry1)
        ->IsSameFunctionAs(reinterpret_cast<CodeEntry*>(entry2));
  }

 private:
  static uint32_t CodeEntryHash(CodeEntry* entry) { return entry->GetHash(); }

  static bool LineTickMatch(void* a, void* b) { return a == b; }

  ProfileTree* tree_;
  CodeEntry* entry_;
  unsigned self_ticks_;
  // Mapping from CodeEntry* to ProfileNode*
  base::CustomMatcherHashMap children_;
  std::vector<ProfileNode*> children_list_;
  ProfileNode* parent_;
  unsigned id_;
  base::CustomMatcherHashMap line_ticks_;

  std::vector<CpuProfileDeoptInfo> deopt_infos_;

  DISALLOW_COPY_AND_ASSIGN(ProfileNode);
};


class ProfileTree {
 public:
  explicit ProfileTree(Isolate* isolate);
  ~ProfileTree();

  ProfileNode* AddPathFromEnd(
      const std::vector<CodeEntry*>& path,
      int src_line = v8::CpuProfileNode::kNoLineNumberInfo,
      bool update_stats = true);
  ProfileNode* root() const { return root_; }
  unsigned next_node_id() { return next_node_id_++; }
  unsigned GetFunctionId(const ProfileNode* node);

  void Print() {
    root_->Print(0);
  }

  Isolate* isolate() const { return isolate_; }

  void EnqueueNode(const ProfileNode* node) { pending_nodes_.push_back(node); }
  size_t pending_nodes_count() const { return pending_nodes_.size(); }
  std::vector<const ProfileNode*> TakePendingNodes() {
    return std::move(pending_nodes_);
  }

 private:
  template <typename Callback>
  void TraverseDepthFirst(Callback* callback);

  std::vector<const ProfileNode*> pending_nodes_;

  CodeEntry root_entry_;
  unsigned next_node_id_;
  ProfileNode* root_;
  Isolate* isolate_;

  unsigned next_function_id_;
  base::CustomMatcherHashMap function_ids_;

  DISALLOW_COPY_AND_ASSIGN(ProfileTree);
};


class CpuProfile {
 public:
  CpuProfile(CpuProfiler* profiler, const char* title, bool record_samples);

  // Add pc -> ... -> main() call path to the profile.
  void AddPath(base::TimeTicks timestamp, const std::vector<CodeEntry*>& path,
               int src_line, bool update_stats);
  void FinishProfile();

  const char* title() const { return title_; }
  const ProfileTree* top_down() const { return &top_down_; }

  int samples_count() const { return static_cast<int>(samples_.size()); }
  ProfileNode* sample(int index) const { return samples_.at(index); }
  base::TimeTicks sample_timestamp(int index) const {
    return timestamps_.at(index);
  }

  base::TimeTicks start_time() const { return start_time_; }
  base::TimeTicks end_time() const { return end_time_; }
  CpuProfiler* cpu_profiler() const { return profiler_; }

  void UpdateTicksScale();

  void Print();

 private:
  void StreamPendingTraceEvents();

  const char* title_;
  bool record_samples_;
  base::TimeTicks start_time_;
  base::TimeTicks end_time_;
  std::vector<ProfileNode*> samples_;
  std::vector<base::TimeTicks> timestamps_;
  ProfileTree top_down_;
  CpuProfiler* const profiler_;
  size_t streaming_next_sample_;

  DISALLOW_COPY_AND_ASSIGN(CpuProfile);
};

class CodeMap {
 public:
  CodeMap() {}

  void AddCode(Address addr, CodeEntry* entry, unsigned size);
  void MoveCode(Address from, Address to);
  CodeEntry* FindEntry(Address addr);
  void Print();

 private:
  struct CodeEntryInfo {
    CodeEntryInfo(CodeEntry* an_entry, unsigned a_size)
        : entry(an_entry), size(a_size) { }
    CodeEntry* entry;
    unsigned size;
  };

  void DeleteAllCoveredCode(Address start, Address end);

  std::map<Address, CodeEntryInfo> code_map_;

  DISALLOW_COPY_AND_ASSIGN(CodeMap);
};

class CpuProfilesCollection {
 public:
  explicit CpuProfilesCollection(Isolate* isolate);

  void set_cpu_profiler(CpuProfiler* profiler) { profiler_ = profiler; }
  bool StartProfiling(const char* title, bool record_samples);
  CpuProfile* StopProfiling(const char* title);
  std::vector<std::unique_ptr<CpuProfile>>* profiles() {
    return &finished_profiles_;
  }
  const char* GetName(Name* name) { return resource_names_.GetName(name); }
  bool IsLastProfile(const char* title);
  void RemoveProfile(CpuProfile* profile);

  // Called from profile generator thread.
  void AddPathToCurrentProfiles(base::TimeTicks timestamp,
                                const std::vector<CodeEntry*>& path,
                                int src_line, bool update_stats);

  // Limits the number of profiles that can be simultaneously collected.
  static const int kMaxSimultaneousProfiles = 100;

 private:
  StringsStorage resource_names_;
  std::vector<std::unique_ptr<CpuProfile>> finished_profiles_;
  CpuProfiler* profiler_;

  // Accessed by VM thread and profile generator thread.
  std::vector<std::unique_ptr<CpuProfile>> current_profiles_;
  base::Semaphore current_profiles_semaphore_;

  DISALLOW_COPY_AND_ASSIGN(CpuProfilesCollection);
};


class ProfileGenerator {
 public:
  explicit ProfileGenerator(CpuProfilesCollection* profiles);

  void RecordTickSample(const TickSample& sample);

  CodeMap* code_map() { return &code_map_; }

 private:
  CodeEntry* FindEntry(void* address);
  CodeEntry* EntryForVMState(StateTag tag);

  CpuProfilesCollection* profiles_;
  CodeMap code_map_;

  DISALLOW_COPY_AND_ASSIGN(ProfileGenerator);
};


}  // namespace internal
}  // namespace v8

#endif  // V8_PROFILER_PROFILE_GENERATOR_H_
