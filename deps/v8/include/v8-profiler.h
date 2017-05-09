// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_V8_PROFILER_H_
#define V8_V8_PROFILER_H_

#include <unordered_set>
#include <vector>
#include "v8.h"  // NOLINT(build/include)

/**
 * Profiler support for the V8 JavaScript engine.
 */
namespace v8 {

class HeapGraphNode;
struct HeapStatsUpdate;

typedef uint32_t SnapshotObjectId;


struct CpuProfileDeoptFrame {
  int script_id;
  size_t position;
};

}  // namespace v8

#ifdef V8_OS_WIN
template class V8_EXPORT std::vector<v8::CpuProfileDeoptFrame>;
#endif

namespace v8 {

struct V8_EXPORT CpuProfileDeoptInfo {
  /** A pointer to a static string owned by v8. */
  const char* deopt_reason;
  std::vector<CpuProfileDeoptFrame> stack;
};

}  // namespace v8

#ifdef V8_OS_WIN
template class V8_EXPORT std::vector<v8::CpuProfileDeoptInfo>;
#endif

namespace v8 {

/**
 * TracingCpuProfiler monitors tracing being enabled/disabled
 * and emits CpuProfile trace events once v8.cpu_profiler tracing category
 * is enabled. It has no overhead unless the category is enabled.
 */
class V8_EXPORT TracingCpuProfiler {
 public:
  static std::unique_ptr<TracingCpuProfiler> Create(Isolate*);
  virtual ~TracingCpuProfiler() = default;

 protected:
  TracingCpuProfiler() = default;
};

// TickSample captures the information collected for each sample.
struct TickSample {
  // Internal profiling (with --prof + tools/$OS-tick-processor) wants to
  // include the runtime function we're calling. Externally exposed tick
  // samples don't care.
  enum RecordCEntryFrame { kIncludeCEntryFrame, kSkipCEntryFrame };

  TickSample()
      : state(OTHER),
        pc(nullptr),
        external_callback_entry(nullptr),
        frames_count(0),
        has_external_callback(false),
        update_stats(true) {}

  /**
   * Initialize a tick sample from the isolate.
   * \param isolate The isolate.
   * \param state Execution state.
   * \param record_c_entry_frame Include or skip the runtime function.
   * \param update_stats Whether update the sample to the aggregated stats.
   * \param use_simulator_reg_state When set to true and V8 is running under a
   *                                simulator, the method will use the simulator
   *                                register state rather than the one provided
   *                                with |state| argument. Otherwise the method
   *                                will use provided register |state| as is.
   */
  void Init(Isolate* isolate, const v8::RegisterState& state,
            RecordCEntryFrame record_c_entry_frame, bool update_stats,
            bool use_simulator_reg_state = true);
  /**
   * Get a call stack sample from the isolate.
   * \param isolate The isolate.
   * \param state Register state.
   * \param record_c_entry_frame Include or skip the runtime function.
   * \param frames Caller allocated buffer to store stack frames.
   * \param frames_limit Maximum number of frames to capture. The buffer must
   *                     be large enough to hold the number of frames.
   * \param sample_info The sample info is filled up by the function
   *                    provides number of actual captured stack frames and
   *                    the current VM state.
   * \param use_simulator_reg_state When set to true and V8 is running under a
   *                                simulator, the method will use the simulator
   *                                register state rather than the one provided
   *                                with |state| argument. Otherwise the method
   *                                will use provided register |state| as is.
   * \note GetStackSample is thread and signal safe and should only be called
   *                      when the JS thread is paused or interrupted.
   *                      Otherwise the behavior is undefined.
   */
  static bool GetStackSample(Isolate* isolate, v8::RegisterState* state,
                             RecordCEntryFrame record_c_entry_frame,
                             void** frames, size_t frames_limit,
                             v8::SampleInfo* sample_info,
                             bool use_simulator_reg_state = true);
  StateTag state;  // The state of the VM.
  void* pc;        // Instruction pointer.
  union {
    void* tos;  // Top stack value (*sp).
    void* external_callback_entry;
  };
  static const unsigned kMaxFramesCountLog2 = 8;
  static const unsigned kMaxFramesCount = (1 << kMaxFramesCountLog2) - 1;
  void* stack[kMaxFramesCount];                 // Call stack.
  unsigned frames_count : kMaxFramesCountLog2;  // Number of captured frames.
  bool has_external_callback : 1;
  bool update_stats : 1;  // Whether the sample should update aggregated stats.
};

/**
 * CpuProfileNode represents a node in a call graph.
 */
class V8_EXPORT CpuProfileNode {
 public:
  struct LineTick {
    /** The 1-based number of the source line where the function originates. */
    int line;

