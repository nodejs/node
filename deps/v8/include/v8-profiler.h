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

#ifndef V8_V8_PROFILER_H_
#define V8_V8_PROFILER_H_

#include "v8.h"

#ifdef _WIN32
// Setup for Windows DLL export/import. See v8.h in this directory for
// information on how to build/use V8 as a DLL.
#if defined(BUILDING_V8_SHARED) && defined(USING_V8_SHARED)
#error both BUILDING_V8_SHARED and USING_V8_SHARED are set - please check the\
  build configuration to ensure that at most one of these is set
#endif

#ifdef BUILDING_V8_SHARED
#define V8EXPORT __declspec(dllexport)
#elif USING_V8_SHARED
#define V8EXPORT __declspec(dllimport)
#else
#define V8EXPORT
#endif

#else  // _WIN32

// Setup for Linux shared library export. See v8.h in this directory for
// information on how to build/use V8 as shared library.
#if defined(__GNUC__) && (__GNUC__ >= 4) && defined(V8_SHARED)
#define V8EXPORT __attribute__ ((visibility("default")))
#else  // defined(__GNUC__) && (__GNUC__ >= 4)
#define V8EXPORT
#endif  // defined(__GNUC__) && (__GNUC__ >= 4)

#endif  // _WIN32


/**
 * Profiler support for the V8 JavaScript engine.
 */
namespace v8 {

typedef uint32_t SnapshotObjectId;

/**
 * CpuProfileNode represents a node in a call graph.
 */
class V8EXPORT CpuProfileNode {
 public:
  /** Returns function name (empty string for anonymous functions.) */
  Handle<String> GetFunctionName() const;

  /** Returns resource name for script from where the function originates. */
  Handle<String> GetScriptResourceName() const;

  /**
   * Returns the number, 1-based, of the line where the function originates.
   * kNoLineNumberInfo if no line number information is available.
   */
  int GetLineNumber() const;

  /**
   * Returns total (self + children) execution time of the function,
   * in milliseconds, estimated by samples count.
   */
  double GetTotalTime() const;

  /**
   * Returns self execution time of the function, in milliseconds,
   * estimated by samples count.
   */
  double GetSelfTime() const;

  /** Returns the count of samples where function exists. */
  double GetTotalSamplesCount() const;

  /** Returns the count of samples where function was currently executing. */
  double GetSelfSamplesCount() const;

  /** Returns function entry UID. */
  unsigned GetCallUid() const;

  /** Returns child nodes count of the node. */
  int GetChildrenCount() const;

  /** Retrieves a child node by index. */
  const CpuProfileNode* GetChild(int index) const;

  static const int kNoLineNumberInfo = Message::kNoLineNumberInfo;
};


/**
 * CpuProfile contains a CPU profile in a form of two call trees:
 *  - top-down (from main() down to functions that do all the work);
 *  - bottom-up call graph (in backward direction).
 */
class V8EXPORT CpuProfile {
 public:
  /** Returns CPU profile UID (assigned by the profiler.) */
  unsigned GetUid() const;

  /** Returns CPU profile title. */
  Handle<String> GetTitle() const;

  /** Returns the root node of the bottom up call tree. */
  const CpuProfileNode* GetBottomUpRoot() const;

  /** Returns the root node of the top down call tree. */
  const CpuProfileNode* GetTopDownRoot() const;

  /**
   * Deletes the profile and removes it from CpuProfiler's list.
   * All pointers to nodes previously returned become invalid.
   * Profiles with the same uid but obtained using different
   * security token are not deleted, but become inaccessible
   * using FindProfile method. It is embedder's responsibility
   * to call Delete on these profiles.
   */
  void Delete();
};


/**
 * Interface for controlling CPU profiling.
 */
class V8EXPORT CpuProfiler {
 public:
  /**
   * A note on security tokens usage. As scripts from different
   * origins can run inside a single V8 instance, it is possible to
   * have functions from different security contexts intermixed in a
   * single CPU profile. To avoid exposing function names belonging to
   * other contexts, filtering by security token is performed while
   * obtaining profiling results.
   */

  /**
   * Returns the number of profiles collected (doesn't include
   * profiles that are being collected at the moment of call.)
   */
  static int GetProfilesCount();

