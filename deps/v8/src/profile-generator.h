// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROFILE_GENERATOR_H_
#define V8_PROFILE_GENERATOR_H_

#include <map>
#include "include/v8-profiler.h"
#include "src/allocation.h"
#include "src/compiler.h"
#include "src/hashmap.h"
#include "src/strings-storage.h"

namespace v8 {
namespace internal {

struct OffsetRange;

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
  inline CodeEntry(Logger::LogEventsAndTags tag, const char* name,
                   const char* name_prefix = CodeEntry::kEmptyNamePrefix,
                   const char* resource_name = CodeEntry::kEmptyResourceName,
                   int line_number = v8::CpuProfileNode::kNoLineNumberInfo,
                   int column_number = v8::CpuProfileNode::kNoColumnNumberInfo,
                   JITLineInfoTable* line_info = NULL,
                   Address instruction_start = NULL);
  ~CodeEntry();

  bool is_js_function() const { return is_js_function_tag(tag()); }
  const char* name_prefix() const { return name_prefix_; }
  bool has_name_prefix() const { return name_prefix_[0] != '\0'; }
  const char* name() const { return name_; }
  const char* resource_name() const { return resource_name_; }
  int line_number() const { return line_number_; }
  int column_number() const { return column_number_; }
  const JITLineInfoTable* line_info() const { return line_info_; }
  int script_id() const { return script_id_; }
  void set_script_id(int script_id) { script_id_ = script_id; }
  int position() const { return position_; }
  void set_position(int position) { position_ = position; }
  void set_bailout_reason(const char* bailout_reason) {
    bailout_reason_ = bailout_reason;
  }
  const char* bailout_reason() const { return bailout_reason_; }

  void set_deopt_info(const char* deopt_reason, SourcePosition position,
                      size_t pc_offset) {
    DCHECK(deopt_position_.IsUnknown());
    deopt_reason_ = deopt_reason;
    deopt_position_ = position;
    pc_offset_ = pc_offset;
  }
  CpuProfileDeoptInfo GetDeoptInfo();
  const char* deopt_reason() const { return deopt_reason_; }
  SourcePosition deopt_position() const { return deopt_position_; }
  bool has_deopt_info() const { return !deopt_position_.IsUnknown(); }
  void clear_deopt_info() {
    deopt_reason_ = kNoDeoptReason;
    deopt_position_ = SourcePosition::Unknown();
  }

  void FillFunctionInfo(SharedFunctionInfo* shared);

  static inline bool is_js_function_tag(Logger::LogEventsAndTags tag);

  List<OffsetRange>* no_frame_ranges() const { return no_frame_ranges_; }
  void set_no_frame_ranges(List<OffsetRange>* ranges) {
    no_frame_ranges_ = ranges;
  }
  void set_inlined_function_infos(
      const std::vector<InlinedFunctionInfo>& infos) {
    inlined_function_infos_ = infos;
  }
  const std::vector<InlinedFunctionInfo> inlined_function_infos() {
    return inlined_function_infos_;
  }

  void SetBuiltinId(Builtins::Name id);
  Builtins::Name builtin_id() const {
    return BuiltinIdField::decode(bit_field_);
  }

  uint32_t GetHash() const;
  bool IsSameFunctionAs(CodeEntry* entry) const;

  int GetSourceLine(int pc_offset) const;

  Address instruction_start() const { return instruction_start_; }

  static const char* const kEmptyNamePrefix;
  static const char* const kEmptyResourceName;
  static const char* const kEmptyBailoutReason;
  static const char* const kNoDeoptReason;

 private:
  class TagField : public BitField<Logger::LogEventsAndTags, 0, 8> {};
  class BuiltinIdField : public BitField<Builtins::Name, 8, 8> {};
  Logger::LogEventsAndTags tag() const { return TagField::decode(bit_field_); }

  uint32_t bit_field_;
  const char* name_prefix_;
  const char* name_;
  const char* resource_name_;
  int line_number_;
  int column_number_;
  int script_id_;
  int position_;
  List<OffsetRange>* no_frame_ranges_;
  const char* bailout_reason_;
  const char* deopt_reason_;
  SourcePosition deopt_position_;
  size_t pc_offset_;
  JITLineInfoTable* line_info_;
  Address instruction_start_;

  std::vector<InlinedFunctionInfo> inlined_function_infos_;

  DISALLOW_COPY_AND_ASSIGN(CodeEntry);
};


class ProfileTree;

class ProfileNode {
 public:
  inline ProfileNode(ProfileTree* tree, CodeEntry* entry);

  ProfileNode* FindChild(CodeEntry* entry);
  ProfileNode* FindOrAddChild(CodeEntry* entry);
  void IncrementSelfTicks() { ++self_ticks_; }
  void IncreaseSelfTicks(unsigned amount) { self_ticks_ += amount; }
  void IncrementLineTicks(int src_line);