    /** The count of samples associated with the source line. */
    unsigned int hit_count;
  };

  /** Returns function name (empty string for anonymous functions.) */
  Local<String> GetFunctionName() const;

  /**
   * Returns function name (empty string for anonymous functions.)
   * The string ownership is *not* passed to the caller. It stays valid until
   * profile is deleted. The function is thread safe.
   */
  const char* GetFunctionNameStr() const;

  /** Returns id of the script where function is located. */
  int GetScriptId() const;

  /** Returns resource name for script from where the function originates. */
  Local<String> GetScriptResourceName() const;

  /**
   * Returns resource name for script from where the function originates.
   * The string ownership is *not* passed to the caller. It stays valid until
   * profile is deleted. The function is thread safe.
   */
  const char* GetScriptResourceNameStr() const;

  /**
   * Returns the number, 1-based, of the line where the function originates.
   * kNoLineNumberInfo if no line number information is available.
   */
  int GetLineNumber() const;

  /**
   * Returns 1-based number of the column where the function originates.
   * kNoColumnNumberInfo if no column number information is available.
   */
  int GetColumnNumber() const;

  /**
   * Returns the number of the function's source lines that collect the samples.
   */
  unsigned int GetHitLineCount() const;

  /** Returns the set of source lines that collect the samples.
   *  The caller allocates buffer and responsible for releasing it.
   *  True if all available entries are copied, otherwise false.
   *  The function copies nothing if buffer is not large enough.
   */
  bool GetLineTicks(LineTick* entries, unsigned int length) const;

  /** Returns bailout reason for the function
    * if the optimization was disabled for it.
    */
  const char* GetBailoutReason() const;

  /**
    * Returns the count of samples where the function was currently executing.
    */
  unsigned GetHitCount() const;

  /** Returns function entry UID. */
  V8_DEPRECATE_SOON(
      "Use GetScriptId, GetLineNumber, and GetColumnNumber instead.",
      unsigned GetCallUid() const);

  /** Returns id of the node. The id is unique within the tree */
  unsigned GetNodeId() const;

  /** Returns child nodes count of the node. */
  int GetChildrenCount() const;

  /** Retrieves a child node by index. */
  const CpuProfileNode* GetChild(int index) const;

  /** Retrieves deopt infos for the node. */
  const std::vector<CpuProfileDeoptInfo>& GetDeoptInfos() const;

  static const int kNoLineNumberInfo = Message::kNoLineNumberInfo;
  static const int kNoColumnNumberInfo = Message::kNoColumnInfo;
};


/**
 * CpuProfile contains a CPU profile in a form of top-down call tree
 * (from main() down to functions that do all the work).
 */
class V8_EXPORT CpuProfile {
 public:
  /** Returns CPU profile title. */
  Local<String> GetTitle() const;

  /** Returns the root node of the top down call tree. */
  const CpuProfileNode* GetTopDownRoot() const;

  /**
   * Returns number of samples recorded. The samples are not recorded unless
   * |record_samples| parameter of CpuProfiler::StartCpuProfiling is true.
   */
  int GetSamplesCount() const;

  /**
   * Returns profile node corresponding to the top frame the sample at
   * the given index.
   */
  const CpuProfileNode* GetSample(int index) const;

  /**
   * Returns the timestamp of the sample. The timestamp is the number of
   * microseconds since some unspecified starting point.
   * The point is equal to the starting point used by GetStartTime.
   */
  int64_t GetSampleTimestamp(int index) const;

  /**
   * Returns time when the profile recording was started (in microseconds)
   * since some unspecified starting point.
   */
  int64_t GetStartTime() const;

