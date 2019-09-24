// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROFILER_PROFILE_GENERATOR_H_
#define V8_PROFILER_PROFILE_GENERATOR_H_

#include <atomic>
#include <deque>
#include <limits>
#include <map>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "include/v8-profiler.h"
#include "src/base/platform/time.h"
#include "src/builtins/builtins.h"
#include "src/codegen/source-position.h"
#include "src/logging/code-events.h"
#include "src/profiler/strings-storage.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {

struct TickSample;

// Provides a mapping from the offsets within generated code or a bytecode array
// to the source line and inlining id.
class V8_EXPORT_PRIVATE SourcePositionTable : public Malloced {
 public:
  SourcePositionTable() = default;

  void SetPosition(int pc_offset, int line, int inlining_id);
  int GetSourceLineNumber(int pc_offset) const;
  int GetInliningId(int pc_offset) const;

  void print() const;

 private:
  struct SourcePositionTuple {
    bool operator<(const SourcePositionTuple& other) const {
      return pc_offset < other.pc_offset;
    }
    int pc_offset;
    int line_number;
    int inlining_id;
  };
  // This is logically a map, but we store it as a vector of tuples, sorted by
  // the pc offset, so that we can save space and look up items using binary
  // search.
  std::vector<SourcePositionTuple> pc_offsets_to_lines_;
  DISALLOW_COPY_AND_ASSIGN(SourcePositionTable);
};

struct CodeEntryAndLineNumber;

class CodeEntry {
 public:
  // CodeEntry doesn't own name strings, just references them.
  inline CodeEntry(CodeEventListener::LogEventsAndTags tag, const char* name,
                   const char* resource_name = CodeEntry::kEmptyResourceName,
                   int line_number = v8::CpuProfileNode::kNoLineNumberInfo,
                   int column_number = v8::CpuProfileNode::kNoColumnNumberInfo,
                   std::unique_ptr<SourcePositionTable> line_info = nullptr,
                   Address instruction_start = kNullAddress,
                   bool is_shared_cross_origin = false);

  const char* name() const { return name_; }
  const char* resource_name() const { return resource_name_; }
  int line_number() const { return line_number_; }
  int column_number() const { return column_number_; }
  const SourcePositionTable* line_info() const { return line_info_.get(); }
  int script_id() const { return script_id_; }
  void set_script_id(int script_id) { script_id_ = script_id; }
  int position() const { return position_; }
  void set_position(int position) { position_ = position; }
  void set_bailout_reason(const char* bailout_reason) {
    EnsureRareData()->bailout_reason_ = bailout_reason;
  }
  const char* bailout_reason() const {
    return rare_data_ ? rare_data_->bailout_reason_ : kEmptyBailoutReason;
  }

  void set_deopt_info(const char* deopt_reason, int deopt_id,
                      std::vector<CpuProfileDeoptFrame> inlined_frames);

  CpuProfileDeoptInfo GetDeoptInfo();
  bool has_deopt_info() const {
    return rare_data_ && rare_data_->deopt_id_ != kNoDeoptimizationId;
  }
  void clear_deopt_info() {
    if (!rare_data_) return;
    // TODO(alph): Clear rare_data_ if that was the only field in use.
    rare_data_->deopt_reason_ = kNoDeoptReason;
    rare_data_->deopt_id_ = kNoDeoptimizationId;
  }
  void mark_used() { bit_field_ = UsedField::update(bit_field_, true); }
  bool used() const { return UsedField::decode(bit_field_); }

  void FillFunctionInfo(SharedFunctionInfo shared);

  void SetBuiltinId(Builtins::Name id);
  Builtins::Name builtin_id() const {
    return BuiltinIdField::decode(bit_field_);
  }

  bool is_shared_cross_origin() const {
    return SharedCrossOriginField::decode(bit_field_);
  }

  uint32_t GetHash() const;
  bool IsSameFunctionAs(const CodeEntry* entry) const;

  int GetSourceLine(int pc_offset) const;

  struct Equals {
    bool operator()(const std::unique_ptr<CodeEntry>& lhs,
                    const std::unique_ptr<CodeEntry>& rhs) const {
      return lhs.get()->IsSameFunctionAs(rhs.get());
    }
  };
  struct Hasher {
    std::size_t operator()(const std::unique_ptr<CodeEntry>& e) const {
      return e->GetHash();
    }
  };

  void SetInlineStacks(
      std::unordered_set<std::unique_ptr<CodeEntry>, Hasher, Equals>
          inline_entries,
      std::unordered_map<int, std::vector<CodeEntryAndLineNumber>>
          inline_stacks);
  const std::vector<CodeEntryAndLineNumber>* GetInlineStack(
      int pc_offset) const;