  /** Returns a profile by index. */
  static const CpuProfile* GetProfile(
      int index,
      Handle<Value> security_token = Handle<Value>());

  /** Returns a profile by uid. */
  static const CpuProfile* FindProfile(
      unsigned uid,
      Handle<Value> security_token = Handle<Value>());

  /**
   * Starts collecting CPU profile. Title may be an empty string. It
   * is allowed to have several profiles being collected at
   * once. Attempts to start collecting several profiles with the same
   * title are silently ignored. While collecting a profile, functions
   * from all security contexts are included in it. The token-based
   * filtering is only performed when querying for a profile.
   */
  static void StartProfiling(Handle<String> title);

  /**
   * Stops collecting CPU profile with a given title and returns it.
   * If the title given is empty, finishes the last profile started.
   */
  static const CpuProfile* StopProfiling(
      Handle<String> title,
      Handle<Value> security_token = Handle<Value>());

  /**
   * Deletes all existing profiles, also cancelling all profiling
   * activity.  All previously returned pointers to profiles and their
   * contents become invalid after this call.
   */
  static void DeleteAllProfiles();
};


class HeapGraphNode;


/**
 * HeapSnapshotEdge represents a directed connection between heap
 * graph nodes: from retainers to retained nodes.
 */
class V8EXPORT HeapGraphEdge {
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
  Handle<Value> GetName() const;

  /** Returns origin node. */
  const HeapGraphNode* GetFromNode() const;

  /** Returns destination node. */
  const HeapGraphNode* GetToNode() const;
};


/**
 * HeapGraphNode represents a node in a heap graph.
 */
class V8EXPORT HeapGraphNode {
 public:
  enum Type {
    kHidden = 0,      // Hidden node, may be filtered when shown to user.
    kArray = 1,       // An array of elements.
    kString = 2,      // A string.
    kObject = 3,      // A JS object (except for arrays and strings).
    kCode = 4,        // Compiled code.
    kClosure = 5,     // Function closure.
    kRegExp = 6,      // RegExp.
    kHeapNumber = 7,  // Number stored in the heap.
    kNative = 8,      // Native object (not from V8 heap).
    kSynthetic = 9    // Synthetic object, usualy used for grouping
                      // snapshot items together.
  };

  /** Returns node type (see HeapGraphNode::Type). */
  Type GetType() const;

  /**
   * Returns node name. Depending on node's type this can be the name
   * of the constructor (for objects), the name of the function (for
   * closures), string value, or an empty string (for compiled code).
   */
  Handle<String> GetName() const;

  /**
   * Returns node id. For the same heap object, the id remains the same
   * across all snapshots.
   */
  SnapshotObjectId GetId() const;

  /** Returns node's own size, in bytes. */
  int GetSelfSize() const;

  /**
   * Returns node's retained size, in bytes. That is, self + sizes of
   * the objects that are reachable only from this object. In other
   * words, the size of memory that will be reclaimed having this node
   * collected.
   */
  int GetRetainedSize() const;

  /** Returns child nodes count of the node. */
  int GetChildrenCount() const;

  /** Retrieves a child by index. */
  const HeapGraphEdge* GetChild(int index) const;

  /** Returns retainer nodes count of the node. */
  int GetRetainersCount() const;

  /** Returns a retainer by index. */
  const HeapGraphEdge* GetRetainer(int index) const;

  /**
   * Returns a dominator node. This is the node that participates in every
   * path from the snapshot root to the current node.
   */
  const HeapGraphNode* GetDominatorNode() const;

  /**
   * Finds and returns a value from the heap corresponding to this node,
   * if the value is still reachable.
   */
  Handle<Value> GetHeapValue() const;
};


/**
 * HeapSnapshots record the state of the JS heap at some moment.
 */
class V8EXPORT HeapSnapshot {
 public:
  enum Type {
    kFull = 0  // Heap snapshot with all instances and references.
  };
  enum SerializationFormat {
    kJSON = 0  // See format description near 'Serialize' method.
  };

  /** Returns heap snapshot type. */
  Type GetType() const;

  /** Returns heap snapshot UID (assigned by the profiler.) */
  unsigned GetUid() const;