  /**
   * Returns time when the profile recording was stopped (in microseconds)
   * since some unspecified starting point.
   * The point is equal to the starting point used by GetStartTime.
   */
  int64_t GetEndTime() const;

  /**
   * Deletes the profile and removes it from CpuProfiler's list.
   * All pointers to nodes previously returned become invalid.
   */
  void Delete();
};

/**
 * Interface for controlling CPU profiling. Instance of the
 * profiler can be created using v8::CpuProfiler::New method.
 */
class V8_EXPORT CpuProfiler {
 public:
  /**
   * Creates a new CPU profiler for the |isolate|. The isolate must be
   * initialized. The profiler object must be disposed after use by calling
   * |Dispose| method.
   */
  static CpuProfiler* New(Isolate* isolate);

  /**
   * Disposes the CPU profiler object.
   */
  void Dispose();

  /**
   * Changes default CPU profiler sampling interval to the specified number
   * of microseconds. Default interval is 1000us. This method must be called
   * when there are no profiles being recorded.
   */
  void SetSamplingInterval(int us);

  /**
   * Starts collecting CPU profile. Title may be an empty string. It
   * is allowed to have several profiles being collected at
   * once. Attempts to start collecting several profiles with the same
   * title are silently ignored. While collecting a profile, functions
   * from all security contexts are included in it. The token-based
   * filtering is only performed when querying for a profile.
   *
   * |record_samples| parameter controls whether individual samples should
   * be recorded in addition to the aggregated tree.
   */
  void StartProfiling(Local<String> title, bool record_samples = false);

  /**
   * Stops collecting CPU profile with a given title and returns it.
   * If the title given is empty, finishes the last profile started.
   */
  CpuProfile* StopProfiling(Local<String> title);

  /**
   * Force collection of a sample. Must be called on the VM thread.
   * Recording the forced sample does not contribute to the aggregated
   * profile statistics.
   */
  void CollectSample();

  /**
   * Tells the profiler whether the embedder is idle.
   */
  void SetIdle(bool is_idle);

 private:
  CpuProfiler();
  ~CpuProfiler();
  CpuProfiler(const CpuProfiler&);
  CpuProfiler& operator=(const CpuProfiler&);
};


/**
 * HeapSnapshotEdge represents a directed connection between heap
 * graph nodes: from retainers to retained nodes.
 */
class V8_EXPORT HeapGraphEdge {
 public:
  enum Type {
    kContextVariable = 0,  // A variable from a function context.
    kElement = 1,          // An element of an array.
    kProperty = 2,         // A named object property.
    kInternal = 3,         // A link that can't be accessed from JS,
                           // thus, its name isn't a real property name
                           // (e.g. parts of a ConsString).
    kHidden = 4,           // A link that is needed for proper sizes
                           // calculation, but may be hidden from user.
    kShortcut = 5,         // A link that must not be followed during
                           // sizes calculation.
    kWeak = 6              // A weak reference (ignored by the GC).
  };

  /** Returns edge type (see HeapGraphEdge::Type). */
  Type GetType() const;

  /**
   * Returns edge name. This can be a variable name, an element index, or
   * a property name.
   */
  Local<Value> GetName() const;

  /** Returns origin node. */
  const HeapGraphNode* GetFromNode() const;

  /** Returns destination node. */
  const HeapGraphNode* GetToNode() const;
};


/**
 * HeapGraphNode represents a node in a heap graph.
 */
class V8_EXPORT HeapGraphNode {
 public:
  enum Type {
    kHidden = 0,         // Hidden node, may be filtered when shown to user.
    kArray = 1,          // An array of elements.
    kString = 2,         // A string.
    kObject = 3,         // A JS object (except for arrays and strings).
    kCode = 4,           // Compiled code.
    kClosure = 5,        // Function closure.
    kRegExp = 6,         // RegExp.
    kHeapNumber = 7,     // Number stored in the heap.
    kNative = 8,         // Native object (not from V8 heap).
    kSynthetic = 9,      // Synthetic object, usualy used for grouping
                         // snapshot items together.
    kConsString = 10,    // Concatenated string. A pair of pointers to strings.
    kSlicedString = 11,  // Sliced string. A fragment of another string.
    kSymbol = 12         // A Symbol (ES6).
  };