  void set_instruction_start(Address start) { instruction_start_ = start; }
  Address instruction_start() const { return instruction_start_; }

  CodeEventListener::LogEventsAndTags tag() const {
    return TagField::decode(bit_field_);
  }

  static const char* const kWasmResourceNamePrefix;
  V8_EXPORT_PRIVATE static const char* const kEmptyResourceName;
  static const char* const kEmptyBailoutReason;
  static const char* const kNoDeoptReason;

  V8_EXPORT_PRIVATE static const char* const kProgramEntryName;
  V8_EXPORT_PRIVATE static const char* const kIdleEntryName;
  static const char* const kGarbageCollectorEntryName;
  // Used to represent frames for which we have no reliable way to
  // detect function.
  V8_EXPORT_PRIVATE static const char* const kUnresolvedFunctionName;
  V8_EXPORT_PRIVATE static const char* const kRootEntryName;

  V8_INLINE static CodeEntry* program_entry() {
    return kProgramEntry.Pointer();
  }
  V8_INLINE static CodeEntry* idle_entry() { return kIdleEntry.Pointer(); }
  V8_INLINE static CodeEntry* gc_entry() { return kGCEntry.Pointer(); }
  V8_INLINE static CodeEntry* unresolved_entry() {
    return kUnresolvedEntry.Pointer();
  }
  V8_INLINE static CodeEntry* root_entry() { return kRootEntry.Pointer(); }

  void print() const;

 private:
  struct RareData {
    const char* deopt_reason_ = kNoDeoptReason;
    const char* bailout_reason_ = kEmptyBailoutReason;
    int deopt_id_ = kNoDeoptimizationId;
    std::unordered_map<int, std::vector<CodeEntryAndLineNumber>> inline_stacks_;
    std::unordered_set<std::unique_ptr<CodeEntry>, Hasher, Equals>
        inline_entries_;
    std::vector<CpuProfileDeoptFrame> deopt_inlined_frames_;
  };

  RareData* EnsureRareData();

  struct V8_EXPORT_PRIVATE ProgramEntryCreateTrait {
    static CodeEntry* Create();
  };
  struct V8_EXPORT_PRIVATE IdleEntryCreateTrait {
    static CodeEntry* Create();
  };
  struct V8_EXPORT_PRIVATE GCEntryCreateTrait {
    static CodeEntry* Create();
  };
  struct V8_EXPORT_PRIVATE UnresolvedEntryCreateTrait {
    static CodeEntry* Create();
  };
  struct V8_EXPORT_PRIVATE RootEntryCreateTrait {
    static CodeEntry* Create();
  };

  V8_EXPORT_PRIVATE static base::LazyDynamicInstance<
      CodeEntry, ProgramEntryCreateTrait>::type kProgramEntry;
  V8_EXPORT_PRIVATE static base::LazyDynamicInstance<
      CodeEntry, IdleEntryCreateTrait>::type kIdleEntry;
  V8_EXPORT_PRIVATE static base::LazyDynamicInstance<
      CodeEntry, GCEntryCreateTrait>::type kGCEntry;
  V8_EXPORT_PRIVATE static base::LazyDynamicInstance<
      CodeEntry, UnresolvedEntryCreateTrait>::type kUnresolvedEntry;
  V8_EXPORT_PRIVATE static base::LazyDynamicInstance<
      CodeEntry, RootEntryCreateTrait>::type kRootEntry;

  using TagField = BitField<CodeEventListener::LogEventsAndTags, 0, 8>;
  using BuiltinIdField = BitField<Builtins::Name, 8, 22>;
  static_assert(Builtins::builtin_count <= BuiltinIdField::kNumValues,
                "builtin_count exceeds size of bitfield");
  using UsedField = BitField<bool, 30, 1>;
  using SharedCrossOriginField = BitField<bool, 31, 1>;

  uint32_t bit_field_;
  const char* name_;
  const char* resource_name_;
  int line_number_;
  int column_number_;
  int script_id_;
  int position_;
  std::unique_ptr<SourcePositionTable> line_info_;
  Address instruction_start_;
  std::unique_ptr<RareData> rare_data_;

  DISALLOW_COPY_AND_ASSIGN(CodeEntry);
};

struct CodeEntryAndLineNumber {
  CodeEntry* code_entry;
  int line_number;
};

struct ProfileStackFrame {
  CodeEntryAndLineNumber entry;
  Address native_context;
  bool filterable;  // If true, the frame should be filtered by context (if a
                    // filter is present).
};

typedef std::vector<ProfileStackFrame> ProfileStackTrace;

// Filters stack frames from sources other than a target native context.
class ContextFilter {
 public:
  explicit ContextFilter(Address native_context_address)
      : native_context_address_(native_context_address) {}