  CodeEntry* entry() const { return entry_; }
  unsigned self_ticks() const { return self_ticks_; }
  const List<ProfileNode*>* children() const { return &children_list_; }
  unsigned id() const { return id_; }
  unsigned function_id() const;
  unsigned int GetHitLineCount() const { return line_ticks_.occupancy(); }
  bool GetLineTicks(v8::CpuProfileNode::LineTick* entries,
                    unsigned int length) const;
  void CollectDeoptInfo(CodeEntry* entry);
  const std::vector<CpuProfileDeoptInfo>& deopt_infos() const {
    return deopt_infos_;
  }

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
  HashMap children_;
  List<ProfileNode*> children_list_;
  unsigned id_;
  HashMap line_ticks_;

  std::vector<CpuProfileDeoptInfo> deopt_infos_;

  DISALLOW_COPY_AND_ASSIGN(ProfileNode);
};


class ProfileTree {
 public:
  ProfileTree();
  ~ProfileTree();

  ProfileNode* AddPathFromEnd(
      const Vector<CodeEntry*>& path,
      int src_line = v8::CpuProfileNode::kNoLineNumberInfo);
  ProfileNode* root() const { return root_; }
  unsigned next_node_id() { return next_node_id_++; }
  unsigned GetFunctionId(const ProfileNode* node);

  void Print() {
    root_->Print(0);
  }

 private:
  template <typename Callback>
  void TraverseDepthFirst(Callback* callback);

  CodeEntry root_entry_;
  unsigned next_node_id_;
  ProfileNode* root_;

  unsigned next_function_id_;
  HashMap function_ids_;

  DISALLOW_COPY_AND_ASSIGN(ProfileTree);
};


class CpuProfile {
 public:
  CpuProfile(const char* title, bool record_samples);

  // Add pc -> ... -> main() call path to the profile.
  void AddPath(base::TimeTicks timestamp, const Vector<CodeEntry*>& path,
               int src_line);
  void CalculateTotalTicksAndSamplingRate();

  const char* title() const { return title_; }
  const ProfileTree* top_down() const { return &top_down_; }

  int samples_count() const { return samples_.length(); }
  ProfileNode* sample(int index) const { return samples_.at(index); }
  base::TimeTicks sample_timestamp(int index) const {
    return timestamps_.at(index);
  }

  base::TimeTicks start_time() const { return start_time_; }
  base::TimeTicks end_time() const { return end_time_; }

  void UpdateTicksScale();

  void Print();

 private:
  const char* title_;
  bool record_samples_;
  base::TimeTicks start_time_;
  base::TimeTicks end_time_;
  List<ProfileNode*> samples_;
  List<base::TimeTicks> timestamps_;
  ProfileTree top_down_;

  DISALLOW_COPY_AND_ASSIGN(CpuProfile);
};


class CodeMap {
 public:
  CodeMap() {}
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

  CodeTree tree_;

  DISALLOW_COPY_AND_ASSIGN(CodeMap);
};


class CpuProfilesCollection {
 public:
  explicit CpuProfilesCollection(Heap* heap);
  ~CpuProfilesCollection();

  bool StartProfiling(const char* title, bool record_samples);
  CpuProfile* StopProfiling(const char* title);
  List<CpuProfile*>* profiles() { return &finished_profiles_; }
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
  bool IsLastProfile(const char* title);
  void RemoveProfile(CpuProfile* profile);

  CodeEntry* NewCodeEntry(
      Logger::LogEventsAndTags tag, const char* name,
      const char* name_prefix = CodeEntry::kEmptyNamePrefix,
      const char* resource_name = CodeEntry::kEmptyResourceName,
      int line_number = v8::CpuProfileNode::kNoLineNumberInfo,
      int column_number = v8::CpuProfileNode::kNoColumnNumberInfo,
      JITLineInfoTable* line_info = NULL, Address instruction_start = NULL);

  // Called from profile generator thread.
  void AddPathToCurrentProfiles(base::TimeTicks timestamp,
                                const Vector<CodeEntry*>& path, int src_line);

  // Limits the number of profiles that can be simultaneously collected.
  static const int kMaxSimultaneousProfiles = 100;

 private:
  StringsStorage function_and_resource_names_;
  List<CodeEntry*> code_entries_;
  List<CpuProfile*> finished_profiles_;

  // Accessed by VM thread and profile generator thread.
  List<CpuProfile*> current_profiles_;
  base::Semaphore current_profiles_semaphore_;

  DISALLOW_COPY_AND_ASSIGN(CpuProfilesCollection);
};


class ProfileGenerator {
 public:
  explicit ProfileGenerator(CpuProfilesCollection* profiles);

  void RecordTickSample(const TickSample& sample);

  CodeMap* code_map() { return &code_map_; }

  static const char* const kProgramEntryName;
  static const char* const kIdleEntryName;
  static const char* const kGarbageCollectorEntryName;
  // Used to represent frames for which we have no reliable way to
  // detect function.
  static const char* const kUnresolvedFunctionName;

 private:
  CodeEntry* EntryForVMState(StateTag tag);

  CpuProfilesCollection* profiles_;
  CodeMap code_map_;
  CodeEntry* program_entry_;
  CodeEntry* idle_entry_;
  CodeEntry* gc_entry_;
  CodeEntry* unresolved_entry_;

  DISALLOW_COPY_AND_ASSIGN(ProfileGenerator);
};


} }  // namespace v8::internal

#endif  // V8_PROFILE_GENERATOR_H_