  /** Returns node type (see HeapGraphNode::Type). */
  Type GetType() const;

  /**
   * Returns node name. Depending on node's type this can be the name
   * of the constructor (for objects), the name of the function (for
   * closures), string value, or an empty string (for compiled code).
   */
  Local<String> GetName() const;

  /**
   * Returns node id. For the same heap object, the id remains the same
   * across all snapshots.
   */
  SnapshotObjectId GetId() const;

  /** Returns node's own size, in bytes. */
  size_t GetShallowSize() const;

  /** Returns child nodes count of the node. */
  int GetChildrenCount() const;

  /** Retrieves a child by index. */
  const HeapGraphEdge* GetChild(int index) const;
};


/**
 * An interface for exporting data from V8, using "push" model.
 */
class V8_EXPORT OutputStream {  // NOLINT
 public:
  enum WriteResult {
    kContinue = 0,
    kAbort = 1
  };
  virtual ~OutputStream() {}
  /** Notify about the end of stream. */
  virtual void EndOfStream() = 0;
  /** Get preferred output chunk size. Called only once. */
  virtual int GetChunkSize() { return 1024; }
  /**
   * Writes the next chunk of snapshot data into the stream. Writing
   * can be stopped by returning kAbort as function result. EndOfStream
   * will not be called in case writing was aborted.
   */
  virtual WriteResult WriteAsciiChunk(char* data, int size) = 0;
  /**
   * Writes the next chunk of heap stats data into the stream. Writing
   * can be stopped by returning kAbort as function result. EndOfStream
   * will not be called in case writing was aborted.
   */
  virtual WriteResult WriteHeapStatsChunk(HeapStatsUpdate* data, int count) {
    return kAbort;
  }
};


/**
 * HeapSnapshots record the state of the JS heap at some moment.
 */
class V8_EXPORT HeapSnapshot {
 public:
  enum SerializationFormat {
    kJSON = 0  // See format description near 'Serialize' method.
  };

  /** Returns the root node of the heap graph. */
  const HeapGraphNode* GetRoot() const;

  /** Returns a node by its id. */
  const HeapGraphNode* GetNodeById(SnapshotObjectId id) const;

  /** Returns total nodes count in the snapshot. */
  int GetNodesCount() const;

  /** Returns a node by index. */
  const HeapGraphNode* GetNode(int index) const;

  /** Returns a max seen JS object Id. */
  SnapshotObjectId GetMaxSnapshotJSObjectId() const;

  /**
   * Deletes the snapshot and removes it from HeapProfiler's list.
   * All pointers to nodes, edges and paths previously returned become
   * invalid.
   */
  void Delete();

  /**
   * Prepare a serialized representation of the snapshot. The result
   * is written into the stream provided in chunks of specified size.
   * The total length of the serialized snapshot is unknown in
   * advance, it can be roughly equal to JS heap size (that means,
   * it can be really big - tens of megabytes).
   *
   * For the JSON format, heap contents are represented as an object
   * with the following structure:
   *
   *  {
   *    snapshot: {
   *      title: "...",
   *      uid: nnn,
   *      meta: { meta-info },
   *      node_count: nnn,
   *      edge_count: nnn
   *    },
   *    nodes: [nodes array],
   *    edges: [edges array],
   *    strings: [strings array]
   *  }
   *
   * Nodes reference strings, other nodes, and edges by their indexes
   * in corresponding arrays.
   */
  void Serialize(OutputStream* stream,
                 SerializationFormat format = kJSON) const;
};


/**
 * An interface for reporting progress and controlling long-running
 * activities.
 */
class V8_EXPORT ActivityControl {  // NOLINT
 public:
  enum ControlOption {
    kContinue = 0,
    kAbort = 1
  };
  virtual ~ActivityControl() {}
  /**
   * Notify about current progress. The activity can be stopped by
   * returning kAbort as the callback result.
   */
  virtual ControlOption ReportProgressValue(int done, int total) = 0;
};


/**
 * AllocationProfile is a sampled profile of allocations done by the program.
 * This is structured as a call-graph.
 */
class V8_EXPORT AllocationProfile {
 public:
  struct Allocation {
    /**
     * Size of the sampled allocation object.
     */
    size_t size;