  // Returns true if the stack frame passes a context check.
  bool Accept(const ProfileStackFrame&);

  // Invoked when a native context has changed address.
  void OnMoveEvent(Address from_address, Address to_address);

  // Update the context's tracked address based on VM-thread events.
  void set_native_context_address(Address address) {
    native_context_address_ = address;
  }
  Address native_context_address() const { return native_context_address_; }

 private:
  Address native_context_address_;
};

class ProfileTree;

class V8_EXPORT_PRIVATE ProfileNode {
 public:
  inline ProfileNode(ProfileTree* tree, CodeEntry* entry, ProfileNode* parent,
                     int line_number = 0);

  ProfileNode* FindChild(
      CodeEntry* entry,
      int line_number = v8::CpuProfileNode::kNoLineNumberInfo);
  ProfileNode* FindOrAddChild(CodeEntry* entry, int line_number = 0);
  void IncrementSelfTicks() { ++self_ticks_; }
  void IncreaseSelfTicks(unsigned amount) { self_ticks_ += amount; }
  void IncrementLineTicks(int src_line);

  CodeEntry* entry() const { return entry_; }
  unsigned self_ticks() const { return self_ticks_; }
  const std::vector<ProfileNode*>* children() const { return &children_list_; }
  unsigned id() const { return id_; }
  unsigned function_id() const;
  ProfileNode* parent() const { return parent_; }
  int line_number() const {
    return line_number_ != 0 ? line_number_ : entry_->line_number();
  }
  CpuProfileNode::SourceType source_type() const;

  unsigned int GetHitLineCount() const {
    return static_cast<unsigned int>(line_ticks_.size());
  }
  bool GetLineTicks(v8::CpuProfileNode::LineTick* entries,
                    unsigned int length) const;
  void CollectDeoptInfo(CodeEntry* entry);
  const std::vector<CpuProfileDeoptInfo>& deopt_infos() const {
    return deopt_infos_;
  }
  Isolate* isolate() const;

  void Print(int indent);

 private:
  struct Equals {
    bool operator()(CodeEntryAndLineNumber lhs,
                    CodeEntryAndLineNumber rhs) const {
      return lhs.code_entry->IsSameFunctionAs(rhs.code_entry) &&
             lhs.line_number == rhs.line_number;
    }
  };
  struct Hasher {
    std::size_t operator()(CodeEntryAndLineNumber pair) const {
      return pair.code_entry->GetHash() ^ ComputeUnseededHash(pair.line_number);
    }
  };

  ProfileTree* tree_;
  CodeEntry* entry_;
  unsigned self_ticks_;
  std::unordered_map<CodeEntryAndLineNumber, ProfileNode*, Hasher, Equals>
      children_;
  int line_number_;
  std::vector<ProfileNode*> children_list_;
  ProfileNode* parent_;
  unsigned id_;
  // maps line number --> number of ticks
  std::unordered_map<int, int> line_ticks_;

  std::vector<CpuProfileDeoptInfo> deopt_infos_;

  DISALLOW_COPY_AND_ASSIGN(ProfileNode);
};

class V8_EXPORT_PRIVATE ProfileTree {
 public:
  explicit ProfileTree(Isolate* isolate);
  ~ProfileTree();

  using ProfilingMode = v8::CpuProfilingMode;

  ProfileNode* AddPathFromEnd(
      const std::vector<CodeEntry*>& path,
      int src_line = v8::CpuProfileNode::kNoLineNumberInfo,
      bool update_stats = true);
  ProfileNode* AddPathFromEnd(
      const ProfileStackTrace& path,
      int src_line = v8::CpuProfileNode::kNoLineNumberInfo,
      bool update_stats = true,
      ProfilingMode mode = ProfilingMode::kLeafNodeLineNumbers,
      ContextFilter* context_filter = nullptr);
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

  unsigned next_node_id_;
  ProfileNode* root_;
  Isolate* isolate_;

  unsigned next_function_id_;
  std::unordered_map<CodeEntry*, unsigned> function_ids_;

  DISALLOW_COPY_AND_ASSIGN(ProfileTree);
};

class CpuProfiler;

class CpuProfile {
 public:
  struct SampleInfo {
    ProfileNode* node;
    base::TimeTicks timestamp;
    int line;
  };

  V8_EXPORT_PRIVATE CpuProfile(CpuProfiler* profiler, const char* title,
                               CpuProfilingOptions options);