  /** Returns heap snapshot title. */
  Handle<String> GetTitle() const;

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
  void Serialize(OutputStream* stream, SerializationFormat format) const;
};


class RetainedObjectInfo;

/**
 * Interface for controlling heap profiling.
 */
class V8EXPORT HeapProfiler {
 public:
  /**
   * Callback function invoked for obtaining RetainedObjectInfo for
   * the given JavaScript wrapper object. It is prohibited to enter V8
   * while the callback is running: only getters on the handle and
   * GetPointerFromInternalField on the objects are allowed.
   */
  typedef RetainedObjectInfo* (*WrapperInfoCallback)
      (uint16_t class_id, Handle<Value> wrapper);

  /** Returns the number of snapshots taken. */
  static int GetSnapshotsCount();

  /** Returns a snapshot by index. */
  static const HeapSnapshot* GetSnapshot(int index);

  /** Returns a profile by uid. */
  static const HeapSnapshot* FindSnapshot(unsigned uid);

  /**
   * Returns SnapshotObjectId for a heap object referenced by |value| if
   * it has been seen by the heap profiler, kUnknownObjectId otherwise.
   */
  static SnapshotObjectId GetSnapshotObjectId(Handle<Value> value);

  /**
   * A constant for invalid SnapshotObjectId. GetSnapshotObjectId will return
   * it in case heap profiler cannot find id  for the object passed as
   * parameter. HeapSnapshot::GetNodeById will always return NULL for such id.
   */
  static const SnapshotObjectId kUnknownObjectId = 0;

  /**
   * Takes a heap snapshot and returns it. Title may be an empty string.
   * See HeapSnapshot::Type for types description.
   */
  static const HeapSnapshot* TakeSnapshot(
      Handle<String> title,
      HeapSnapshot::Type type = HeapSnapshot::kFull,
      ActivityControl* control = NULL);

  /**
   * Starts tracking of heap objects population statistics. After calling
   * this method, all heap objects relocations done by the garbage collector
   * are being registered.
   */
  static void StartHeapObjectsTracking();

  /**
   * Adds a new time interval entry to the aggregated statistics array. The
   * time interval entry contains information on the current heap objects
   * population size. The method also updates aggregated statistics and
   * reports updates for all previous time intervals via the OutputStream
   * object. Updates on each time interval are provided as a stream of the
   * HeapStatsUpdate structure instances.
   *
   * StartHeapObjectsTracking must be called before the first call to this
   * method.
   */
  static void PushHeapObjectsStats(OutputStream* stream);

  /**
   * Stops tracking of heap objects population statistics, cleans up all
   * collected data. StartHeapObjectsTracking must be called again prior to
   * calling PushHeapObjectsStats next time.
   */
  static void StopHeapObjectsTracking();

  /**
   * Deletes all snapshots taken. All previously returned pointers to
   * snapshots and their contents become invalid after this call.
   */
  static void DeleteAllSnapshots();

  /** Binds a callback to embedder's class ID. */
  static void DefineWrapperClass(
      uint16_t class_id,
      WrapperInfoCallback callback);

  /**
   * Default value of persistent handle class ID. Must not be used to
   * define a class. Can be used to reset a class of a persistent
   * handle.
   */
  static const uint16_t kPersistentHandleNoClassId = 0;

  /** Returns the number of currently existing persistent handles. */
  static int GetPersistentHandleCount();
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
 * objects for heap snapshots, he can do it in a GC prologue
 * handler, and / or by assigning wrapper class ids in the following way:
 *
 *  1. Bind a callback to class id by calling DefineWrapperClass.
 *  2. Call SetWrapperClassId on certain persistent handles.
 *
 * V8 takes ownership of RetainedObjectInfo instances passed to it and
 * keeps them alive only during snapshot collection. Afterwards, they
 * are freed by calling the Dispose class function.
 */
class V8EXPORT RetainedObjectInfo {  // NOLINT
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
 * See HeapProfiler::PushHeapObjectsStats.
 */
struct HeapStatsUpdate {
  HeapStatsUpdate(uint32_t index, uint32_t count, uint32_t size)
    : index(index), count(count), size(size) { }
  uint32_t index;  // Index of the time interval that was changed.
  uint32_t count;  // New value of count field for the interval with this index.
  uint32_t size;  // New value of size field for the interval with this index.
};


}  // namespace v8


#undef V8EXPORT


#endif  // V8_V8_PROFILER_H_