    /**
     * The number of objects of such size that were sampled.
     */
    unsigned int count;
  };

  /**
   * Represents a node in the call-graph.
   */
  struct Node {
    /**
     * Name of the function. May be empty for anonymous functions or if the
     * script corresponding to this function has been unloaded.
     */
    Local<String> name;

    /**
     * Name of the script containing the function. May be empty if the script
     * name is not available, or if the script has been unloaded.
     */
    Local<String> script_name;

    /**
     * id of the script where the function is located. May be equal to
     * v8::UnboundScript::kNoScriptId in cases where the script doesn't exist.
     */
    int script_id;

    /**
     * Start position of the function in the script.
     */
    int start_position;

    /**
     * 1-indexed line number where the function starts. May be
     * kNoLineNumberInfo if no line number information is available.
     */
    int line_number;

    /**
     * 1-indexed column number where the function starts. May be
     * kNoColumnNumberInfo if no line number information is available.
     */
    int column_number;

    /**
     * List of callees called from this node for which we have sampled
     * allocations. The lifetime of the children is scoped to the containing
     * AllocationProfile.
     */
    std::vector<Node*> children;

    /**
     * List of self allocations done by this node in the call-graph.
     */
    std::vector<Allocation> allocations;
  };

  /**
   * Returns the root node of the call-graph. The root node corresponds to an
   * empty JS call-stack. The lifetime of the returned Node* is scoped to the
   * containing AllocationProfile.
   */
  virtual Node* GetRootNode() = 0;

  virtual ~AllocationProfile() {}

  static const int kNoLineNumberInfo = Message::kNoLineNumberInfo;
  static const int kNoColumnNumberInfo = Message::kNoColumnInfo;
};


/**
 * Interface for controlling heap profiling. Instance of the
 * profiler can be retrieved using v8::Isolate::GetHeapProfiler.
 */
class V8_EXPORT HeapProfiler {
 public:
  enum SamplingFlags {
    kSamplingNoFlags = 0,
    kSamplingForceGC = 1 << 0,
  };

  typedef std::unordered_set<const v8::PersistentBase<v8::Value>*>
      RetainerChildren;
  typedef std::vector<std::pair<v8::RetainedObjectInfo*, RetainerChildren>>
      RetainerGroups;
  typedef std::vector<std::pair<const v8::PersistentBase<v8::Value>*,
                                const v8::PersistentBase<v8::Value>*>>
      RetainerEdges;

  struct RetainerInfos {
    RetainerGroups groups;
    RetainerEdges edges;
  };

  /**
   * Callback function invoked to retrieve all RetainerInfos from the embedder.
   */
  typedef RetainerInfos (*GetRetainerInfosCallback)(v8::Isolate* isolate);

  /**
   * Callback function invoked for obtaining RetainedObjectInfo for
   * the given JavaScript wrapper object. It is prohibited to enter V8
   * while the callback is running: only getters on the handle and
   * GetPointerFromInternalField on the objects are allowed.
   */
  typedef RetainedObjectInfo* (*WrapperInfoCallback)(uint16_t class_id,
                                                     Local<Value> wrapper);

  /** Returns the number of snapshots taken. */
  int GetSnapshotCount();

  /** Returns a snapshot by index. */
  const HeapSnapshot* GetHeapSnapshot(int index);

  /**
   * Returns SnapshotObjectId for a heap object referenced by |value| if
   * it has been seen by the heap profiler, kUnknownObjectId otherwise.
   */
  SnapshotObjectId GetObjectId(Local<Value> value);

  /**
   * Returns heap object with given SnapshotObjectId if the object is alive,
   * otherwise empty handle is returned.
   */
  Local<Value> FindObjectById(SnapshotObjectId id);

  /**
   * Clears internal map from SnapshotObjectId to heap object. The new objects
   * will not be added into it unless a heap snapshot is taken or heap object
   * tracking is kicked off.
   */
  void ClearObjectIds();