  // Checks whether or not the given TickSample should be (sub)sampled, given
  // the sampling interval of the profiler that recorded it (in microseconds).
  V8_EXPORT_PRIVATE bool CheckSubsample(base::TimeDelta sampling_interval);
  // Add pc -> ... -> main() call path to the profile.
  void AddPath(base::TimeTicks timestamp, const ProfileStackTrace& path,
               int src_line, bool update_stats,
               base::TimeDelta sampling_interval);
  void FinishProfile();

  const char* title() const { return title_; }
  const ProfileTree* top_down() const { return &top_down_; }

  int samples_count() const { return static_cast<int>(samples_.size()); }
  const SampleInfo& sample(int index) const { return samples_[index]; }

  int64_t sampling_interval_us() const {
    return options_.sampling_interval_us();
  }

  base::TimeTicks start_time() const { return start_time_; }
  base::TimeTicks end_time() const { return end_time_; }
  CpuProfiler* cpu_profiler() const { return profiler_; }
  ContextFilter* context_filter() const { return context_filter_.get(); }

  void UpdateTicksScale();

  V8_EXPORT_PRIVATE void Print();

 private:
  void StreamPendingTraceEvents();

  const char* title_;
  const CpuProfilingOptions options_;
  std::unique_ptr<ContextFilter> context_filter_;
  base::TimeTicks start_time_;
  base::TimeTicks end_time_;
  std::deque<SampleInfo> samples_;
  ProfileTree top_down_;
  CpuProfiler* const profiler_;
  size_t streaming_next_sample_;
  uint32_t id_;
  // Number of microseconds worth of profiler ticks that should elapse before
  // the next sample is recorded.
  base::TimeDelta next_sample_delta_;

  static std::atomic<uint32_t> last_id_;

  DISALLOW_COPY_AND_ASSIGN(CpuProfile);
};

class V8_EXPORT_PRIVATE CodeMap {
 public:
  CodeMap();
  ~CodeMap();

  void AddCode(Address addr, CodeEntry* entry, unsigned size);
  void MoveCode(Address from, Address to);
  CodeEntry* FindEntry(Address addr);
  void Print();

 private:
  struct CodeEntryMapInfo {
    unsigned index;
    unsigned size;
  };

  union CodeEntrySlotInfo {
    CodeEntry* entry;
    unsigned next_free_slot;
  };

  static constexpr unsigned kNoFreeSlot = std::numeric_limits<unsigned>::max();

  void ClearCodesInRange(Address start, Address end);
  unsigned AddCodeEntry(Address start, CodeEntry*);
  void DeleteCodeEntry(unsigned index);

  CodeEntry* entry(unsigned index) { return code_entries_[index].entry; }

  std::deque<CodeEntrySlotInfo> code_entries_;
  std::map<Address, CodeEntryMapInfo> code_map_;
  unsigned free_list_head_ = kNoFreeSlot;

  DISALLOW_COPY_AND_ASSIGN(CodeMap);
};

class V8_EXPORT_PRIVATE CpuProfilesCollection {
 public:
  explicit CpuProfilesCollection(Isolate* isolate);

  void set_cpu_profiler(CpuProfiler* profiler) { profiler_ = profiler; }
  bool StartProfiling(const char* title, CpuProfilingOptions options = {});

  CpuProfile* StopProfiling(const char* title);
  std::vector<std::unique_ptr<CpuProfile>>* profiles() {
    return &finished_profiles_;
  }
  const char* GetName(Name name) { return resource_names_.GetName(name); }
  bool IsLastProfile(const char* title);
  void RemoveProfile(CpuProfile* profile);

  // Finds a common sampling interval dividing each CpuProfile's interval,
  // rounded up to the nearest multiple of the CpuProfiler's sampling interval.
  // Returns 0 if no profiles are attached.
  base::TimeDelta GetCommonSamplingInterval() const;

  // Called from profile generator thread.
  void AddPathToCurrentProfiles(base::TimeTicks timestamp,
                                const ProfileStackTrace& path, int src_line,
                                bool update_stats,
                                base::TimeDelta sampling_interval);

  // Called from profile generator thread.
  void UpdateNativeContextAddressForCurrentProfiles(Address from, Address to);

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

class V8_EXPORT_PRIVATE ProfileGenerator {
 public:
  explicit ProfileGenerator(CpuProfilesCollection* profiles, CodeMap* code_map);

  void RecordTickSample(const TickSample& sample);

  void UpdateNativeContextAddress(Address from, Address to);

  CodeMap* code_map() { return code_map_; }

 private:
  CodeEntry* FindEntry(Address address);
  CodeEntry* EntryForVMState(StateTag tag);

  CpuProfilesCollection* profiles_;
  CodeMap* const code_map_;

  DISALLOW_COPY_AND_ASSIGN(ProfileGenerator);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PROFILER_PROFILE_GENERATOR_H_