  /**
   * A constant for invalid SnapshotObjectId. GetSnapshotObjectId will return
   * it in case heap profiler cannot find id  for the object passed as
   * parameter. HeapSnapshot::GetNodeById will always return NULL for such id.
   */
  static const SnapshotObjectId kUnknownObjectId = 0;

  /**
   * Callback interface for retrieving user friendly names of global objects.
   */
  class ObjectNameResolver {
   public:
    /**
     * Returns name to be used in the heap snapshot for given node. Returned
     * string must stay alive until snapshot collection is completed.
     */
    virtual const char* GetName(Local<Object> object) = 0;

   protected:
    virtual ~ObjectNameResolver() {}
  };

  /**
   * Takes a heap snapshot and returns it.
   */
  const HeapSnapshot* TakeHeapSnapshot(
      ActivityControl* control = NULL,
      ObjectNameResolver* global_object_name_resolver = NULL);

  /**
   * Starts tracking of heap objects population statistics. After calling
   * this method, all heap objects relocations done by the garbage collector
   * are being registered.
   *
   * |track_allocations| parameter controls whether stack trace of each
   * allocation in the heap will be recorded and reported as part of
   * HeapSnapshot.
   */
  void StartTrackingHeapObjects(bool track_allocations = false);

  /**
   * Adds a new time interval entry to the aggregated statistics array. The
   * time interval entry contains information on the current heap objects
   * population size. The method also updates aggregated statistics and
   * reports updates for all previous time intervals via the OutputStream
   * object. Updates on each time interval are provided as a stream of the
   * HeapStatsUpdate structure instances.
   * If |timestamp_us| is supplied, timestamp of the new entry will be written
   * into it. The return value of the function is the last seen heap object Id.
   *
   * StartTrackingHeapObjects must be called before the first call to this
   * method.
   */
  SnapshotObjectId GetHeapStats(OutputStream* stream,
                                int64_t* timestamp_us = NULL);

  /**
   * Stops tracking of heap objects population statistics, cleans up all
   * collected data. StartHeapObjectsTracking must be called again prior to
   * calling GetHeapStats next time.
   */
  void StopTrackingHeapObjects();

  /**
   * Starts gathering a sampling heap profile. A sampling heap profile is
   * similar to tcmalloc's heap profiler and Go's mprof. It samples object
   * allocations and builds an online 'sampling' heap profile. At any point in
   * time, this profile is expected to be a representative sample of objects
   * currently live in the system. Each sampled allocation includes the stack
   * trace at the time of allocation, which makes this really useful for memory
   * leak detection.
   *
   * This mechanism is intended to be cheap enough that it can be used in
   * production with minimal performance overhead.
   *
   * Allocations are sampled using a randomized Poisson process. On average, one
   * allocation will be sampled every |sample_interval| bytes allocated. The
   * |stack_depth| parameter controls the maximum number of stack frames to be
   * captured on each allocation.
   *
   * NOTE: This is a proof-of-concept at this point. Right now we only sample
   * newspace allocations. Support for paged space allocation (e.g. pre-tenured
   * objects, large objects, code objects, etc.) and native allocations
   * doesn't exist yet, but is anticipated in the future.
   *
   * Objects allocated before the sampling is started will not be included in
   * the profile.
   *
   * Returns false if a sampling heap profiler is already running.
   */
  bool StartSamplingHeapProfiler(uint64_t sample_interval = 512 * 1024,
                                 int stack_depth = 16,
                                 SamplingFlags flags = kSamplingNoFlags);

  /**
   * Stops the sampling heap profile and discards the current profile.
   */
  void StopSamplingHeapProfiler();

  /**
   * Returns the sampled profile of allocations allocated (and still live) since
   * StartSamplingHeapProfiler was called. The ownership of the pointer is
   * transfered to the caller. Returns nullptr if sampling heap profiler is not
   * active.
   */
  AllocationProfile* GetAllocationProfile();

  /**
   * Deletes all snapshots taken. All previously returned pointers to
   * snapshots and their contents become invalid after this call.
   */
  void DeleteAllHeapSnapshots();

  /** Binds a callback to embedder's class ID. */
  void SetWrapperClassInfoProvider(
      uint16_t class_id,
      WrapperInfoCallback callback);

  void SetGetRetainerInfosCallback(GetRetainerInfosCallback callback);

  /**
   * Default value of persistent handle class ID. Must not be used to
   * define a class. Can be used to reset a class of a persistent
   * handle.
   */
  static const uint16_t kPersistentHandleNoClassId = 0;

  /** Returns memory used for profiler internal data and snapshots. */
  size_t GetProfilerMemorySize();

  /**
   * Sets a RetainedObjectInfo for an object group (see V8::SetObjectGroupId).
   */
  void SetRetainedObjectInfo(UniqueId id, RetainedObjectInfo* info);

 private:
  HeapProfiler();
  ~HeapProfiler();
  HeapProfiler(const HeapProfiler&);
  HeapProfiler& operator=(const HeapProfiler&);
};

/**
 * Interface for providing information about embedder's objects
 * held by global handles. This information is reported in two ways:
 *
 *  1. When calling AddObjectGroup, an embedder may pass
 *     RetainedObjectInfo instance describing the group.  To collect
 *     this information while taking a heap snapshot, V8 calls GC
 *     prologue and epilogue callbacks.
 *
 *  2. When a heap snapshot is collected, V8 additionally
 *     requests RetainedObjectInfos for persistent handles that
 *     were not previously reported via AddObjectGroup.
 *
 * Thus, if an embedder wants to provide information about native
 * objects for heap snapshots, it can do it in a GC prologue
 * handler, and / or by assigning wrapper class ids in the following way:
 *
 *  1. Bind a callback to class id by calling SetWrapperClassInfoProvider.
 *  2. Call SetWrapperClassId on certain persistent handles.
 *
 * V8 takes ownership of RetainedObjectInfo instances passed to it and
 * keeps them alive only during snapshot collection. Afterwards, they
 * are freed by calling the Dispose class function.
 */
class V8_EXPORT RetainedObjectInfo {  // NOLINT
 public:
  /** Called by V8 when it no longer needs an instance. */
  virtual void Dispose() = 0;

  /** Returns whether two instances are equivalent. */
  virtual bool IsEquivalent(RetainedObjectInfo* other) = 0;

  /**
   * Returns hash value for the instance. Equivalent instances
   * must have the same hash value.
   */
  virtual intptr_t GetHash() = 0;

  /**
   * Returns human-readable label. It must be a null-terminated UTF-8
   * encoded string. V8 copies its contents during a call to GetLabel.
   */
  virtual const char* GetLabel() = 0;

  /**
   * Returns human-readable group label. It must be a null-terminated UTF-8
   * encoded string. V8 copies its contents during a call to GetGroupLabel.
   * Heap snapshot generator will collect all the group names, create
   * top level entries with these names and attach the objects to the
   * corresponding top level group objects. There is a default
   * implementation which is required because embedders don't have their
   * own implementation yet.
   */
  virtual const char* GetGroupLabel() { return GetLabel(); }

  /**
   * Returns element count in case if a global handle retains
   * a subgraph by holding one of its nodes.
   */
  virtual intptr_t GetElementCount() { return -1; }

  /** Returns embedder's object size in bytes. */
  virtual intptr_t GetSizeInBytes() { return -1; }

 protected:
  RetainedObjectInfo() {}
  virtual ~RetainedObjectInfo() {}

 private:
  RetainedObjectInfo(const RetainedObjectInfo&);
  RetainedObjectInfo& operator=(const RetainedObjectInfo&);
};


/**
 * A struct for exporting HeapStats data from V8, using "push" model.
 * See HeapProfiler::GetHeapStats.
 */
struct HeapStatsUpdate {
  HeapStatsUpdate(uint32_t index, uint32_t count, uint32_t size)
    : index(index), count(count), size(size) { }
  uint32_t index;  // Index of the time interval that was changed.
  uint32_t count;  // New value of count field for the interval with this index.
  uint32_t size;  // New value of size field for the interval with this index.
};


}  // namespace v8


#endif  // V8_V8_PROFILER_H_
